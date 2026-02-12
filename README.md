# UCSC-Embedded-Firmware-Essentials-Class

A collection of ESP32-S3 projects developed for the UCSC Embedded Firmware Essentials class.

## Directory Structure

```
projects/
├── esp32-shell/            # Interactive shell for ESP32-S3
├── led_sequence/           # LED chasing/running light demo
├── seven_segment_counter/  # 7-segment display counter
├── week1/                  # Week 1 class materials
│   └── EmbeddedFirmwareEssentialsClass/
│       ├── README.md
│       ├── IMG_3892.MOV
│       ├── IMG_3893.MOV
│       └── IMG_3894.MOV
└── week3/                  # Week 3 class materials
    └── task_priority/      # FreeRTOS task priority demo
```

## Project Descriptions

### esp32-shell/
An interactive command-line shell for ESP32-S3 microcontrollers. Provides a Unix-like interface over USB serial console with support for file system operations and device commands.

### led_sequence/
Demonstrates GPIO control on the ESP32-S3 by blinking multiple LEDs in a sequential pattern. Creates a "chasing" or "running light" effect using 4 LEDs connected to GPIO pins 4-7.

### seven_segment_counter/
Drives a 7-segment display to count from 0 to 9 repeatedly. Demonstrates GPIO control and supports both common cathode and common anode display types.

### week1/
Contains class materials and resources for Week 1, including documentation and video demonstrations in the `EmbeddedFirmwareEssentialsClass/` subdirectory.

### week3/
Contains class materials for Week 3. The `task_priority/` subdirectory contains a FreeRTOS project demonstrating task scheduling, priority preemption, and semaphore-based synchronization with four concurrent tasks.

## Hardware Requirements

- **Board**: ESP32-S3-DevKitC-1 (or compatible)
- **Chip**: ESP32-S3
- USB cable for programming and serial communication

## Software Requirements

- **ESP-IDF**: v5.3.2 or later
- **Python**: 3.8+
- **CMake**: 3.16+
- **Ninja**: Build system
