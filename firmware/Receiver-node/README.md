# Receiver node

This is the PlatformIO version of the receiver previously maintained as an Arduino IDE `.ino` sketch. The implementation now lives in [`src/main.cpp`](src/main.cpp); there is no duplicate `.ino` entry point.

The receiver listens for RYLR890 `+RCV` lines, displays an alert on the onboard RGB LED, and appends received payloads to `/detections.csv` on the SD card.

## Current wiring

- LoRa RX/TX: GPIO17/GPIO16
- SD card SPI: SCK GPIO12, MISO GPIO13, MOSI GPIO10, CS GPIO11
- RGB LED: GPIO38

Adjust the constants in `src/main.cpp` for a different board or wiring. The SD log records the receiver uptime, source LoRa address, and received payload. The receiver must be powered and have a mounted SD card for entries to be written.
