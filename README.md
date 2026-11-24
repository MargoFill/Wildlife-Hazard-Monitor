# Wildlife Hazard Monitor

An embedded wildlife-detection prototype for identifying large animals near roads and warning people before a possible road hazard develops. Wildlife collisions are especially difficult to prevent at night and on narrow roads, where visibility, reaction time, and road conditions are limited. Existing road-safety systems cannot fully remove this risk.

The project combines an ESP32-S3 sensor node, an MLX90640 thermal camera, PIR motion sensing, an on-device classifier, and a LoRa receiver. A detection is processed at the edge and sent to a secondary receiver that provides a visible LED alert and stores the event on a microSD card. The resulting data can support road authorities, natural-resource departments, drivers, and scientists studying animal movement and road hazards.

> **Project status:** MVP complete for the course project. The end-to-end prototype detects thermal wildlife events, sends LoRa alerts, drives the receiver LED, logs events to SD, and reconstructs frames for visualization. Field reliability, weatherproofing, model accuracy, regulatory compliance, and safety performance still require further validation.

## Project overview

The intended deployment is an off-grid sensor near a road, wildlife crossing, or other area where animals may approach traffic and be able to send alerts to the road via flashing light to warn drivers. The sensor remains in deep sleep until a PIR event occurs. It then captures low-resolution thermal frames, classifies the scene locally, and sends a compact alert over LoRa when the required wildlife vote threshold is met. A receiver node stays powered at a safer or more accessible location, raises an LED, and records the event for later inspection.

This architecture avoids sending raw thermal images continuously. For demonstration purposes, the sensor sends a small status message first for low-latency alerts, then sends a quantized thermal frame only when wildlife is detected. The receiver reconstructs the frame for visualization and logging.

## Functional requirements

The system is designed to:

- Collect PIR and MLX90640 thermal sensor data.
- Process sensor data on the ESP32-S3 rather than relying on a cloud connection.
- Classify scenes that may contain a hazardous large animal.
- Generate a compact wildlife or clear status message.
- Send alerts over a long-range, low-power LoRa link.
- Raise a receiver LED with low processing delay.
- Store status messages and reconstructed thermal frames on an SD card.
- Provide direct sensor and receiver serial output for visualization and debugging.

## Non-functional requirements

- **Performance:** alert status is sent as soon as five positive votes are reached; the embedded model measured 7-8 ms per evaluated frame in the recorded benchmark.
- **Reliability:** frame headers, timestamps, source addresses, chunk indexes, duplicate chunks, and incomplete-frame timeouts are checked by the receiver.
- **Power efficiency:** the sensor uses PIR wake-up and deep sleep, while the receiver remains awake to avoid losing incoming UART data.
- **Scalability:** the packet format includes a source address and can be extended to multiple sensor nodes, subject to LoRa airtime and duty-cycle limits.
- **Movement direction:** direction estimation is a planned requirement, not a completed feature. The current prototype classifies thermal scenes but does not yet track an animal across frames to determine whether it is moving toward the road.

## MVP definition

The minimum viable product is one off-grid sensor node that detects a large heat source representing a hazardous animal, filters out flat background scenes, and sends a wireless alert to a secondary receiver node. The receiver successfully illuminates an LED, logs the event, and can reconstruct a transmitted thermal frame for inspection.

## System architecture

The prototype contains:

- Two ESP32-S3 development boards.
- An MLX90640 24x32 thermal camera on the sensor node.
- An EKMC1603111-style PIR motion sensor for wake-up.
- Two RYLR LoRa modules for the sensor-to-receiver link.
- A high-visibility external LED and onboard RGB LED on the receiver.
- A microSD card on the receiver for event and frame logging.
- Battery power for the sensor prototype.

### Interfaces and data flow

1. The PIR output wakes the sensor ESP32 through an external wake GPIO.
2. The ESP32 requests thermal frames from the MLX90640 over I2C.
3. The sensor evaluates eight frames and requires five positive model votes for wildlife detection.
4. At the fifth positive vote, the sensor sends a compact `STATUS,wildlife=1` packet immediately.
5. The sensor completes the camera sequence and sends a thermal frame only for a detected event. The frame uses one-byte quantization, a header, and six Base64 data packets.
6. The receiver parses the RYLR `+RCV` lines, validates and reassembles the frame, and prints `FRAME_PIXELS` for the receiver visualizer.
7. The receiver raises the alert LED, logs status packets to `/detections.csv`, and logs reconstructed pixels to `/frames.csv`.
8. Wildlife heartbeats are sent every 30 seconds while the sensor remains active. A clear status is sent when PIR goes LOW, and the receiver also has a 90-second communication-loss timeout.

## Technology selection

