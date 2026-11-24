# Combined sensor node

The project uses the board's default flash mode and does not force PSRAM. Do not force `opi`/`opi_opi` or define `BOARD_HAS_PSRAM` unless the exact ESP32-S3 module has those features configured; otherwise the bootloader can report flash or PSRAM errors before the application starts.

The ESP32-S3 remains in deep sleep until the PIR output goes HIGH. After wake it captures eight MLX90640 frames, runs the embedded model, and requires five animal votes before transmitting a LoRa alert. It waits for the PIR output to return LOW before arming the next wake event. SD-card logging is handled by the receiver node; the sensor node does not mount or write an SD card.

## Wiring

- PIR output: GPIO5 (must be an RTC-capable pin for `ext0` wake)
- MLX90640 SDA/SCL: GPIO8/GPIO9
- RYLR890 module RX/TX: ESP32 GPIO3/GPIO2 (UART1, 9600 baud)
- Status LED: GPIO40

The LoRa destination address is `1`, and the radio is configured for 868.5 MHz. The transmitted payload is compact text:

`WILDLIFE,moose,node=sensor-1,ts=<unix-seconds>`

If the ESP32 has no wall-clock time configured, `ts=uptime_ms=<milliseconds>` is sent instead. Set the ESP32 clock from a gateway, GPS, or an RTC if an absolute timestamp is required.

`include/thermal_model.h` is the model used by the AI camera project, copied into this project so PlatformIO can compile the sensor node independently.

## Benchmark results

The following measurements came from one wildlife event logged on the sensor and receiver serial consoles:

| Metric | Result |
| --- | ---: |
| Model inference per frame | 7-8 ms (mean 7.83 ms) |
| Complete sensor decision | 8.538 s |
| Fast wildlife status packet | 491 ms local send transaction |
| Complete thermal frame transmission | 5.023 s |

The decision time includes MLX90640 capture and processing for eight frames. The model inference value is only the model call itself. The fast status packet is sent as soon as five wildlife votes are reached, before the larger frame transfer.

The `packet_tx_ms` value measures the sensor's AT command and local RYLR response. It does not by itself prove over-the-air arrival time.

The firmware prints additional measurements using `[BENCH]` lines: `inference_ms`, `decision_ms`, `packet_tx_ms`, and `frame_tx_ms`.
