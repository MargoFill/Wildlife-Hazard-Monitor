# Combined sensor node

The project uses the board's default flash mode and does not force PSRAM. Do not force `opi`/`opi_opi` or define `BOARD_HAS_PSRAM` unless the exact ESP32-S3 module has those features configured; otherwise the bootloader can report flash or PSRAM errors before the application starts.

The ESP32-S3 remains in deep sleep until the PIR output goes HIGH. After wake it captures eight MLX90640 frames, runs the embedded model, and requires five Moose votes before transmitting a LoRa alert. It waits for the PIR output to return LOW before arming the next wake event.

## Wiring

- PIR output: GPIO5 (must be an RTC-capable pin for `ext0` wake)
- MLX90640 SDA/SCL: GPIO8/GPIO9
- RYLR890 module RX/TX: ESP32 GPIO3/GPIO2 (UART1, 9600 baud)
- Status LED: GPIO40

The LoRa destination address is `1`, and the radio is configured for 868.5 MHz. The transmitted payload is compact text:

`WILDLIFE,moose,node=sensor-1,ts=<unix-seconds>`

If the ESP32 has no wall-clock time configured, `ts=uptime_ms=<milliseconds>` is sent instead. Set the ESP32 clock from a gateway, GPS, or an RTC if an absolute timestamp is required.

`include/thermal_model.h` is the model used by the AI camera project, copied into this project so PlatformIO can compile the sensor node independently.
