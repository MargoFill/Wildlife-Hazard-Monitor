# Benchmark Notes

## Firmware output

After reflashing both boards, watch USB serial at 115200 baud.

Sensor output:

- `[BENCH] inference_ms=...` — model time for one camera frame.
- `[BENCH] decision_ms=...` — camera/model decision time for the wake event.
- `[BENCH] packet_tx_ms=...` — time for one LoRa packet command and local radio response.
- `[BENCH] frame_tx_ms=...` — header plus all thermal-frame packets.

Receiver output:

- `[BENCH] status_received_ms=...` — receiver-local arrival time.
- `[BENCH] led_set_ms=... processing_ms=...` — alert handling time.
- `[BENCH] frame_reassembly_ms=...` — header-to-complete-frame time.

## Collecting results

Capture at least 20 events from each serial monitor. Record the numeric values in a spreadsheet or Python script and calculate minimum, mean, median, 95th percentile, and maximum.

The two ESP32 boards have independent `millis()` clocks, so subtracting sensor and receiver timestamps is not a valid end-to-end latency measurement. For exact animal-to-LED latency, toggle a spare sensor GPIO when detection is confirmed and measure that edge against the receiver LED GPIO with a logic analyzer or oscilloscope.

## Files

- Sensor firmware: `Sensor-node/src/main.cpp`
- Receiver firmware: `Receiver-node/src/main.cpp`
- Receiver visualizer: `Receiver-node/tools/visualizer.py`
