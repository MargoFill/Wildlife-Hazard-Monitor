
#include <Arduino.h>
#include <HardwareSerial.h>
#include <SPI.h>
#include <SD.h>
#include "esp_sleep.h"
#include "driver/gpio.h"

constexpr int LORA_BAUD = 115200;
constexpr uint8_t LORA_SELF_ADDRESS = 1;
constexpr int LORA_NETWORK_ID = 11;
constexpr const char* LORA_BAND = "868500000";

constexpr const char* LORA_PARAMS = "7,7,2,6"; // SF7, 125kHz, CR 4/6 — matches sensor node

constexpr int LORA_RX_PIN = 17;
constexpr int LORA_TX_PIN = 16;
constexpr int RGB_PIN = 38;
// Set this to the GPIO connected to the external alert LED. Set to -1 if unused.
constexpr int WILDLIFE_LED_PIN = 2;
constexpr unsigned long LED_HOLD_MS = 1000;

// SD wiring
constexpr int SD_CARD_PIN_MOSI = 10;
constexpr int SD_CARD_PIN_CS = 11;
constexpr int SD_CARD_PIN_SCK = 12;
constexpr int SD_CARD_PIN_MISO = 13;

HardwareSerial LoRaSerial(1);
String line;
unsigned long led_off_at = 0;
bool sd_ready = false;

// --- Frame reassembly state ---
struct FrameAssembly {
  String    source;
  String    ts;               // timestamp from header
  float     t_min = 0.0f;
  float     t_max = 0.0f;
  int       totalChunks = 0;
  int       receivedChunks = 0;
  uint8_t   q[768];           // quantised pixel buffer
  bool      active = false;   // true while a frame is in progress
  bool      received[16] = {}; // duplicate packets must not complete a frame
  unsigned long startedAt = 0;
} frame;

constexpr unsigned long FRAME_TIMEOUT_MS = 15000; // abandon incomplete frame after 15 s

void sendCmd(const String& cmd) {
  Serial.println(">> " + cmd);
  LoRaSerial.println(cmd);
  const unsigned long started = millis();
  String response = "";
  while (millis() - started < 300) {
    while (LoRaSerial.available()) {
      const char c = LoRaSerial.read();
      if (c == '\n') {
        response.trim();
        if (response.length() > 0) Serial.println("<< " + response);
        response = "";
      } else if (c != '\r') {
        response += c;
      }
    }
  }
  if (response.length() > 0) Serial.println("<< " + response);
}

bool initializeSd() {
  Serial.println("[SD] starting");
  SPI.begin(SD_CARD_PIN_SCK, SD_CARD_PIN_MISO, SD_CARD_PIN_MOSI, SD_CARD_PIN_CS);
  if (!SD.begin(SD_CARD_PIN_CS)) {
    Serial.println("[SD] Card mount failed");
    return false;
  }

  if (!SD.exists("/detections.csv")) {
    File file = SD.open("/detections.csv", FILE_WRITE);
    if (!file) {
      Serial.println("[SD] Failed to create /detections.csv");
      return false;
    }
    file.println("receiver_timestamp,source_address,payload");
    file.close();
  }

  Serial.println("[SD] Card initialized");
  return true;
}

// --- Minimal Base64 decoder ---
static int b64val(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1; // padding or invalid
}

// Decode Base64 string into dst. Returns number of bytes written.
static int base64Decode(const String& src, uint8_t* dst, int dstMax) {
  int out = 0;
  for (int i = 0; i + 3 < src.length() && out < dstMax; i += 4) {
    const int a = b64val(src[i]),     b = b64val(src[i+1]);
    const int c = b64val(src[i+2]),   d = b64val(src[i+3]);
    if (a < 0 || b < 0) break;
    dst[out++] = (uint8_t)((a << 2) | (b >> 4));
    if (c >= 0 && out < dstMax) dst[out++] = (uint8_t)((b << 4) | (c >> 2));
    if (d >= 0 && out < dstMax) dst[out++] = (uint8_t)((c << 2) | d);
  }
  return out;
}

// Log the dequantised frame to SD as CSV rows and print a compact summary.
// Format per row: receiver_ts,source,FRAME,<ts>,<pixel_index>,<temp_x10>
void logFrame(const String& source) {
  const float range = frame.t_max - frame.t_min;
  Serial.printf("[FRAME] Reassembled from %s | t_min=%.1f t_max=%.1f\n",
                source.c_str(), frame.t_min, frame.t_max);

  // Print CSV-compatible flat pixel line for visualizer
  // Format: FRAME_PIXELS,<ts>,<768 comma-separated float temps>,<source>
  Serial.print("FRAME_PIXELS,");
  Serial.print(frame.ts);
  Serial.print(',');
  for (int p = 0; p < 768; p++) {
    float temp = frame.t_min + (frame.q[p] / 255.0f) * range;
    Serial.print(temp, 1);
    if (p < 767) Serial.print(',');
  }
  Serial.print(',');
  Serial.println(source);

  if (!sd_ready) return;
  File file = SD.open("/frames.csv", FILE_APPEND);
  if (!file) {
    Serial.println("[SD] Failed to open /frames.csv");
    return;
  }
  for (int p = 0; p < 768; p++) {
    float temp = frame.t_min + (frame.q[p] / 255.0f) * range;
    file.print(millis()); file.print(',');
    file.print(source);   file.print(',');
    file.print("FRAME");  file.print(',');
    file.print(frame.ts); file.print(',');
    file.print(p);        file.print(',');
    file.println((int)(temp * 10)); // store as integer tenths to save space
  }
  file.close();
}

