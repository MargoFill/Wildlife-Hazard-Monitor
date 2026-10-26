#include <Arduino.h>
#include <HardwareSerial.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>

// Приёмник LoRa (Reyax) на ESP32-S3 с глубоким сном.
// Модуль слушает эфир, ESP спит. Когда модуль принял пакет, линия RX падает
// в LOW и будит ESP (ext0). ESP мигает светодиодом и снова засыпает.

// ---------- настройки радио (совпадают с передатчиком) ----------
constexpr int           LORA_BAUD         = 9600;
constexpr uint8_t       LORA_SELF_ADDRESS = 1;       // передатчик шлёт на адрес 1
constexpr int           LORA_NETWORK_ID   = 11;
constexpr const char*   LORA_BAND         = "868500000";
constexpr const char*   LORA_PARAMS       = "10,7,2,6";

// ---------- подключение (ESP32-S3) ----------
constexpr gpio_num_t LORA_RX_PIN = GPIO_NUM_17;  // ESP RX <- TXD модуля (источник пробуждения)
constexpr int        LORA_TX_PIN = 16;           // ESP TX -> RXD модуля

// ---------- индикатор: встроенный RGB на GPIO 38 ----------
constexpr int           RGB_PIN     = 38;
constexpr unsigned long LED_HOLD_MS = 1000;

HardwareSerial LoRaSerial(1);

void configureLoRaModule() {
  LoRaSerial.begin(LORA_BAUD, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN);
  delay(100);
  LoRaSerial.println("AT+ADDRESS=" + String(LORA_SELF_ADDRESS));
  LoRaSerial.println("AT+NETWORKID=" + String(LORA_NETWORK_ID));
  LoRaSerial.println("AT+BAND=" + String(LORA_BAND));
  LoRaSerial.println("AT+PARAMETER=" + String(LORA_PARAMS));
  delay(300);
  while (LoRaSerial.available()) Serial.write(LoRaSerial.read());  // показать ответы
  LoRaSerial.end();   // освободить пин для пробуждения
}

void goToSleep() {
  pinMode(LORA_RX_PIN, INPUT_PULLUP);
  while (digitalRead(LORA_RX_PIN) == LOW) delay(5);

  rtc_gpio_pullup_en(LORA_RX_PIN);
  esp_sleep_enable_ext0_wakeup(LORA_RX_PIN, 0);

  Serial.println("[SLEEP] waiting for LoRa data on RX pin...");
  Serial.flush();
  delay(20);
  esp_deep_sleep_start();
  while (true) delay(1000);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  neopixelWrite(RGB_PIN, 0, 0, 0);   // светодиод выключен

  const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

  if (cause == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("[WAKE] LoRa signal received");
    neopixelWrite(RGB_PIN, 40, 40, 40);
    delay(LED_HOLD_MS);
    neopixelWrite(RGB_PIN, 0, 0, 0);
  } else {
    Serial.println("[BOOT] configuring LoRa module");
    configureLoRaModule();
  }

  goToSleep();
}

void loop() {}
