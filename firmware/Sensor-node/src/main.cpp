#include <Arduino.h>
#include <math.h>
#include <Wire.h>
#include <Adafruit_MLX90640.h>
#include <HardwareSerial.h>
#include <time.h>
#include <esp_sleep.h>
#include "animal_prediction.h"

//#include "thermal_model.h"
#include "thermal_modelV2.h"

constexpr gpio_num_t PIR_PIN = GPIO_NUM_5;
constexpr int I2C_SDA_PIN = 8;
constexpr int I2C_SCL_PIN = 9;
constexpr int LORA_RX_PIN = 17;
constexpr int LORA_TX_PIN = 16;

constexpr int LORA_BAUD = 115200;
constexpr uint8_t LORA_DESTINATION = 1;

// Payload budget: RYLR limit is 240 chars for the AT+SEND payload field.
// Prefix "FRAME_DATA,<ts>,<chunk>," costs up to ~32 chars, leaving 208 for Base64.
// 208 Base64 chars encode 156 raw bytes; 150 gives a safe margin.
// 768 / 150 = 6 chunks (last chunk = 18 pixels = 24 Base64 chars).
constexpr int FRAME_CHUNK_PIXELS = 150;

constexpr int FRAME_COUNT = 8;
constexpr int REQUIRED_VOTES = 5;
constexpr unsigned long PIR_CALIBRATION_MS = 10000;
constexpr unsigned long WILDLIFE_HEARTBEAT_MS = 30000;

Adafruit_MLX90640 mlx;
HardwareSerial LoRaSerial(1);
[[noreturn]] void enterDeepSleep();
bool initializeLoRa();
bool sendStatus(bool wildlife);
bool fastAlertSent = false;

String timestamp() {
  const time_t now = time(nullptr);
  if (now >= 1700000000) return String(static_cast<unsigned long>(now));
  return String("uptime_ms=") + String(static_cast<unsigned long>(millis()));
}

float lastMin = 0.0f, lastMax = 0.0f, lastDelta = 0.0f;

bool initializeThermalCamera(float* warmupFrame) {
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
    mlx.getFrame(warmupFrame);
    delay(200);
  }
  return true;
}

// Returns a pointer to the static deciding-vote frame on detection, nullptr otherwise.
// The static buffers live inside this function (BSS), keeping them off the global namespace.
const float* detectAnimal() {
  static float frame[768];         // rolling capture buffer — reused every call
  static float decidingFrame[768]; // snapshot of the frame that cast the deciding vote

  if (!initializeThermalCamera(frame)) return nullptr;
  int votes = 0;
  int validFrames = 0;

  for (int i = 0; i < FRAME_COUNT; ++i) {
    if (mlx.getFrame(frame) != 0) {
      Serial.println("[CAMERA] Frame read failed");
      delay(50);
      continue;
    }
    ++validFrames;

    // Find the hottest and coldest pixels
    float t_min = 100.0f, t_max = -100.0f;
    for (int p = 0; p < 768; p++) {
      if (frame[p] > t_max) t_max = frame[p];
      if (frame[p] < t_min) t_min = frame[p];
    }
    float delta = t_max - t_min;
    lastMin = t_min;
    lastMax = t_max;
    lastDelta = delta;
    Serial.printf("[DEBUG] Min: %.1fC | Max: %.1fC | Delta: %.1fC\n", t_min, t_max, delta);

    bool raw_detection = false;

    // If hottest pixel is less than 4.0 deg hotter than coldest, skip the model
    if (delta < 4.0f) {
      Serial.println("[MODEL] Skipped - Flat background temperature");
    } else {
      raw_detection = predict_animal(frame);
    }

    if (raw_detection) {
      votes++;
      if (votes == REQUIRED_VOTES) {
        // Keep capturing the remaining frames so the direct USB visualizer
        // receives the complete eight-frame sequence.
        memcpy(decidingFrame, frame, sizeof(decidingFrame));
        // Send the compact alert immediately; remaining frames continue for
        // the direct USB visualizer and improve the final frame selection.
        if (!fastAlertSent) {
          fastAlertSent = initializeLoRa() && sendStatus(true);
          Serial.println(fastAlertSent ? "[FAST ALERT] sent" : "[FAST ALERT] failed");
        }
      }
    }

    // Print the 768 pixels for visualizer.py
    for (int p = 0; p < 768; p++) {
      Serial.print(frame[p], 1);
      Serial.print(',');
    }
    // Print the final verdict for this frame (1 or 0)
    Serial.println(raw_detection ? 1 : 0);
  }

  Serial.printf("[MODEL] %d/%d Animal votes\n", votes, validFrames);
  const bool detected = (validFrames >= REQUIRED_VOTES) && (votes >= REQUIRED_VOTES);
  return detected ? decidingFrame : nullptr;
}

// --- Minimal Base64 encoder, no external library needed ---
static const char B64_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void base64Encode(const uint8_t* src, size_t srcLen, String& out) {
  out.reserve(out.length() + ((srcLen + 2) / 3) * 4);
  for (size_t i = 0; i < srcLen; i += 3) {
    uint32_t b = (uint32_t)src[i] << 16;
    if (i + 1 < srcLen) b |= (uint32_t)src[i + 1] << 8;
    if (i + 2 < srcLen) b |= (uint32_t)src[i + 2];
    out += B64_CHARS[(b >> 18) & 0x3F];
    out += B64_CHARS[(b >> 12) & 0x3F];
    out += (i + 1 < srcLen) ? B64_CHARS[(b >>  6) & 0x3F] : '=';
    out += (i + 2 < srcLen) ? B64_CHARS[ b        & 0x3F] : '=';
  }
}

