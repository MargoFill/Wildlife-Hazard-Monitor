#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MLX90640.h>
#include <HardwareSerial.h>
#include <time.h>
#include <esp_sleep.h>
#include "animal_prediction.h"

#include "thermal_model.h"

constexpr gpio_num_t PIR_PIN = GPIO_NUM_5;
constexpr int LED_PIN = 40;
constexpr int I2C_SDA_PIN = 8;
constexpr int I2C_SCL_PIN = 9;
constexpr int LORA_RX_PIN = 3;
constexpr int LORA_TX_PIN = 2;
constexpr int LORA_BAUD = 9600;
constexpr uint8_t LORA_DESTINATION = 1;

constexpr int FRAME_COUNT = 8;
constexpr int REQUIRED_VOTES = 5;
constexpr unsigned long PIR_CALIBRATION_MS = 10000;

Adafruit_MLX90640 mlx;
HardwareSerial LoRaSerial(1);
float frame[32 * 24];


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

bool detectMoose() {
  if (!initializeThermalCamera()) return false;
  int votes = 0;
  int validFrames = 0;
  
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
    }
    // Print the final verdict for this frame (1 or 0)
    Serial.println(raw_detection ? 1 : 0);
    
    delay(220);
  }
  
  Serial.printf("[MODEL] %d/%d Moose votes\n", votes, validFrames);
  return (validFrames >= REQUIRED_VOTES) && (votes >= REQUIRED_VOTES);
}

String timestamp() {
  const time_t now = time(nullptr);
  if (now >= 1700000000) return String(static_cast<unsigned long>(now));
  return String("uptime_ms=") + String(static_cast<unsigned long>(millis()));
}

void initializeLoRa() {
  LoRaSerial.begin(LORA_BAUD, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN);
  delay(100);
  LoRaSerial.println("AT+BAND=868500000");
  LoRaSerial.println("AT+PARAMETER=10,7,2,6");
  LoRaSerial.println("AT+CRFOP=14");
  delay(300);
}

void sendMooseFinding() {
  initializeLoRa();
  const String payload = String("WILDLIFE,moose,node=sensor-1,ts=") + timestamp();
  const String command = String("AT+SEND=") + String(LORA_DESTINATION) + "," + String(payload.length()) + "," + payload;
  LoRaSerial.println(command);
  LoRaSerial.flush();
  Serial.println("[LORA] " + payload);
}

[[noreturn]] void enterDeepSleep() {
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
  pinMode(LED_PIN, OUTPUT);
  pinMode(PIR_PIN, INPUT);

  const esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
  if (wakeCause == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("[WAKE] PIR motion detected");
    digitalWrite(LED_PIN, HIGH);
    const bool mooseDetected = detectMoose();
    digitalWrite(LED_PIN, LOW);
    if (mooseDetected) sendMooseFinding();
    else Serial.println("[MODEL] No Moose finding");
  } else {
    Serial.println("[BOOT] PIR calibration");
    const unsigned long start = millis();
    while (millis() - start < PIR_CALIBRATION_MS) {
      digitalWrite(LED_PIN, ((millis() / 250) % 2) == 0);
      delay(20);
    }
    digitalWrite(LED_PIN, LOW);
  }
  enterDeepSleep();
}

void loop() {}
