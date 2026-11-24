# Benchmark Notes

Firmware benchmark lines are printed to USB serial:

- Sensor: inference_ms, decision_ms, packet_tx_ms, frame_tx_ms.
- Receiver: status_received_ms, LED processing time, frame_reassembly_ms.

These use each board's local millis() clock. Cross-board timestamps cannot be subtracted directly. True animal-to-LED latency requires a shared GPIO trigger and logic analyzer. Collect at least 20 events and calculate median, 95th percentile, and maximum.
