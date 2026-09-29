# Environmental Monitoring to Prevent Road Hazards Project

## Problem Statement
Nowadays, hazards on the roads caused by wild animals is an important problem. A lot of car crashes happen especially at nighttime and at the narrow roads. Even with existing technologies it is not possible to fully exclude the danger. Thousands of people suffer from this problem every year. Statistically, 122 cases of these accidents are leading to personal injuries and roughly 3 fatalities annually. These facts highlight the necessity of detecting wild animals near roads and informing road users about possible threats nearby.

## Project Goal
Create a weatherproof device with thermal camera, distance sensor (PIR) and LoRa transmitter for communication with LED Possible additions include dashboard for displaying alerts. The MVP would be an energy efficient device that can correctly identify hazardous animals and transmit alerts via LoRa.

## What should the system do?
- Collect sensor data
- Process edge data
- Classify hazardous animals 
- Generate Alerts
- Low latency communication

## System Architecture

<img width="920" height="473" alt="image" src="https://github.com/user-attachments/assets/ecb367cb-c659-4261-bcc4-04f6c79234f3" />

##  Technologies Used
- ESP32 S3 (Receiver and Transmitter)
- MLX96040 THERMAL SENSOR MODULE
- Lora RYLR 890 x2
- SKU 101020060 PIR sensor
- CR123A x 6 batteries

## Team members
* Niklas Etling– 
    - Sensor integration
    - Energy/software optimization
    - General Embedded development
* Margarita Filenko – 
    - AI development 
    - Integration
    - General development
* Arsenii Marchenko– 
    - LoRa Communication 
    - Data logging
    - General Embedded development
