# ESP32-S3 SD Card, USB MSC, LCD Clock, Wi-Fi, NTP, Telegram Bot, and Web Server

## UCSC Silicon Valley - Embedded Firmware Essentials Week 5 Assignment

This project builds on the Week 4 SD/USB/LCD clock and adds Wi-Fi connectivity,
NTP time synchronization, a Telegram Bot for receiving messages and photos, and
an HTTP web server to display the received content.

---

## Features

### Week 4 Features (existing)

#### 1. SD Card FAT File System
- Mount SD card via SDMMC interface (1-line mode)
- Read, write, and modify files using FAT file system
- Directory listing support
- Card information display (size, type, speed)

#### 2. USB Mass Storage (MSC)
- Expose SD card to PC as removable drive
- Direct sector access (no POSIX) for USB operations
- Uses TinyUSB stack
- PC can browse, create, modify, and delete files

#### 3. I2C LCD Clock Display
- 16x2 HD44780-compatible LCD with I2C backpack (PCF8574)
- 4-bit driver mode via I2C expander
- Cycles between 3 display modes (5 seconds each):
  - Mode 0: NTP-synced date/time with SD/NTP status
  - Mode 1: Wi-Fi connection status and IP address
  - Mode 2: Latest Telegram message (scrolling for long messages)

### Week 5 Features (new)

#### 4. Wi-Fi Station Mode
- Connects to hidden SSID "Embedded" (password: class2026-embedded)
- Uses WIFI_ALL_CHANNEL_SCAN for hidden network probe
- Auto-reconnect on disconnection (up to 10 retries)
- IP address logged to serial console and shown on LCD

#### 5. NTP Time Synchronization
- Syncs system clock from time.nist.gov via SNTP
- Timezone: America/Los_Angeles (PST8PDT with automatic DST transitions)
- Replaces Week 4 software RTC -- uses POSIX time()/localtime()
- Periodic re-sync corrects oscillator drift
- LCD shows "--:--:--" until first sync completes

#### 6. Telegram Bot Integration
- Polls the Telegram Bot API via HTTPS (long polling with 30s timeout)
- Receives text messages and stores the latest in memory
- Downloads photos (largest size) and saves to /sdcard/telegram_photo.jpg
- Runs in a dedicated FreeRTOS task with automatic retry on errors
- Thread-safe shared state via mutex

#### 7. HTTP Web Server
- Runs on port 80, accessible from any device on the same network
- Three endpoints:
  - `GET /` -- Main page showing latest Telegram message and photo (auto-refresh 10s)
  - `GET /photo` -- Serves the latest Telegram photo as JPEG (chunked transfer)
  - `GET /status` -- JSON with Wi-Fi, NTP, time, heap, uptime, last message

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
- Wi-Fi network "Embedded" (hidden SSID) available
- Telegram bot token (already configured in source)

### Build and Flash

```bash
# Set up ESP-IDF environment
get_idf

# Navigate to project directory
cd ~/esp/projects/week5/sd_usb_lcd

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

## Wi-Fi Configuration

Wi-Fi credentials are defined as macros in `main/main.c`:
```c
#define WIFI_SSID     "Embedded"          // Hidden SSID
#define WIFI_PASS     "class2026-embedded" // Network password
#define WIFI_MAX_RETRY 10                  // Reconnect attempts
```

The network uses a hidden SSID. The driver is configured with
`scan_method = WIFI_ALL_CHANNEL_SCAN` to probe for it.

---

## Telegram Bot Setup

The Telegram bot token is configured in `main/main.c`:
```c
#define TELEGRAM_TOKEN ""
```

To use your own bot:
1. Open Telegram and search for @BotFather
2. Send `/newbot` and follow the instructions
3. Copy the token and replace `TELEGRAM_TOKEN` in main.c
4. Send a message to your bot to start a conversation
5. The ESP32 polls for new messages every 30 seconds

Received text messages appear on the web page and LCD.
Photos are saved to `/sdcard/telegram_photo.jpg` and served via the web server.

---

## Accessing the Web Server

1. Connect your computer/phone to the same Wi-Fi network
2. Find the ESP32's IP address in the serial monitor output
   (also shown on the LCD in Mode 1)
3. Open a browser and navigate to:
   - `http://<ESP32_IP>/` -- Main page with Telegram content
   - `http://<ESP32_IP>/photo` -- Latest photo (JPEG)
   - `http://<ESP32_IP>/status` -- JSON device status

