#include <Wire.h>
#include <Adafruit_MLX90640.h>

Adafruit_MLX90640 mlx;
float frame[32 * 24];

void setup() {
  Serial.begin(921600);
  delay(2000);
  Wire.begin(8, 9);          // SDA, SCL
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

  for (int i = 0; i < 768; i++) {
    Serial.print(frame[i], 1);
    if (i < 767) Serial.print(',');
  }
  Serial.println();
}
