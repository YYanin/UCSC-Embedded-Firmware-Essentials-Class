# ESP32-S3 SD Card, USB Mass Storage, and LCD Clock Display

## UCSC Silicon Valley - Embedded Firmware Essentials Week 4 Assignment

This project demonstrates SD card file management, USB Mass Storage, and I2C LCD clock display on the ESP32-S3 platform.

---

## Features

### 1. SD Card FAT File System
- Mount SD card via SDMMC interface (1-line mode)
- Read, write, and modify files using FAT file system
- Directory listing support
- Card information display (size, type, speed)

### 2. USB Mass Storage (MSC)
- Expose SD card to PC as removable drive
- Direct sector access (no POSIX) for USB operations
- Uses TinyUSB stack
- PC can browse, create, modify, and delete files

### 3. I2C LCD Clock Display
- 16x2 HD44780-compatible LCD with I2C backpack (PCF8574)
- 4-bit driver mode via I2C expander
- Displays date, time, and status indicators
- Updates every second

---

## Hardware Requirements

### Main Components
| Component | Description |
|-----------|-------------|
| ESP32-S3-WROOM | Freenove development board with onboard SD slot |
| microSD Card | FAT32 formatted |
| I2C LCD 16x2 | HD44780 with PCF8574 backpack (address 0x27) |
| 1k Ohm Resistors (x2) | I2C pull-ups for SDA and SCL |

### GPIO Pin Configuration

#### SDMMC Interface (Onboard SD Card Slot)
| Signal | GPIO | Notes |
|--------|------|-------|
| CMD | GPIO 38 | Command/response |
| CLK | GPIO 39 | Clock |
| D0 | GPIO 40 | Data line 0 (1-line mode) |

#### I2C Interface (LCD)
| Signal | GPIO | Pull-up |
|--------|------|---------|
| SDA | GPIO 8 | 1k Ohm to 3.3V |
| SCL | GPIO 9 | 1k Ohm to 3.3V |

---

## Wiring Diagram

```
                        ESP32-S3 Freenove
                    +-------------------+
                    |                   |
  [SD Card Slot]    |  (Onboard SDMMC)  |
  Built-in on board |  CMD=38 CLK=39   |
                    |  D0=40            |
                    |                   |
                    |   GPIO 8 (SDA) ---+--[1k]--+-- 3.3V
                    |                   |        |
                    |   GPIO 9 (SCL) ---+--[1k]--+
                    |                   |
                    +-------------------+
                           |   |
                           |   |
                    +------+---+------+
                    |  I2C LCD 16x2   |
                    |  PCF8574 0x27   |
                    +-----------------+
```

---

## Build Instructions

### Prerequisites
- ESP-IDF v5.3 or later
- ESP32-S3 toolchain configured

### Build and Flash

```bash
# Set up ESP-IDF environment
get_idf

# Navigate to project directory
cd ~/esp/projects/week4/sd_usb_lcd

# Set target (if not already set)
idf.py set-target esp32s3

# Build the project
idf.py build

# Flash and monitor (use UART port, not USB/OTG)
idf.py -p /dev/ttyACM0 flash monitor
```

### Exit Monitor
Press `Ctrl+]` to exit the serial monitor.

---

## Usage

### Serial Console Output
On boot, the ESP32-S3 will:
1. Initialize I2C and scan for devices
2. Mount the SD card and display card info
3. Perform test file operations (write, read, append)
4. Initialize USB Mass Storage
5. Start the LCD clock display task

### USB Mass Storage
1. Connect the USB/OTG port (separate from UART) to PC
2. PC will detect "SD Card Reader" (VID:303a, PID:4002)
3. Browse and manage files on the SD card

### LCD Display Format
```
Row 0: MM/DD SD:Y USB:N
Row 1:     HH:MM:SS
```
- SD:Y/N = SD card mounted status
- USB:Y/N = USB MSC connection status

---

## Project Structure

```
sd_usb_lcd/
+-- CMakeLists.txt          # Project CMake configuration
+-- README.md               # This file
+-- sdkconfig               # ESP-IDF SDK configuration
+-- main/
    +-- CMakeLists.txt      # Main component CMake
    +-- main.c              # Application source (~2500 lines with comments)
```

---

## Configuration Options

### menuconfig Settings
Key settings enabled in `sdkconfig`:
- FATFS component for FAT file system
- TinyUSB stack for USB MSC
- SDMMC peripheral support
- I2C driver

### Customization
Edit defines in `main/main.c`:
- `LCD_I2C_ADDR`: Change if LCD uses different address (0x27 or 0x3F)
- `SD_MMC_*_GPIO`: Adjust if using different board with different SD pins
- `I2C_SDA_GPIO` / `I2C_SCL_GPIO`: Change I2C pins if needed

---

## Troubleshooting

### SD Card Not Detected
- Verify SD card is FAT32 formatted
- Check SDMMC GPIO connections (pins 38, 39, 40 for Freenove)
- Try a different SD card

### LCD Shows Garbage or Nothing
- Verify I2C address (use I2C scanner)
- Check 1k pull-up resistors on SDA and SCL
- Ensure no other devices conflict on I2C pins

### USB MSC Not Appearing on PC
- Use the USB/OTG port, not the UART/programming port
- Check USB cable supports data (not charge-only)
- Verify TinyUSB is enabled in menuconfig

### DMA Memory Errors
- Reduce concurrent operations
- LCD timing uses microsecond delays to avoid FreeRTOS overhead

---

## Success Criteria Checklist

### SD Card FAT File System
- [x] SD card mounts successfully with FAT file system
- [x] File create operation works
- [x] File read operation works
- [x] File modify/append operation works
- [x] Directory listing works
- [x] Card info is displayed

### USB Mass Storage
- [x] USB MSC initializes without errors
- [x] PC recognizes ESP32-S3 as removable drive
- [x] PC can browse files on SD card
- [x] PC can create/modify/delete files
- [x] No POSIX used in MSC implementation (direct sector access)

### LCD Clock Display
- [x] I2C LCD initializes correctly
- [x] 1k Ohm pull-up resistors used on I2C lines
- [x] 4-bit LCD driver used as per assignment
- [x] Date is displayed in readable format
- [x] Time updates every second

---

## License

This project was created for educational purposes as part of the UCSC Silicon Valley Embedded Firmware Essentials course.

---

## Author

Generated with AI assistance for Week 4 Assignment (February 2026)
