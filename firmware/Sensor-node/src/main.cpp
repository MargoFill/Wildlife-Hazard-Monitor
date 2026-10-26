#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MLX90640.h>
#include <HardwareSerial.h>
#include <time.h>
#include <esp_sleep.h>

#include "thermal_model.h"

constexpr gpio_num_t PIR_PIN = GPIO_NUM_5;
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

bool predictMoose(const float *rawFrame) {
  static float input[24][32];
  for (int row = 0; row < 24; ++row) {
    for (int col = 0; col < 32; ++col) input[row][col] = (rawFrame[row * 32 + col] - 18.0f) / 10.0f;
  }

  static float conv1Out[8][12][16];
  for (int filter = 0; filter < 8; ++filter) {
    for (int row = 0; row < 12; ++row) {
      for (int col = 0; col < 16; ++col) {
        float sum = conv1_bias[filter];
        for (int kr = 0; kr < 3; ++kr) {
          for (int kc = 0; kc < 3; ++kc) {
            const int inputRow = row * 2 + kr - 1;
            const int inputCol = col * 2 + kc - 1;
            if (inputRow >= 0 && inputRow < 24 && inputCol >= 0 && inputCol < 32) sum += input[inputRow][inputCol] * conv1_weight[filter * 9 + kr * 3 + kc];
          }
        }
        conv1Out[filter][row][col] = max(0.0f, sum);
      }
    }
  }

  static float features[768];
  int featureIndex = 0;
  for (int filter = 0; filter < 16; ++filter) {
    for (int row = 0; row < 6; ++row) {
      for (int col = 0; col < 8; ++col) {
        float sum = conv2_bias[filter];
        for (int inputFilter = 0; inputFilter < 8; ++inputFilter) {
          for (int kr = 0; kr < 3; ++kr) {
            for (int kc = 0; kc < 3; ++kc) {
              const int inputRow = row * 2 + kr - 1;
              const int inputCol = col * 2 + kc - 1;
              if (inputRow >= 0 && inputRow < 12 && inputCol >= 0 && inputCol < 16) {
                const int weightIndex = filter * (8 * 9) + inputFilter * 9 + kr * 3 + kc;
                sum += conv1Out[inputFilter][inputRow][inputCol] * conv2_weight[weightIndex];
              }
            }
          }
        }
        features[featureIndex++] = max(0.0f, sum);
      }
    }
  }

  static float hidden[16];
  for (int neuron = 0; neuron < 16; ++neuron) {
    float sum = fc1_bias[neuron];
    for (int feature = 0; feature < 768; ++feature) sum += features[feature] * fc1_weight[neuron * 768 + feature];
    hidden[neuron] = max(0.0f, sum);
  }

  float logits[2] = {fc2_bias[0], fc2_bias[1]};
  for (int neuron = 0; neuron < 16; ++neuron) {
    logits[0] += hidden[neuron] * fc2_weight[neuron];
    logits[1] += hidden[neuron] * fc2_weight[16 + neuron];
  }
  return logits[1] > logits[0];
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
  return true;
}

bool detectMoose() {
  if (!initializeThermalCamera()) return false;
  int votes = 0;
  int validFrames = 0;
  for (int i = 0; i < FRAME_COUNT; ++i) {
    if (mlx.getFrame(frame) != 0) {
      Serial.println("[CAMERA] Frame read failed");
      continue;
    }
    ++validFrames;
    if (predictMoose(frame)) ++votes;
    delay(30);
  }
  Serial.printf("[MODEL] %d/%d Moose votes\n", votes, validFrames);
  return validFrames > 0 && votes >= REQUIRED_VOTES;
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
//TODO sensor numbering logic 
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

  pinMode(PIR_PIN, INPUT);

  const esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
  if (wakeCause == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("[WAKE] PIR motion detected");
    const bool mooseDetected = detectMoose();
    if (mooseDetected) sendMooseFinding();
    else Serial.println("[MODEL] No Moose finding");
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
