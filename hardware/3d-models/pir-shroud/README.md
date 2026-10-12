# PIR Sensor Field-of-View Restrictor Shroud
This documentation details the design and purpose for the 3D-printed PIR sensor shroud.

## 1. Purpose and System Architecture
The edge node operates entirely off-grid using batteries. To optimize energy consumption and mitigate the risk of short battery life, the system relies on deep sleep cycles.
* The ESP32 S3 microcontroller remains in deep sleep until the PIR sensor EKMC1603111 detects motion and triggers a GPIO interrupt.
* Upon waking, the system requests data from the MLX90640 thermal sensor module to run classification algorithms before transmitting alerts via a LoRa RYLR 890 module
* Without this shroud, the PIR sensor's native 120° field of view (FOV) is much wider than the thermal camera's 55°x35° FOV. This mismatch causes "ghost wakes" where the PIR detects animals the camera cannot see, resulting in empty processing cycles that drain power. The shroud physically restricts the PIR's vision to perfectly overlap with the thermal camera.

## 2. Mechanical Design Specifications
The shroud is designed as a tapered loft featuring a circular mounting base that transitions into a precise rectangular front window. 
* **Total Depth:** 1.13 cm extending outward from the main device casing.
* **Window Width:** 1.82 cm (calculated to restrict the horizontal FOV to exactly 55°).
* **Window Height:** 0.86 cm (calculated to restrict the vertical FOV to exactly 35°).
* **Geometry:** The front window utilizes straight edges and flat inner walls, directly matching the rectangular pixel grid of the MLX90640 to prevent peripheral blind spots.
