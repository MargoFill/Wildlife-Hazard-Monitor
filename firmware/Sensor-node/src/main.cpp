  #include <Arduino.h>
  #include <Wire.h>
  #include <Adafruit_MLX90640.h>
  #include <HardwareSerial.h>
  #include <time.h>
  #include <esp_sleep.h>
  #include "animal_prediction.h"
  #include <SPI.h> //library for SD card connection
  #include <SD.h> //library for files usage

  //#include "thermal_model.h"
  #include "thermal_modelV2.h"
  
  constexpr gpio_num_t PIR_PIN = GPIO_NUM_5;
  constexpr int I2C_SDA_PIN = 8;
  constexpr int I2C_SCL_PIN = 9;
  constexpr int LORA_RX_PIN = 17;
  constexpr int LORA_TX_PIN = 16;
  constexpr int SD_CARD_PIN_MOSI=10;//MOSI
  constexpr int SD_CARD_PIN_CS=11; //CS
  constexpr int SD_CARD_PIN_SCK=12; //SCK
  constexpr int SD_CARD_PIN_MISO=13; //MISO 

  
  constexpr int LORA_BAUD = 115200; 
  constexpr uint8_t LORA_DESTINATION = 1;

  constexpr int FRAME_COUNT = 8;
  constexpr int REQUIRED_VOTES = 5;
  constexpr unsigned long PIR_CALIBRATION_MS = 10000;

  Adafruit_MLX90640 mlx;
  HardwareSerial LoRaSerial(1);
  float frame[32 * 24];
  bool sdReady=false; // if sd card is not found, sd block is not executed

  String timestamp() {
    const time_t now = time(nullptr);
    if (now >= 1700000000) return String(static_cast<unsigned long>(now));
    return String("uptime_ms=") + String(static_cast<unsigned long>(millis()));
  }


  // Initializing SD card and creating log header
  bool initSD() {
    SPI.begin(SD_CARD_PIN_SCK, SD_CARD_PIN_MISO, SD_CARD_PIN_MOSI, SD_CARD_PIN_CS);
    if (!SD.begin(SD_CARD_PIN_CS)) {
      Serial.println("[SD] Card mount failed!");
      return false;
    }
    Serial.println("[SD] Card initialized");

    // Create an event table header if the file does not already exist
    if (!SD.exists("/detections.csv")) {
      File f = SD.open("/detections.csv", FILE_WRITE);// creates file
      if (f) {
        f.println("timestamp,animal_detected,detected_frames,total_frames");
        f.close(); // Closes the file and forces data to be flushed from the ESP32's RAM to the SD card's flash memory.
      }
    }
    return true;
  }
  
  void logDetection(bool detected, int detectedFrames, int totalFrames) {
    if (!sdReady) return;
    File f = SD.open("/detections.csv", FILE_APPEND);// opens file after the last written data, in the end
    if (f) {
      f.print(timestamp());
      f.print(',');
      f.print(detected ? 1 : 0);
      f.print(",");
      f.print(detectedFrames);
      f.print(',');
      f.println(totalFrames);
      f.close(); // close the file right after writing mew data, so it saves to the flash memory immediately
    } else {
      Serial.println("[SD] Failed to write /detections.csv");
    }
  }
  
  bool initializeThermalCamera() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(400000);

    if (!mlx.begin(MLX90640_I2CADDR_DEFAULT, &Wire)) {
      Serial.println("[CAMERA] MLX90640 not found");
      return false;
    }
    mlx.setMode(MLX90640_CHESS);
    mlx.setResolution(MLX90640_ADC_18BIT);
    mlx.setRefreshRate(MLX90640_4_HZ);
    for (int skip = 0; skip < 2; skip++) {
      mlx.getFrame(frame);
      delay(200);
    }

    return true;
  }

  bool detectAnimal() {
    if (!initializeThermalCamera()) return false;
    int votes = 0;
    int validFrames = 0;

    uint32_t frame_start = millis();

    // Open the file once in 8 frames
    File frameFile;
    if (sdReady) {
      frameFile = SD.open("/frames.csv", FILE_APPEND);
      if (!frameFile) {
        Serial.println("[SD] Failed to open /frames.csv");
      }
    }

    for (int i = 0; i < FRAME_COUNT; ++i) {
      if (mlx.getFrame(frame) != 0) {
        Serial.println("[CAMERA] Frame read failed");
        delay(50);
        continue;
      }
      ++validFrames;

      // --- NEW THRESHOLD CHECK ---
      // Find the hottest and coldest pixels
      float t_min = 100.0f, t_max = -100.0f;
      for (int p = 0; p < 768; p++) {
        if (frame[p] > t_max) t_max = frame[p];
        if (frame[p] < t_min) t_min = frame[p];
      }

      float delta = t_max - t_min;
      // PRINT THE MATH SO WE CAN SEE IT IN THE CONSOLE
      Serial.printf("[DEBUG] Min: %.1fC | Max: %.1fC | Delta: %.1fC\n", t_min, t_max, delta);

      bool raw_detection = false;

      // Threshold check
      // if hottest pixel is less than 4.0 deg hotter than coldest, will skip complex logic
      if (delta < 4.0f) {
        Serial.println("[MODEL] Skipped - Flat background temperature");
      } else {
        raw_detection = predict_animal(frame);
      }
      // ---------------------------

      if (raw_detection) {
        votes++;
      }

      // Print the 768 pixels for visualizer.py
      for (int p = 0; p < 768; p++) {
        Serial.print(frame[p], 1);
        Serial.print(',');
        if (frameFile) {
          frameFile.print(frame[p], 1);
          frameFile.print(',');
        }
      }
      // Print the final verdict for this frame (1 or 0)
      int verdict = raw_detection ? 1 : 0;
      Serial.println(verdict);
      if (frameFile) {
        frameFile.println(verdict);
        frameFile.flush(); // forces the buffer to be flushed from the ESP32's RAM to the card after every frame
      }
    }
    if (frameFile) {
      frameFile.close(); // closes the file and writes the final result in events.csv
    }

    Serial.printf("[MODEL] %d/%d Animal votes\n", votes, validFrames);
    //return (validFrames >= REQUIRED_VOTES) && (votes >= REQUIRED_VOTES);
    bool detected = (validFrames >= REQUIRED_VOTES) && (votes >= REQUIRED_VOTES);
    // Save detection verdict and frame vote count to /detections.csv
    logDetection(detected, votes, validFrames);
    return detected;
  }


  void initializeLoRa() {
    LoRaSerial.begin(LORA_BAUD, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN);
    delay(100);
    LoRaSerial.println("AT+ADDRESS=5");
    delay(100);
    LoRaSerial.println("AT+NETWORKID=11");
    delay(100);
    LoRaSerial.println("AT+BAND=868500000");
    delay(100);
    LoRaSerial.println("AT+PARAMETER=10,7,2,6");
    delay(100);
    LoRaSerial.println("AT+CRFOP=14");
    delay(300);
  }

  void sendAnimalFinding() {
    initializeLoRa();
    const String payload = String("WILDLIFE,animal,node=sensor-1,ts=") + timestamp();
    const String command = String("AT+SEND=") + String(LORA_DESTINATION) + "," + String(payload.length()) + "," + payload;
    LoRaSerial.println(command);
    LoRaSerial.flush();
    Serial.println("[LORA] " + payload);

    unsigned long startWait = millis();
    while (millis() - startWait < 3000) {
      if (LoRaSerial.available()) {
        String response = LoRaSerial.readStringUntil('\n');
        Serial.println("[LORA RESPONSE] " + response);
        if (response.indexOf("+OK") >= 0 || response.indexOf("ERR") >= 0) {
          break; // Transmission finished or failed
        }
      }
      delay(10);
    }
  }

  [[noreturn]] void enterDeepSleep() {
    // filoe system security before going deep fleep mode
    if (sdReady) {
      SD.end();
      SPI.end();
    }
    while (digitalRead(PIR_PIN) == HIGH) delay(100);
    esp_sleep_enable_ext0_wakeup(PIR_PIN, 1);
    Serial.println("[SLEEP] Waiting for PIR motion");
    Serial.flush();
    delay(20);
    esp_deep_sleep_start();
    while (true) delay(1000);
  }

  void setup() {
    Serial.begin(115200);
    delay(300);
    pinMode(PIR_PIN, INPUT);

    sdReady = initSD(); // Initialize SD card on every boot/wake

    const esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
    if (wakeCause == ESP_SLEEP_WAKEUP_EXT0) {
      Serial.println("[WAKE] PIR motion detected");
      const bool animalDetected = detectAnimal();
      if (animalDetected) sendAnimalFinding();
      else Serial.println("[MODEL] No Animal finding");
    } else {
      Serial.println("[BOOT] PIR calibration");
      const unsigned long start = millis();
      while (millis() - start < PIR_CALIBRATION_MS) {
        delay(20);
      }
    }
    enterDeepSleep();
  }

  void loop() {} 