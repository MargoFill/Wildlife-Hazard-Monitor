 #include <Arduino.h>
  #include <HardwareSerial.h>

  constexpr int           LORA_BAUD         = 115200;
  constexpr uint8_t       LORA_SELF_ADDRESS = 1;
  constexpr int           LORA_NETWORK_ID   = 11;
  constexpr const char*   LORA_BAND         = "868500000";
  constexpr const char*   LORA_PARAMS       = "10,7,2,6";

  constexpr int LORA_RX_PIN = 17;
  constexpr int LORA_TX_PIN = 16;

  constexpr int           RGB_PIN     = 38;
  constexpr unsigned long LED_HOLD_MS = 1000;

  HardwareSerial LoRaSerial(1);
  String line;
  unsigned long led_off_at = 0;

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
    neopixelWrite(RGB_PIN, 0, 0, 0);

    LoRaSerial.begin(LORA_BAUD, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN);
    delay(100);

    Serial.println("[BOOT] configuring LoRa module");
    sendCmd("AT");
    sendCmd("AT+ADDRESS=" + String(LORA_SELF_ADDRESS));
    sendCmd("AT+NETWORKID=" + String(LORA_NETWORK_ID));
    sendCmd("AT+BAND=" + String(LORA_BAND));
    sendCmd("AT+PARAMETER=" + String(LORA_PARAMS));
    Serial.println("[READY] waiting for moose detection...");
  }

  void loop() {
    while (LoRaSerial.available()) {
      char c = LoRaSerial.read();
      if (c == '\n') {
        line.trim();
        if (line.length() > 0 && line.startsWith("+RCV")) {
          Serial.println("[RAW] " + line);

          int c1 = line.indexOf(',');
          int c2 = line.indexOf(',', c1 + 1);
          int lastC = line.lastIndexOf(',');
          int secondLastC = line.lastIndexOf(',', lastC - 1);
          if (c2 >= 0 && secondLastC > c2) {
            String msg = line.substring(c2 + 1, secondLastC);
            Serial.println("[MESSAGE] " + msg);
          }

          Serial.println("[WAKE] Moose is detected");
          neopixelWrite(RGB_PIN, 40, 40, 40);
          led_off_at = millis() + LED_HOLD_MS;
        }
        line = "";
      } else if (c != '\r') {
        line += c;
      }
    }

    if (led_off_at != 0 && millis() > led_off_at) {
      neopixelWrite(RGB_PIN, 0, 0, 0);
      led_off_at = 0;
    }
  }
