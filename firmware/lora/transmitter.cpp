#include <SoftwareSerial.h>

#define RX 3
#define TX 2
SoftwareSerial LoRaSerial(RX, TX); // LoRa module

void setup() {
  Serial.begin(9600);  // Serial Monitor
  LoRaSerial.begin(9600); // LoRa module
  delay(1000);

  Serial.println("Balloon Arduino Ready");

  // Configure LoRa
  LoRaSerial.println("AT+BAND=868500000\r\n");
  LoRaSerial.println("AT+PARAMETER=10,7,2,6\r\n"); // SF10, BW125kHz, CR 4/6, Preamble 6
  LoRaSerial.println("AT+CRFOP=14\r\n");
  delay(2000);
}

void loop() {
  if (Serial.available()) {
    String gpsData = Serial.readStringUntil("\n");  // Read GPS data from Raspberry Pi
    Serial.println("Received GPS Data: " + gpsData);

    if (gpsData != "waiting_for_fix") {
      String loraCommand = "AT+SEND=1," + String(gpsData.length()) + "," + gpsData;
      LoRaSerial.println(loraCommand);
      Serial.println("Sent to LoRa: " + loraCommand);
    } else {
      Serial.println("GPS Fix Pending...");
    }

    delay(20000); // Adjust if needed
  }
}