void logReceivedPayload(const String& sourceAddress, const String& payload) {
  if (!sd_ready) return;

  File file = SD.open("/detections.csv", FILE_APPEND);
  if (!file) {
    Serial.println("[SD] Failed to open /detections.csv");
    return;
  }

  file.print(millis());
  file.print(',');
  file.print(sourceAddress);
  file.print(',');
  file.println(payload);
  file.close();
}

void handleStatus(const String& sourceAddress, const String& payload) {
  logReceivedPayload(sourceAddress, payload);
  Serial.println("[STATUS] " + payload);
  if (payload.indexOf("wildlife=1") >= 0) {
    if (WILDLIFE_LED_PIN >= 0) digitalWrite(WILDLIFE_LED_PIN, HIGH);
    neopixelWrite(RGB_PIN, 40, 20, 0);
    led_off_at = millis() + LED_HOLD_MS;
  }
}

// Extract the payload from a +RCV line.
// +RCV=<addr>,<len>,<payload>,<rssi>,<snr>
static bool parseRCV(const String& rcv, String& srcAddr, String& payload) {
  // find first comma after +RCV=
  const int eq   = rcv.indexOf('=');
  const int c1   = rcv.indexOf(',', eq + 1);   // after address
  const int c2   = rcv.indexOf(',', c1 + 1);   // after length
  const int last = rcv.lastIndexOf(',');         // before snr
  const int sLast= rcv.lastIndexOf(',', last-1); // before rssi
  if (eq < 0 || c1 < 0 || c2 < 0 || sLast <= c2) return false;
  srcAddr = rcv.substring(eq + 1, c1);
  payload = rcv.substring(c2 + 1, sLast);
  return true;
}

