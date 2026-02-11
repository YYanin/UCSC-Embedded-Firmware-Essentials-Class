# Espressif ESP-IDF Setup Guide for ESP32-S3

This document outlines the main steps for setting up the Espressif IDF (IoT Development Framework) environment to work with ESP32-S3 devices.

---

## Prerequisites

- Ubuntu/Debian Linux (or WSL2 on Windows)
- Git
- Python 3.8 or newer
- At least 4GB of free disk space

---

## Step 1: Install System Dependencies

Install required packages for building ESP-IDF projects:

```bash
sudo apt-get update
sudo apt-get install -y git wget flex bison gperf python3 python3-pip python3-venv \
    cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
```

---

## Step 2: Clone ESP-IDF Repository

Create a directory for ESP-IDF and clone the repository:

```bash
mkdir -p ~/esp
cd ~/esp
git clone -b v5.3.2 --recursive https://github.com/espressif/esp-idf.git
```

Note: The `--recursive` flag is required to fetch all submodules.

---

## Step 3: Install ESP-IDF Tools

Run the install script to download and set up the toolchain for ESP32-S3:

```bash
cd ~/esp/esp-idf
./install.sh esp32s3
```

This installs:
- Xtensa cross-compiler (xtensa-esp-elf-gcc)
- OpenOCD for debugging
- Python dependencies in a virtual environment
- esptool.py for flashing

---

## Step 4: Set Up Environment Variables

Add the ESP-IDF export script to your shell configuration:

```bash
echo 'alias get_idf=". $HOME/esp/esp-idf/export.sh"' >> ~/.bashrc
source ~/.bashrc
```

Now you can activate ESP-IDF in any terminal by typing:

```bash
get_idf
```

Or manually source the export script:

```bash
. ~/esp/esp-idf/export.sh
```

---

## Step 5: Verify Installation

Test the installation by building the hello_world example:

```bash
get_idf
cd ~/esp/esp-idf/examples/get-started/hello_world
idf.py set-target esp32s3
idf.py build
```

If the build completes successfully, the environment is ready.

---

## Step 6: Connect ESP32-S3 Device

1. Connect ESP32-S3 via USB cable
2. Check the serial port:
   ```bash
   ls /dev/ttyACM* /dev/ttyUSB*
   ```
3. Common ports: `/dev/ttyACM0` or `/dev/ttyUSB0`

If permission denied, add your user to the dialout group:

```bash
sudo usermod -a -G dialout $USER
```

Then log out and log back in.

---

## Step 7: Flash and Monitor

Flash firmware to the device:

```bash
idf.py -p /dev/ttyACM0 flash
```

Open serial monitor to see output:

```bash
idf.py -p /dev/ttyACM0 monitor
```

Combined flash and monitor:

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

Exit monitor with: `Ctrl+]`

---

## Common idf.py Commands

| Command | Description |
|---------|-------------|
| `idf.py set-target esp32s3` | Set target chip to ESP32-S3 |
| `idf.py menuconfig` | Open configuration menu |
| `idf.py build` | Build the project |
| `idf.py flash` | Flash to device |
| `idf.py monitor` | Open serial monitor |
| `idf.py flash monitor` | Flash and open monitor |
| `idf.py clean` | Clean build artifacts |
| `idf.py fullclean` | Remove all build files |
| `idf.py size` | Show firmware size analysis |
| `idf.py size-components` | Show size by component |

---

## Project Structure

A minimal ESP-IDF project requires:

```
my_project/
    CMakeLists.txt          # Project-level CMake config
    main/
        CMakeLists.txt      # Component CMake config
        main.c              # Application entry point
    sdkconfig.defaults      # Default configuration options
```

### Project CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(my_project)
```

### Component CMakeLists.txt (main/)

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer
)
```

### Minimal main.c

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void) {
    printf("Hello from ESP32-S3!\n");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

---

## Troubleshooting

### Build Errors: Missing Headers

If you get "No such file or directory" errors, add the required component to REQUIRES in your component's CMakeLists.txt:

```cmake
REQUIRES driver esp_timer spi_flash esp_system
```

### Flash Errors: Permission Denied

```bash
sudo chmod 666 /dev/ttyACM0
# Or add user to dialout group (permanent fix)
sudo usermod -a -G dialout $USER
```

### Flash Errors: Device Not Found

- Check USB cable (some cables are charge-only)
- Try a different USB port
- Hold BOOT button while pressing RESET, then release

### Monitor Shows Garbage Characters

Ensure baud rate matches (default: 115200):

```bash
idf.py -p /dev/ttyACM0 -b 115200 monitor
```

---

## Additional Resources

- ESP-IDF Programming Guide: https://docs.espressif.com/projects/esp-idf/en/v5.3.2/esp32s3/
- ESP32-S3 Technical Reference: https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf
- ESP-IDF GitHub: https://github.com/espressif/esp-idf

---

## Version Information

- ESP-IDF Version: v5.3.2
- Target: ESP32-S3
- Toolchain: xtensa-esp-elf-gcc 13.2.0
- Last Updated: January 2026