// Each command consumes its own response; +OK is module acceptance, not remote delivery.
bool radioCommand(const String& command) {
  while (LoRaSerial.available()) LoRaSerial.read();
  LoRaSerial.println(command);
  String response;
  const unsigned long started = millis();
  while (millis() - started < 5000) {
    while (LoRaSerial.available()) {
      const char c = LoRaSerial.read();
      if (c == '\n') {
        response.trim();
        Serial.println("[RADIO] " + response);
        if (response == "+OK") { delay(350); return true; }
        if (response.startsWith("+ERR")) return false;
        response = "";
      } else if (c != '\r') response += c;
    }
    delay(1);
  }
  Serial.println("[RADIO] Command timed out: " + command.substring(0, 24));
  return false;
}

bool sendPacket(const String& payload) {
  if (payload.length() > 240) return false;
  return radioCommand(String("AT+SEND=") + LORA_DESTINATION + "," + payload.length() + "," + payload);
}

void sendFrame(const float* f) {
  float lo = f[0], hi = f[0];
  for (int i = 0; i < 768; ++i) {
    if (!isfinite(f[i])) { Serial.println("[FRAME] Invalid temperature"); return; }
    lo = min(lo, f[i]); hi = max(hi, f[i]);
  }
  // Quantize against exactly the same endpoints used by the receiver.
  lo = floorf(lo * 10) / 10; hi = ceilf(hi * 10) / 10;
  uint8_t q[768];
  for (int i = 0; i < 768; ++i)
    q[i] = hi > lo ? (uint8_t)lroundf((f[i] - lo) * 255 / (hi - lo)) : 0;
  const String id = timestamp();
  const String header = String("FRAME_HDR,") + id + "," + String((int)lroundf(lo * 10))
      + "," + String((int)lroundf(hi * 10)) + ",6";
  if (!sendPacket(header)) { Serial.println("[FRAME] Header send failed; aborted"); return; }
  for (int chunk = 0; chunk < 6; ++chunk) {
    String encoded;
    base64Encode(q + chunk * 150, min(150, 768 - chunk * 150), encoded);
    if (!sendPacket(String("FRAME_DATA,") + id + "," + chunk + "," + encoded)) {
      Serial.println("[FRAME] Chunk send failed; aborted"); return;
    }
  }
  Serial.println("[FRAME] All packets accepted by local radio");
}

bool initializeLoRa() {
  LoRaSerial.begin(LORA_BAUD, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN);
  delay(200);
  const char* commands[] = {"AT", "AT+ADDRESS=5", "AT+NETWORKID=11",
    "AT+BAND=868500000", "AT+PARAMETER=7,7,2,6", "AT+CRFOP=14"};
  for (const char* command : commands) if (!radioCommand(command)) return false;
  return true;
}

bool sendStatus(bool wildlife) {
  return sendPacket(String("STATUS,wildlife=") + (wildlife ? "1" : "0")
      + ",min_x10=" + String((int)(lastMin * 10))
      + ",max_x10=" + String((int)(lastMax * 10))
      + ",uptime_ms=" + String(millis()));
}

void monitorPirAndHeartbeat(bool wildlife) {
  if (!wildlife) return; // the wake-cycle STATUS,wildlife=0 already cleared it

  unsigned long nextHeartbeat = millis() + WILDLIFE_HEARTBEAT_MS;
  Serial.println("[ALERT] PIR high; heartbeat monitoring started");
  while (digitalRead(PIR_PIN) == HIGH) {
    if ((long)(millis() - nextHeartbeat) >= 0) {
      if (sendStatus(true)) Serial.println("[HEARTBEAT] wildlife alert refreshed");
      nextHeartbeat = millis() + WILDLIFE_HEARTBEAT_MS;
    }
    delay(100);
  }
  if (sendStatus(false)) Serial.println("[CLEAR] PIR low; wildlife alert cleared");
}

[[noreturn]] void enterDeepSleep() {
  Serial.println("[SLEEP] Waiting for PIR to go LOW...");
  Serial.flush();
  unsigned long waitStart = millis();
  constexpr unsigned long PIR_LOW_TIMEOUT_MS = 15000;
  while (digitalRead(PIR_PIN) == HIGH) {
    if (millis() - waitStart > PIR_LOW_TIMEOUT_MS) {
      Serial.println("[SLEEP] PIR still HIGH after timeout, sleeping anyway");
      break;
    }
    delay(100);
  }
  Serial.printf("[SLEEP] PIR is %s, entering deep sleep\n",
                digitalRead(PIR_PIN) == HIGH ? "HIGH" : "LOW");
  Serial.flush();
  esp_sleep_enable_ext0_wakeup(PIR_PIN, 1);
  delay(20);
  esp_deep_sleep_start();
  while (true) delay(1000);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(PIR_PIN, INPUT_PULLDOWN);

  const esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
  if (wakeCause == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("[WAKE] PIR motion detected");
    const float* decidingFrame = detectAnimal();
    if (!fastAlertSent && !initializeLoRa()) enterDeepSleep();
    if (!fastAlertSent && !sendStatus(decidingFrame != nullptr)) enterDeepSleep();
    if (decidingFrame) {
      delay(600); // give receiver RYLR module time to exit post-receive state
      sendFrame(decidingFrame); // transmit the frame that cast the deciding vote
    } else {
      Serial.println("[MODEL] No Animal finding");
    }
    monitorPirAndHeartbeat(decidingFrame != nullptr);
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