void handleLoRaLine(String receivedLine) {
  receivedLine.trim();
  if (receivedLine.length() == 0) return;

  // The +RCV= prefix can be preceded by UART noise — find it anywhere in the line.
  const int rcvIdx = receivedLine.indexOf("+RCV");
  if (rcvIdx < 0) return;
  if (rcvIdx > 0) receivedLine = receivedLine.substring(rcvIdx);

  Serial.println("[RAW] " + receivedLine);

  String srcAddr, payload;
  if (!parseRCV(receivedLine, srcAddr, payload)) {
    Serial.println("[LORA] Invalid +RCV line");
    return;
  }
  Serial.println("[MESSAGE] " + payload);

  // --- Abandon stale incomplete frame ---
  if (frame.active && (millis() - frame.startedAt > FRAME_TIMEOUT_MS)) {
    Serial.println("[FRAME] Timeout — discarding incomplete frame");
    frame.active = false;
  }

  if (payload.startsWith("FRAME_HDR,")) {
    // Format: FRAME_HDR,<ts>,<t_min_x10>,<t_max_x10>,<num_chunks>
    int f1 = payload.indexOf(',');           // after FRAME_HDR
    int f2 = payload.indexOf(',', f1 + 1);  // after ts
    int f3 = payload.indexOf(',', f2 + 1);  // after t_min_x10
    int f4 = payload.indexOf(',', f3 + 1);  // after t_max_x10
    if (f1 < 0 || f2 < 0 || f3 < 0 || f4 < 0) {
      Serial.println("[FRAME] Malformed FRAME_HDR");
      return;
    }
    frame.ts           = payload.substring(f1 + 1, f2);
    frame.source       = srcAddr;
    frame.t_min        = payload.substring(f2 + 1, f3).toInt() / 10.0f;
    frame.t_max        = payload.substring(f3 + 1, f4).toInt() / 10.0f;
    frame.totalChunks  = payload.substring(f4 + 1).toInt();
    if (frame.totalChunks <= 0 || frame.totalChunks > 16 || frame.t_max < frame.t_min) {
      Serial.println("[FRAME] Invalid frame header values");
      frame.active = false;
      return;
    }
    frame.receivedChunks = 0;
    frame.active       = true;
    memset(frame.received, 0, sizeof(frame.received));
    frame.startedAt    = millis();
    memset(frame.q, 0, sizeof(frame.q));
    Serial.printf("[FRAME] HDR ts=%s t_min=%.1f t_max=%.1f chunks=%d\n",
                  frame.ts.c_str(), frame.t_min, frame.t_max, frame.totalChunks);

  } else if (payload.startsWith("FRAME_DATA,") && frame.active) {
    // Format: FRAME_DATA,<ts>,<chunk_idx>,<base64>
    int f1 = payload.indexOf(',');           // after FRAME_DATA
    int f2 = payload.indexOf(',', f1 + 1);  // after ts
    int f3 = payload.indexOf(',', f2 + 1);  // after chunk_idx
    if (f1 < 0 || f2 < 0 || f3 < 0) {
      Serial.println("[FRAME] Malformed FRAME_DATA");
      return;
    }
    const int chunkIdx = payload.substring(f2 + 1, f3).toInt();
    if (payload.substring(f1 + 1, f2) != frame.ts || srcAddr != frame.source) {
      Serial.println("[FRAME] Chunk does not match active header");
      return;
    }
    if (chunkIdx < 0 || chunkIdx >= frame.totalChunks || chunkIdx >= 16) {
      Serial.println("[FRAME] chunk index out of range");
      return;
    }
    const String b64   = payload.substring(f3 + 1);

    // Calculate destination offset in q[]
    // Each chunk holds up to (total_pixels / totalChunks, rounded) pixels.
    // We recover the original FRAME_CHUNK_PIXELS from the first chunk's decoded size.
    // Simpler: offset = chunkIdx * ceil(768 / totalChunks) ← use floor division consistent with sender.
    // Sender uses: start = chunk * FRAME_CHUNK_PIXELS (150), so we do the same.
    constexpr int CHUNK_PIXELS = 150;
    const int offset = chunkIdx * CHUNK_PIXELS;
    const int maxPixels = 768 - offset;
    if (offset >= 768) {
      Serial.printf("[FRAME] chunk %d out of range\n", chunkIdx);
      return;
    }

    const int decoded = base64Decode(b64, frame.q + offset, maxPixels);
    if (!frame.received[chunkIdx]) {
      frame.received[chunkIdx] = true;
      frame.receivedChunks++;
    } else {
      Serial.printf("[FRAME] duplicate chunk %d ignored for completion count\n", chunkIdx);
    }
    Serial.printf("[FRAME] chunk %d/%d decoded %d bytes\n",
                  chunkIdx + 1, frame.totalChunks, decoded);

    if (frame.receivedChunks >= frame.totalChunks) {
      logFrame(srcAddr);
      frame.active = false;
      neopixelWrite(RGB_PIN, 0, 40, 0); // green flash on complete frame
      led_off_at = millis() + LED_HOLD_MS;
    }

  } else if (payload.startsWith("STATUS,")) {
    handleStatus(srcAddr, payload);

  } else if (payload.startsWith("WILDLIFE,")) {
    // Detection alert packet (sent before the frame)
    logReceivedPayload(srcAddr, payload);
    Serial.println("[ALERT] Animal detection confirmed from " + srcAddr);
    const bool wildlife = payload.startsWith("WILDLIFE,1,");
    if (wildlife && WILDLIFE_LED_PIN >= 0) digitalWrite(WILDLIFE_LED_PIN, HIGH);
    neopixelWrite(RGB_PIN, wildlife ? 40 : 0, wildlife ? 20 : 0, 0); // amber on detection alert
    led_off_at = millis() + LED_HOLD_MS;

  } else {
    // Unknown payload — log it anyway
    logReceivedPayload(srcAddr, payload);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  neopixelWrite(RGB_PIN, 0, 0, 0);
  if (WILDLIFE_LED_PIN >= 0) {
    pinMode(WILDLIFE_LED_PIN, OUTPUT);
    digitalWrite(WILDLIFE_LED_PIN, LOW);
  }

  sd_ready = initializeSd();
  LoRaSerial.begin(LORA_BAUD, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN);
  delay(100);

  Serial.println("[BOOT] configuring LoRa module");
  sendCmd("AT");
  sendCmd("AT+ADDRESS=" + String(LORA_SELF_ADDRESS));
  sendCmd("AT+NETWORKID=" + String(LORA_NETWORK_ID));
  sendCmd("AT+BAND=" + String(LORA_BAND));
  sendCmd("AT+PARAMETER=" + String(LORA_PARAMS));
  Serial.println("[READY] waiting for animal detection...");
}

void loop() {
  // Drain all available UART data before considering sleep
  while (LoRaSerial.available()) {
    const char character = LoRaSerial.read();
    if (character == '\n') {
      line.trim();
      if (line.length() > 0) Serial.println("[LORA RAW] " + line);
      handleLoRaLine(line);
      line = "";
    } else if (character != '\r') {
      line += character;
    }
  }

  if (led_off_at != 0 && millis() > led_off_at) {
    neopixelWrite(RGB_PIN, 0, 0, 0);
    if (WILDLIFE_LED_PIN >= 0) digitalWrite(WILDLIFE_LED_PIN, LOW);
    led_off_at = 0;
  }

  // No light sleep — receiver must stay awake to catch all UART bytes.
  // Light sleep causes the first ~20 bytes of each LoRa packet to be lost
  // because the ESP32 wakes on the UART start bit but the UART peripheral
  // isn't ready in time, corrupting the +RCV= header.
  delay(5);
}