The main page auto-refreshes every 10 seconds.

---

## Usage

### Serial Console Output
On boot, the ESP32-S3 will:
1. Initialize NVS flash
2. Create synchronization mutex
3. Mount the SD card and display card info
4. Perform test file operations (write, read, append)
5. Initialize I2C and LCD display
6. Connect to Wi-Fi (hidden SSID "Embedded")
7. Synchronize time via NTP (time.nist.gov)
8. Initialize USB Mass Storage
9. Start Telegram Bot polling task
10. Start HTTP web server on port 80
11. Start LCD clock display task (multi-mode)

### USB Mass Storage
1. Connect the USB/OTG port (separate from UART) to PC
2. PC will detect "SD Card Reader" (VID:303a, PID:4002)
3. Browse and manage files on the SD card

### LCD Display Modes (auto-cycles every 5 seconds)
```
Mode 0 - Date and Time:
  Row 0: MM/DD SD:Y NT:Y
  Row 1:     HH:MM:SS

Mode 1 - Wi-Fi Status:
  Row 0: WiFi: Connected
  Row 1: 192.168.1.100

Mode 2 - Telegram Message:
  Row 0: Telegram:
  Row 1: Hello from bot!
```
- SD:Y/N = SD card mounted status
- NT:Y/N = NTP time synchronized status
- Messages longer than 16 chars scroll horizontally

---

## Project Structure

```
sd_usb_lcd/
+-- CMakeLists.txt          # Project CMake configuration
+-- README.md               # This file
+-- sdkconfig               # ESP-IDF SDK configuration
+-- sdkconfig.defaults      # Default config (large partition, TinyUSB, etc.)
+-- AI_Interaction.md       # AI interaction documentation
+-- main/
    +-- CMakeLists.txt      # Main component CMake (all REQUIRES listed)
    +-- idf_component.yml   # Managed component dependencies (TinyUSB)
    +-- main.c              # Application source (~4700 lines with comments)
+-- managed_components/
    +-- espressif__esp_tinyusb/  # TinyUSB ESP-IDF wrapper
    +-- espressif__tinyusb/      # TinyUSB core library
```

---

## Configuration Options

### menuconfig / sdkconfig Settings
Key settings enabled in `sdkconfig`:
- `CONFIG_ESP_WIFI_ENABLED=y` -- Wi-Fi for ESP32-S3
- `CONFIG_LWIP_SNTP_MAX_SERVERS=1` -- SNTP client for NTP
- `CONFIG_ESP_HTTP_CLIENT_ENABLE_HTTPS=y` -- HTTPS for Telegram API
- `CONFIG_ESP_TLS_USING_MBEDTLS=y` -- TLS support
- `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y` -- Full CA certificate bundle
- `CONFIG_TINYUSB_MSC_ENABLED=y` -- USB Mass Storage Class
- `CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y` -- 1500KB app partition
- `CONFIG_FATFS_LFN_HEAP=y` -- Long filename support
- `CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y` -- 4MB flash

### Component Dependencies (main/CMakeLists.txt)
```
REQUIRES driver fatfs sdmmc esp_wifi esp_event esp_netif nvs_flash
         esp_http_client esp-tls json esp_http_server esp_timer
```

### Customization
Edit defines in `main/main.c`:
- `WIFI_SSID` / `WIFI_PASS`: Wi-Fi network credentials
- `TELEGRAM_TOKEN`: Telegram Bot API token
- `NTP_SERVER`: NTP server hostname (default: time.nist.gov)
- `LCD_I2C_ADDR`: LCD address (0x27 or 0x3F)
- `SD_MMC_*_GPIO`: SD card SDMMC pins
- `I2C_SDA_GPIO` / `I2C_SCL_GPIO`: I2C bus pins
- `DISPLAY_MODE_DURATION_MS`: LCD mode cycle time (default: 5000ms)

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

