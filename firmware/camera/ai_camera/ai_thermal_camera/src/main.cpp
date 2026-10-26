#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MLX90640.h>
#include "thermal_model.h"
#include "animal_prediction.h"

Adafruit_MLX90640 mlx;
float frame[32 * 24];

// Sliding window for smoothing results (3/5 frames)
const int WINDOW_SIZE = 8;
int detection_history[WINDOW_SIZE] = {0};
int history_index = 0;

void setup() {
  Serial.begin(921600);
  delay(2000);
  Wire.begin(8, 9); // SDA, SCL
  Wire.setClock(400000);

  if (!mlx.begin(MLX90640_I2CADDR_DEFAULT, &Wire)) {
    while (1) {
      Serial.println("ERR");
      delay(1000);
    }
  }
  mlx.setMode(MLX90640_CHESS);
  mlx.setResolution(MLX90640_ADC_18BIT);
  mlx.setRefreshRate(MLX90640_4_HZ);
}

void loop() {
  if (mlx.getFrame(frame) != 0) return;
  uint32_t t0 = millis();
  // 1. Current frame assessment
  bool raw_detection = predict_animal(frame);
  uint32_t t1 = millis();
  // 2. debouncing
  detection_history[history_index] = raw_detection ? 1 : 0;
  history_index = (history_index + 1) % WINDOW_SIZE;

  int positive_votes = 0;
  for (int i = 0; i < WINDOW_SIZE; i++) {
    positive_votes += detection_history[i];
  }
  
  int final_verdict = (positive_votes >= 5) ? 1 : 0;

  // 3. Transmission of 768 pixels + 769th element (verdict: 1 = Moose, 0 = Forest)
  for (int i = 0; i < 768; i++) {
    Serial.print(frame[i], 1);
    Serial.print(',');
  }
  Serial.println(final_verdict);
  Serial.printf("\n inference time  %lu \n", (unsigned long) (t1-t0));
}