# Wildlife Hazard Monitor

An embedded wildlife-detection prototype for identifying animals near roads and sending alerts over LoRa. The system combines a thermal camera, PIR motion sensing, an ESP32-S3, an on-device classifier, and battery-conscious deep-sleep operation.

> **Project status:** prototype / active development. This repository contains firmware, machine-learning experiments, documentation, and hardware assets. Field reliability, weatherproofing, and safety performance have not been validated here.

## What is here
- **Sensor node** - ESP32-S3 firmware for PIR wake, MLX90640 capture, classification, SD logging, and LoRa alerts.
- **Receiver node** - PlatformIO project that receives LoRa alerts, drives the status LED, and logs payloads to SD.
- **Machine learning** - PyTorch training code and CSV datasets for `Animal` / `Forest` classification.
- **Camera experiments** - Standalone thermal and AI-camera sketches and visualisation tools.
- **Hardware** - KiCad footprints, enclosure parts, and sensor-related models.
- **Docs** - Integration notes and the current development TODO list.

## System overview

<img width="920" height="473" alt="image" src="https://github.com/user-attachments/assets/ecb367cb-c659-4261-bcc4-04f6c79234f3" />

## Hardware currently referenced
- ESP32-S3 development boards
- MLX90640 24x32 thermal sensor
- RYLR890 LoRa modules
- PIR motion sensor
- SD card module
- CR123A battery cells

Review the firmware README and source constants before wiring a board. Pin assignments and radio settings are prototype details and may change.

## Getting started

The firmware projects use [PlatformIO](https://platformio.org/). Open `firmware/Sensor-node` or `firmware/Receiver-node` as a PlatformIO project and review its `platformio.ini` before building or flashing.

The training script is in [`ML/machine_learning.py`](ML/machine_learning.py), with dataset splits under `ML/data_csv/`. It currently expects to be run from the `ML` directory and requires PyTorch, pandas, and NumPy.

## Limitations and next steps
- The datasets are small and include synthetic background data; reported accuracy is not field performance.
- Hardware and radio behaviour depend on the exact board, module configuration, wiring, and regional requirements.
- The project still needs broader field data, repeatable evaluation, enclosure validation, power measurements, and end-to-end receiver testing.
- Open development items are tracked in [`docs/TODO.md`](docs/TODO.md).

## Contributors
- **Niklas Etling** - sensor integration, embedded development, and energy/software optimisation
- **Margarita Filenko** - machine learning, integration, and general development
- **Arsenii Marchenko** - LoRa communication, data logging, and embedded development

## License

No license has been declared yet. Until one is added, assume that the contents are not licensed for reuse beyond the permissions granted by the repository host.