### Wi-Fi Won't Connect
- Verify the "Embedded" AP is powered on and in range
- Check credentials match (case-sensitive SSID/password)
- Serial log shows retry count -- watch for "WIFI_EVENT_STA_DISCONNECTED"
- The hidden SSID requires all-channel scanning (already configured)

### NTP Time Shows "--:--:--"
- Time placeholder shown before first NTP sync (typically 5-15 seconds)
- Ensure Wi-Fi is connected first (NTP needs network)
- Check serial log for "SNTP: time synchronized" message
- Verify time.nist.gov is reachable from the network

### Telegram Bot Not Receiving Messages
- Verify the bot token is correct
- Send `/start` to the bot first in Telegram
- Check serial log for HTTP response codes (200 = OK, 401 = bad token)
- The bot uses HTTPS -- TLS certificate bundle must be enabled

### Web Page Not Loading
- Verify your device is on the same Wi-Fi network
- Check the ESP32's IP address in serial log or LCD Mode 1
- Try http://<IP>/ (not https)
- Check serial log for "HTTP server started" message

### Binary Too Large / Partition Overflow
- sdkconfig.defaults includes CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y
- This provides a 1500KB app partition (default is only 1MB)
- If re-running menuconfig, ensure this setting is preserved

### DMA Memory Errors
- Reduce concurrent operations
- LCD timing uses microsecond delays to avoid FreeRTOS overhead

---

## Success Criteria Checklist

### Week 4 -- SD Card FAT File System
- [x] SD card mounts successfully with FAT file system
- [x] File create operation works
- [x] File read operation works
- [x] File modify/append operation works
- [x] Directory listing works
- [x] Card info is displayed

### Week 4 -- USB Mass Storage
- [x] USB MSC initializes without errors
- [x] PC recognizes ESP32-S3 as removable drive
- [x] PC can browse files on SD card
- [x] PC can create/modify/delete files
- [x] No POSIX used in MSC implementation (direct sector access)

### Week 4 -- LCD Clock Display
- [x] I2C LCD initializes correctly
- [x] 1k Ohm pull-up resistors used on I2C lines
- [x] 4-bit LCD driver used as per assignment
- [x] Date is displayed in readable format
- [x] Time updates every second

### Week 5 -- Wi-Fi Connectivity
- [x] Connects to hidden SSID "Embedded" with correct password
- [x] Auto-reconnect on disconnection (up to 10 retries)
- [x] IP address obtained and logged to serial console
- [x] Wi-Fi status displayed on LCD (Mode 1)

### Week 5 -- NTP Time Synchronization
- [x] Time synced from time.nist.gov
- [x] Timezone set to America/Los_Angeles (PST/PDT)
- [x] DST transitions handled via POSIX TZ string
- [x] LCD displays accurate local time (Mode 0)
- [x] Periodic re-sync corrects drift

### Week 5 -- Telegram Bot Integration
- [x] Bot polls getUpdates via HTTPS
- [x] Text messages received and stored
- [x] Photos downloaded and saved to SD card
- [x] Runs in dedicated FreeRTOS task
- [x] Thread-safe shared state via mutex

### Week 5 -- HTTP Web Server
- [x] Server runs on port 80
- [x] GET / shows latest Telegram message and photo
- [x] GET /photo serves JPEG from SD card
- [x] GET /status returns JSON with device info
- [x] Page auto-refreshes every 10 seconds

### Week 5 -- Integration
- [x] NVS flash initialized first for Wi-Fi
- [x] LCD cycles between 3 display modes
- [x] All tasks run concurrently without crashes
- [x] Build fits in 1500KB partition (25% free)

---

## License

This project was created for educational purposes as part of the UCSC Silicon Valley Embedded Firmware Essentials course.

---

## Author

Generated with AI assistance for Week 4 and Week 5 Assignments (February 2026)
