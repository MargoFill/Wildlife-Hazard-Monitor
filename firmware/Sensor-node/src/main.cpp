#include <Arduino.h>
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

Adafruit_MLX90640 mlx;
HardwareSerial LoRaSerial(1);

String timestamp() {
  const time_t now = time(nullptr);
  if (now >= 1700000000) return String(static_cast<unsigned long>(now));
  return String("uptime_ms=") + String(static_cast<unsigned long>(millis()));
}

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
        // Snapshot the deciding-vote frame and exit immediately — no need to
        // capture remaining frames; starting LoRa sooner saves up to
        // (FRAME_COUNT - i - 1) * 250 ms at the 4 Hz refresh rate.
        memcpy(decidingFrame, frame, sizeof(decidingFrame));
        break; // early exit — votes threshold met; validFrames >= REQUIRED_VOTES already holds
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

// Transmit one quantised thermal frame over LoRa.
// Packet format:
//   Header : FRAME_HDR,<ts>,<t_min_x10>,<t_max_x10>,<num_chunks>
//   Data x5: FRAME_DATA,<ts>,<chunk_idx>,<base64_payload>
// Receiver dequantises with: temp = t_min + (q / 255.0) * (t_max - t_min)
void sendFrame(const float* f) {
  float t_min = 100.0f, t_max = -100.0f;
  for (int p = 0; p < 768; p++) {
    if (f[p] > t_max) t_max = f[p];
    if (f[p] < t_min) t_min = f[p];
  }
  const float range = (t_max - t_min) > 0.0f ? (t_max - t_min) : 1.0f;

  uint8_t q[768];
  for (int p = 0; p < 768; p++) {
    q[p] = (uint8_t)(((f[p] - t_min) / range) * 255.0f + 0.5f);
  }

  const int totalChunks = (768 + FRAME_CHUNK_PIXELS - 1) / FRAME_CHUNK_PIXELS;
  const String ts = timestamp();

  // Header packet
  {
    String payload = String("FRAME_HDR,") + ts
        + "," + String((int)(t_min * 10.0f))
        + "," + String((int)(t_max * 10.0f))
        + "," + String(totalChunks);
    String cmd = String("AT+SEND=") + LORA_DESTINATION + "," + payload.length() + "," + payload;
    LoRaSerial.println(cmd);
    LoRaSerial.flush();
    Serial.println("[LORA FRAME HDR] " + payload);
    delay(500);
  }

  // Data packets
  String b64; // pre-allocated once; reused per chunk to avoid repeated heap churn
  for (int chunk = 0; chunk < totalChunks; chunk++) {
    const int start  = chunk * FRAME_CHUNK_PIXELS;
    const int pixels = min(FRAME_CHUNK_PIXELS, 768 - start);

    b64.clear();
    base64Encode(q + start, (size_t)pixels, b64);

    String payload = String("FRAME_DATA,") + ts
        + "," + String(chunk)
        + "," + b64;
    // Drain any leftover bytes (e.g. previous +OK) before sending the next command
    while (LoRaSerial.available()) LoRaSerial.read();

    String cmd = String("AT+SEND=") + LORA_DESTINATION + "," + payload.length() + "," + payload;
    LoRaSerial.println(cmd);
    LoRaSerial.flush();
    Serial.printf("[LORA FRAME] chunk %d/%d (%d chars)\n", chunk + 1, totalChunks, payload.length());

    // Gate on +OK or ERR before sending next chunk
    const unsigned long t0 = millis();
    while (millis() - t0 < 2000) {
      if (LoRaSerial.available()) {
        String resp = LoRaSerial.readStringUntil('\n');
        resp.trim();
        Serial.println("[LORA RESP] " + resp);
        if (resp.indexOf("+OK") >= 0 || resp.indexOf("ERR") >= 0) break;
      }
      delay(10);
    }
    delay(300); // 300ms idle before next send — prevents back-to-back UART collisions
  }
  Serial.println("[LORA FRAME] All chunks sent");
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
  // AT+PARAMETER=<SF>,<BW>,<CR>,<Preamble>
  //   SF=7   Spreading Factor. Range 5-11. Higher = longer range but slower & more airtime.
  //          SF7 is the fastest option (~300-800m open field, less in dense forest).
  //          Gives ~8x less airtime than SF10. Increase to SF8/9/10 only if RSSI at the
  //          gateway is worse than -120 dBm or SNR below -10 dB.
  //   BW=7   Bandwidth index. 7=125kHz (standard). 8=250kHz, 9=500kHz.
  //          125kHz is the safest choice: best sensitivity and widest receiver support.
  //   CR=2   Coding Rate. 1=4/5, 2=4/6, 3=4/7, 4=4/8.
  //          CR 4/6 adds moderate forward error correction. Use CR=1 (4/5) in a clean
  //          RF environment to shave ~15% off airtime with no real downside.
  //   Preamble=6  Symbols prepended so receiver can lock on. Min is 4 per LoRa spec.
  //          6 is a safe margin for outdoor use; 4 saves a small amount of airtime.
  LoRaSerial.println("AT+PARAMETER=7,7,2,6");  // SF7, 125kHz, CR 4/6 — fast; CR 4/5 causes ERR=13 on back-to-back sends
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
      if (response.indexOf("+OK") >= 0 || response.indexOf("ERR") >= 0) break;
    }
    delay(10);
  }
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
    if (decidingFrame) {
      sendAnimalFinding();
      sendFrame(decidingFrame); // transmit the frame that cast the deciding vote
    } else {
      Serial.println("[MODEL] No Animal finding");
    }
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