- **ESP32-S3:** selected over Raspberry Pi and STM32 for its low cost, integrated peripherals, useful processing capacity for small edge models, and lower power use than a general-purpose Linux computer.
- **MLX90640 thermal sensor:** works in darkness and uses less processing and energy than a conventional camera pipeline. A visible-light camera may provide richer classification data but requires more compute, power, and environmental control. A microphone is poorly suited to quiet animals.
- **RYLR LoRa modules:** provide long-range, low-power communication without a cellular subscription. Cellular networking could support direct cloud monitoring but would increase cost, power use, and deployment complexity.
- **PIR motion sensor:** provides an energy-efficient wake signal and reduces unnecessary thermal processing. Ultrasonic and time-of-flight alternatives are more sensitive to outdoor conditions or consume more active power.
- **microSD logging:** keeps event data local and simple to retrieve. It avoids the cost and energy of transmitting every frame to a cloud service.
- **Battery power:** the sensor is intended for off-grid operation, so deep sleep and event-driven radio use are central to the design.
- **Real-time clock:** the current firmware falls back to uptime timestamps when wall-clock time is unavailable. An RTC or network/GPS time source would improve long-term event correlation.

## Getting started

The firmware projects use [PlatformIO](https://platformio.org/). Open `firmware/Sensor-node` or `firmware/Receiver-node` as a PlatformIO project, review the corresponding `platformio.ini`, and build and flash each board separately.

Run the receiver visualizer from the receiver project directory:

```text
python tools/visualizer.py COM16
```

Use the receiver's USB serial port for reconstructed LoRa frames. The same visualizer can read direct sensor output when given the sensor's port, for example `COM8`; this shows the eight local camera frames before or during transmission.

The training script is in [`ML/machine_learning.py`](ML/machine_learning.py), with dataset splits under `ML/data_csv/`. It currently expects to be run from the `ML` directory and requires PyTorch, pandas, and NumPy.

## Firmware benchmark results

Measurements from one wildlife event:

| Metric | Result |
| --- | ---: |
| Model inference per frame | 7-8 ms (mean 7.83 ms) |
| Complete sensor decision | 8.538 s |
| Fast wildlife status packet | 491 ms local send transaction |
| Complete thermal frame transmission | 5.023 s |
| Receiver status handling to LED assertion | 18 ms |
| Receiver frame reassembly | 4.584 s |

The packet transmission metric measures the sensor's AT command and local RYLR response; it does not by itself prove exact over-the-air arrival time. The two boards have independent `millis()` clocks, so exact animal-to-LED latency requires a shared GPIO trigger measured with a logic analyzer or oscilloscope.

## Development plan

- **MVP:** complete a working sensor, receiver, alert LED, and local data logger.
- **Testing:** measure thermal detection performance, background filtering, LoRa delivery, packet loss, alert latency, and battery behavior.
- **Integration:** validate autonomous operation of the PIR, thermal camera, classifier, radio, receiver, LED, SD card, and enclosure.
- **Future work:** add movement-direction tracking, end-to-end CRCs, more field data, weatherproofing, power profiling, synchronized timestamps, and multi-node deployment tests.

## Limitations and risks

- The datasets are small and include synthetic background data; benchmark accuracy is not field accuracy.
- Extreme temperature and weather can affect batteries, sensor readings, radio performance, and MCU behavior.
- The current frame protocol has validation and reassembly checks but does not yet include an application-level end-to-end CRC.
- LoRa airtime and regional duty-cycle rules limit how frequently full frames can be sent.
- The receiver timeout is a communication-loss fallback; it cannot prove that an animal has left the scene.
- Pin assignments, radio settings, and external LED wiring are prototype details. Review the firmware source before wiring hardware.

## Repository contents

- **Sensor node** - ESP32-S3 firmware for PIR wake, MLX90640 capture, classification, low-latency status alerts, heartbeats, and frame transmission.
- **Receiver node** - PlatformIO firmware for LoRa reception, LED control, SD logging, frame reconstruction, and serial visualization.
- **Machine learning** - PyTorch training code and CSV datasets for `Animal` / `Forest` classification.
- **Camera experiments** - Standalone thermal and AI-camera sketches and visualization tools.
- **Hardware** - KiCad footprints, enclosure parts, and sensor-related models.
- **Docs** - Development TODO list and supporting project documentation.

## System overview

<img width="920" height="473" alt="image" src="https://github.com/user-attachments/assets/ecb367cb-c659-4261-bcc4-04f6c79234f3" />

## Contributors

- **Niklas Etling** - sensor integration, embedded development, and energy/software optimisation
- **Margarita Filenko** - machine learning, integration, and general development
- **Arsenii Marchenko** - LoRa communication, data logging, and general embedded development

## License

No license has been declared yet. Until one is added, assume that the contents are not licensed for reuse beyond the permissions granted by the repository host.
