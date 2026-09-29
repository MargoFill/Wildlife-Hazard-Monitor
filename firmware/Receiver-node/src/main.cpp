#include <Arduino.h>

#define LED_PIN 40


void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(LED_PIN, OUTPUT);
  Serial.println();
  Serial.println("Plain LED blinking on GPIO40.");
}

void loop() {
  digitalWrite(LED_PIN, HIGH);   // on
  delay(500);                    // <-- wait half a second

  digitalWrite(LED_PIN, LOW);    // off
  delay(500);                    // <-- and again
}