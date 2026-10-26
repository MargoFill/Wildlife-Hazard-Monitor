#include <Arduino.h>
#include <HardwareSerial.h>

// Приёмник LoRa БЕЗ сна (для отладки).
// Модуль слушает эфир, ESP всегда включён, печатает всё, что пришло,
// и при получении пакета (+RCV) мигает белым светодиодом.

// ---------- настройки радио (совпадают с передатчиком) ----------
constexpr int           LORA_BAUD         = 115200;
constexpr uint8_t       LORA_SELF_ADDRESS = 1;       // передатчик шлёт на адрес 1
constexpr int           LORA_NETWORK_ID   = 11;
constexpr const char*   LORA_BAND         = "868500000";
constexpr const char*   LORA_PARAMS       = "10,7,2,6";

// ---------- подключение (ESP32-S3) ----------
constexpr int LORA_RX_PIN = 17;   // ESP RX <- TXD модуля
constexpr int LORA_TX_PIN = 16;   // ESP TX -> RXD модуля

// ---------- индикатор: встроенный RGB на GPIO 38 ----------
constexpr int           RGB_PIN     = 38;
constexpr unsigned long LED_HOLD_MS = 1000;

HardwareSerial LoRaSerial(1);
String line;
unsigned long led_off_at = 0;

// Отправить команду модулю и показать ответ
void sendCmd(const String &cmd) {
  Serial.println(">> " + cmd);
  LoRaSerial.println(cmd);
  unsigned long t = millis();
  while (millis() - t < 300) {
    while (LoRaSerial.available()) Serial.write(LoRaSerial.read());
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  neopixelWrite(RGB_PIN, 0, 0, 0);   // светодиод выключен

  LoRaSerial.begin(LORA_BAUD, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN);
  delay(100);

  Serial.println("[BOOT] configuring LoRa module");
  sendCmd("AT");                                          // проверка связи, ждём +OK
  sendCmd("AT+ADDRESS=" + String(LORA_SELF_ADDRESS));
  sendCmd("AT+NETWORKID=" + String(LORA_NETWORK_ID));
  sendCmd("AT+BAND=" + String(LORA_BAND));
  sendCmd("AT+PARAMETER=" + String(LORA_PARAMS));
  Serial.println("[READY] waiting for LoRa packets...");
}

void loop() {
  // Читаем строки от модуля
  while (LoRaSerial.available()) {
    char c = LoRaSerial.read();
    if (c == '\n') {
      line.trim();
      if (line.length() > 0) {
        Serial.println("[LORA] " + line);
        if (line.startsWith("+RCV")) {
          Serial.println("[WAKE] LoRa signal received");
          neopixelWrite(RGB_PIN, 40, 40, 40);
          led_off_at = millis() + LED_HOLD_MS;
        }
      }
      line = "";
    } else if (c != '\r') {
      line += c;
    }
  }

  // Выключить светодиод через секунду
  if (led_off_at != 0 && millis() > led_off_at) {
    neopixelWrite(RGB_PIN, 0, 0, 0);
    led_off_at = 0;
  }
}
