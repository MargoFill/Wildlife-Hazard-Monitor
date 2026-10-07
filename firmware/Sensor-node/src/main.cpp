#include <Arduino.h>

#define LED_PIN 40
#define PIR_PIN 5  

void setup() {
  Serial.begin(115200);
  delay(500); 
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(PIR_PIN, INPUT); // Standard INPUT works best for Seeed modules

  // 1. Determine what caused the ESP32-S3 to boot up
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    // --- MOTION TRIGGERED WAKEUP ---
    Serial.println("\n[WAKE] PIR Motion Detected!");
    
    // Execute your tracking sequence
    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_PIN, HIGH);
      delay(200);
      digitalWrite(LED_PIN, LOW);
      delay(200);
    }
    
    Serial.println("[PROCESS] Processing completed. (Ready for MLX90640 / LoRa tasks)"); 
  } 
  else {
    // --- COLD BOOT / BATTERY INSERTION ---
    // Let the Seeed Studio chip complete its baseline room calibration
    Serial.println("\n[BOOT] Cold boot detected. Calibrating Seeed PIR module...");
    for (int i = 0; i < 20; i++) {
      digitalWrite(LED_PIN, HIGH);
      delay(500);
      digitalWrite(LED_PIN, LOW);
      delay(500);
      Serial.print(".");
    }
    Serial.println("\n[BOOT] Calibration finished.");
  }

  // 2. CRITICAL DEEP SLEEP PREPARATION
  // The Seeed sensor latches HIGH for up to 4 seconds per trigger event.
  // We must block execution until the physical sensor drops the pin back to LOW.
  if (digitalRead(PIR_PIN) == HIGH) {
    Serial.println("[SLEEP] PIR pin is still HIGH. Waiting for area to clear...");
    while (digitalRead(PIR_PIN) == HIGH) {
      delay(100); // Poll every 100ms until the module resets its active signal
    }
    Serial.println("[SLEEP] PIR pin cleared to LOW.");
  }

  // 3. Configure the wake-up trigger
  // Arguments: (RTC_GPIO_Pin, Level) -> 1 triggers when the pin goes HIGH
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIR_PIN, 1);

  // 4. Enter Low-Power Deep Sleep
  Serial.println("[SLEEP] Entering Deep Sleep mode now...");
  Serial.flush(); // Force the serial buffer to clear before the main processor shuts off
  
  esp_deep_sleep_start();
}

void loop() {
  // Stays empty. Execution halts at esp_deep_sleep_start().
}