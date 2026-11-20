
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
constexpr const char* LORA_PARAMS = "10,7,2,6";

constexpr int LORA_RX_PIN = 17;
constexpr int LORA_TX_PIN = 16;
constexpr int RGB_PIN = 38;
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

void handleLoRaLine(String receivedLine) {
  receivedLine.trim();
  if (receivedLine.length() == 0 || !receivedLine.startsWith("+RCV")) return;

  Serial.println("[RAW] " + receivedLine);

  const int firstComma = receivedLine.indexOf(',');
  const int secondComma = receivedLine.indexOf(',', firstComma + 1);
  const int lastComma = receivedLine.lastIndexOf(',');
  const int secondLastComma = receivedLine.lastIndexOf(',', lastComma - 1);
  if (firstComma < 0 || secondComma < 0 || secondLastComma <= secondComma) {
    Serial.println("[LORA] Invalid receive line");
    return;
  }

  const String sourceAddress = receivedLine.substring(firstComma + 1, secondComma);
  const String payload = receivedLine.substring(secondComma + 1, secondLastComma);
  Serial.println("[MESSAGE] " + payload);
  logReceivedPayload(sourceAddress, payload);

  Serial.println("[WAKE] Animal is detected");
  neopixelWrite(RGB_PIN, 40, 40, 40);
  led_off_at = millis() + LED_HOLD_MS;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  neopixelWrite(RGB_PIN, 0, 0, 0);

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
    led_off_at = 0;
  }

  // Only sleep when the LED is off and no partial line is buffered
  if (led_off_at == 0 && line.length() == 0) {
    // Wake on GPIO 17 going LOW (LoRa UART start bit)
    gpio_wakeup_enable((gpio_num_t)LORA_RX_PIN, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();
    Serial.println("Going to sleep");
    Serial.flush();  // Ensure all serial output is sent before sleeping
    esp_light_sleep_start();
    // CPU resumes here; UART state and all variables are preserved
  }
  Serial.println("WAKEUP");
}