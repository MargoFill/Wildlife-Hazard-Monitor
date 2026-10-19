#include <Wire.h>
#include <Adafruit_MLX90640.h>

Adafruit_MLX90640 mlx;
float frame[32 * 24];

void setup() {
  Serial.begin(115200);
  delay(2000);
  Wire.begin(8, 9);          // SDA, SCL
  Wire.setClock(400000);

  if (!mlx.begin(MLX90640_I2CADDR_DEFAULT, &Wire)) {
    Serial.println("MLX90640 не найден! Проверь провода");
    while (1) delay(10);
  }
  Serial.println("MLX90640 найден");
  mlx.setMode(MLX90640_CHESS);
  mlx.setResolution(MLX90640_ADC_18BIT);
  mlx.setRefreshRate(MLX90640_2_HZ);
}

void loop() {
  if (mlx.getFrame(frame) != 0) {
    Serial.println("Ошибка чтения кадра");
    return;
  }

  float mn = frame[0], mx = frame[0];
  for (int i = 1; i < 768; i++) {
    if (frame[i] < mn) mn = frame[i];
    if (frame[i] > mx) mx = frame[i];
  }
  Serial.printf("Min %.1f  Max %.1f  Центр %.1f C\n", mn, mx, frame[12 * 32 + 16]);

  const char *chars = " .:-=+*#%@";
  for (int y = 0; y < 24; y++) {
    for (int x = 0; x < 32; x++) {
      int idx = (frame[y * 32 + x] - mn) / (mx - mn + 0.01) * 9;
      Serial.print(chars[idx]);
      Serial.print(chars[idx]);
    }
    Serial.println();
  }
  Serial.println("--------------------");
}
