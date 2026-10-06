#include <Arduino.h>

#define LED_PIN 40
#define PIR_PIN 5 

void setup() {
  Serial.begin(115200);
  delay(100); // Brief serial stabilization
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(PIR_PIN, INPUT);

  // 1. Check what caused the ESP32 to wake up
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    // Woke up from PIR sensor interrupt
    Serial.println("PIR triggered! Detection Sequence commencing!");
    
    // Your processing pipeline goes here. 
    // It will run to completion regardless of the PIR's current state.
    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_PIN, HIGH);
      delay(200);
      digitalWrite(LED_PIN, LOW);
      delay(200);
      digitalWrite(LED_PIN, HIGH);
      delay(200);
      digitalWrite(LED_PIN, LOW);
      delay(200);
    }
    
    Serial.println("Sequence completed. ( add MLX90640 and LoRa logic)"); 
  } 
  else {
    // Woke up from a hard reset or battery insertion
    Serial.println("Cold Boot: Allowing PIR sensor 30-60 seconds to calibrate...");
    // delay(30000); // Uncomment in production for hardware warmup
  }

  // 2. Configure the wake-up source for the next sleep cycle
  // Wake up when PIR_PIN (GPIO 5) goes HIGH (1)
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIR_PIN, 1);

  // 3. Go to deep sleep immediately
  Serial.println("Entering Deep Sleep...");
  Serial.flush(); // Ensure serial prints finish before sleeping
  esp_deep_sleep_start();
}

void loop() {
  // The loop is completely empty. 
  // The ESP32 will never reach this point because esp_deep_sleep_start() halts execution.
}