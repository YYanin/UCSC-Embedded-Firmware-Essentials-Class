
## 2026-02-17 23:23 - week4Prompts.md Creation

### Request
User requested creation of week4Prompts.md file following the same structure as week3Prompts.md.

### Actions Taken
1. Read AgentConstraints.md to understand constraints
2. Read Assignment.md to understand Week 4 goals:
   - Mount SD card with FAT file system read/write/modify
   - Implement USB Mass Storage (no POSIX)
   - Implement clock display on I2C LCD (4-bit driver, 1k Ohm pull-ups)
   - Optional: Servo controlled by accelerometer
3. Read week3Prompts.md to understand the structure:
   - Summary section
   - Multiple phases with prompts and manual verification tests
   - Success criteria
   - Additional notes
4. Read task_priority/README.md for hardware info
5. Created week4Prompts.md with 10 phases:
   - Phase 1: Project Setup and SD Card Hardware Configuration
   - Phase 2: SD Card Initialization and FAT File System Mounting
   - Phase 3: FAT File System Read/Write/Modify Operations
   - Phase 4: USB Mass Storage (MSC) Implementation
   - Phase 5: I2C LCD Display Setup
   - Phase 6: Real-Time Clock Implementation
   - Phase 7: Main Application Integration
   - Phase 8: Build, Flash, and Test
   - Phase 9: Optional - Servo Control with Accelerometer
   - Phase 10: Documentation and Submission

### Result
DONE - week4Prompts.md created successfully at /home/nordiffico/esp/projects/week4/week4Prompts.md


## 2026-02-17 23:33 - Phase 1: Project Setup and SD Card Hardware Configuration

### Request
User requested to proceed with Phase 1 of week4Prompts.md

### Actions Taken (Prompt 1.1, 1.2, 1.3)
1. Created project directory structure:
   - ~/esp/projects/week4/sd_usb_lcd/
   - ~/esp/projects/week4/sd_usb_lcd/main/

2. Created CMakeLists.txt files:
   - Project CMakeLists.txt with cmake_minimum_required and project() calls
   - main/CMakeLists.txt with idf_component_register() requiring driver, fatfs, sdmmc

3. Created main/main.c with:
   - SD Card SPI GPIO definitions:
     - MOSI: GPIO 11
     - MISO: GPIO 13
     - CLK: GPIO 12
     - CS: GPIO 10
     - SPI Host: SPI2_HOST
   - I2C LCD GPIO definitions:
     - SDA: GPIO 8
     - SCL: GPIO 9
     - Note: 1k Ohm pull-up resistors required
     - I2C address: 0x27
   - Optional servo/accelerometer GPIO definitions
   - USB MSC configuration placeholders
   - Function prototypes for all modules
   - Detailed comments explaining all configurations

4. Configured ESP32-S3 target:
   - Ran idf.py set-target esp32s3
   - sdkconfig generated
   - Build successful with only unused variable warnings (expected at this stage)

### GPIO Pin Assignment Summary
| Function  | GPIO           | Notes                           |
|-----------|---------------|---------------------------------|
| SD MOSI   | GPIO 11       | SPI data to SD card             |
| SD MISO   | GPIO 13       | SPI data from SD card           |
| SD CLK    | GPIO 12       | SPI clock                       |
| SD CS     | GPIO 10       | SPI chip select                 |
| I2C SDA   | GPIO 8        | LCD data (needs 1k pull-up)     |
| I2C SCL   | GPIO 9        | LCD clock (needs 1k pull-up)    |
| Servo PWM | GPIO 18       | Optional servo control          |
| Button    | GPIO 0        | From Week 3 (BOOT button)       |
| LEDs      | GPIO 4-7      | From Week 3                     |

### Files Created
- /home/nordiffico/esp/projects/week4/sd_usb_lcd/CMakeLists.txt
- /home/nordiffico/esp/projects/week4/sd_usb_lcd/main/CMakeLists.txt
- /home/nordiffico/esp/projects/week4/sd_usb_lcd/main/main.c
- /home/nordiffico/esp/projects/week4/sd_usb_lcd/sdkconfig (auto-generated)

### Result
DONE - Phase 1 completed successfully. Project builds without errors.


## 2026-02-17 23:40 - Phase 2: SD Card Initialization and FAT File System Mounting

### Request
User requested to proceed with Phase 2 of week4Prompts.md

### Actions Taken (Prompt 2.1, 2.2, 2.3)

1. Implemented sd_card_init() function:
   - Configures SPI bus with spi_bus_initialize()
   - Uses SPI2_HOST with DMA for efficient data transfer
   - Configures SD card slot with SDSPI_DEVICE_CONFIG_DEFAULT()
   - Sets up FAT mount configuration:
     - format_if_mount_failed = false (preserve data)
     - max_files = 5
     - allocation_unit_size = 16KB
   - Calls esp_vfs_fat_sdspi_mount() to mount at /sdcard
   - Detailed comments explain each step

2. Implemented sd_card_print_info() function:
   - Displays card name from CID register
   - Detects card type (SDSC/SDHC/SDXC) from OCR register
   - Calculates and displays capacity in MB/GB
   - Shows sector size and count (for USB MSC)
   - Shows max speed and CSD version
   - Confirms mount point

3. Implemented sd_card_unmount() function:
   - Calls esp_vfs_fat_sdcard_unmount() to safely unmount
   - Frees SPI bus resources
   - Resets sd_card pointer

4. Updated app_main():
   - Creates sd_access_mutex for USB/local access coordination
   - Calls sd_card_init()
   - Provides helpful error message with GPIO pins on failure

5. Added sd_protocol_defs.h include for SD_OCR_SDHC_CAP macro

### Build Result
DONE - Build successful with only expected warnings (undefined functions for future phases)

### Functions Implemented
- sd_card_init(): SPI init + FAT mount
- sd_card_print_info(): Display card information
- sd_card_unmount(): Safe unmount and cleanup

### Next Steps
Phase 3: Implement file read/write/modify operations


## 2026-02-17 23:50 - Phase 3: FAT File System Read/Write/Modify Operations

### Request
User requested to proceed with Phase 3 of week4Prompts.md

### Actions Taken (Prompt 3.1, 3.2, 3.3, 3.4)

1. Implemented file_write() function:
   - Opens file with fopen() in 'w' mode (create/truncate)
   - Writes content using fprintf()
   - Includes USB MSC conflict check
   - Proper error handling and file closing
   - Detailed comments explaining write mode behavior

2. Implemented file_read() function:
   - Opens file with fopen() in 'r' mode
   - Reads line by line using fgets() with 256-byte buffer
   - Prints contents to serial console
   - Handles file not found error gracefully
   - Detailed comments explaining read process

3. Implemented file_append() function:
   - Opens file with fopen() in 'a' mode (append)
   - Creates file if it doesn't exist
   - Preserves existing content, adds new content at end
   - Detailed comments explaining append vs write modes

4. Implemented directory_list() function:
   - Uses opendir()/readdir()/closedir() POSIX functions
   - Uses stat() to get file sizes
   - Distinguishes files vs directories with [FILE]/[DIR] prefixes
   - Formats sizes in bytes/KB/MB as appropriate
   - Shows total count of files and directories
   - Handles empty directories

5. Created file_operations_demo() function:
   - Demonstrates all file operations in sequence:
     - Step 1: List initial directory
     - Step 2: Create test.txt with sample content
     - Step 3: Read file back
     - Step 4: Append additional content
     - Step 5: Read again to verify append
     - Step 6: List directory showing new file

6. Updated app_main() to call file_operations_demo()

### USB MSC Conflict Prevention
All file operations check usb_msc_active flag before proceeding to prevent
file system corruption when PC is accessing SD card via USB.

### Build Result
DONE - Build successful with only expected warnings (undefined functions for future phases)

### Functions Implemented
- file_write(): Create/overwrite file with content
- file_read(): Read and display file contents
- file_append(): Append content to file
- directory_list(): List directory contents with sizes
- file_operations_demo(): Demonstrate all operations

### Next Steps
Phase 4: Implement USB Mass Storage (MSC)


## 2026-02-18 00:23 - Phase 4: USB Mass Storage (MSC) Implementation

### Request
User requested to proceed with Phase 4 of week4Prompts.md

### Actions Taken (Prompt 4.1, 4.2, 4.3, 4.4)

1. Added esp_tinyusb component dependency:
   - Updated main/idf_component.yml with espressif/esp_tinyusb dependency
   - Added tinyusb.h and tusb_msc_storage.h includes

2. Implemented USB MSC configuration:
   - USB device descriptor with VID/PID (0x303A/0x4002 - Espressif)
   - USB configuration descriptor with MSC interface
   - String descriptors (manufacturer, product, serial)
   - Detailed comments explaining each descriptor

3. Implemented USB MSC initialization (usb_msc_init):
   - Uses tinyusb_msc_storage_init_sdmmc() with our SPI-based SD card
   - Internally uses sdmmc_read_sectors/sdmmc_write_sectors (NO POSIX)
   - Installs TinyUSB driver with MSC configuration

4. Implemented mount state callback (usb_msc_mount_changed_cb):
   - Sets usb_msc_active flag when PC mounts storage
   - Clears flag when PC unmounts/ejects
   - Logs mount state changes

5. Implemented access coordination functions:
   - usb_msc_expose_to_host(): Unmount local, allow USB access
   - usb_msc_mount_locally(): Remount for ESP32 local access
   - usb_msc_task(): Optional task for status monitoring

6. Created sdkconfig.defaults with required settings:
   - CONFIG_TINYUSB_MSC_ENABLED=y
   - CONFIG_FATFS_LFN_HEAP=y
   - Console configuration for UART (USB used for MSC)

7. Updated app_main():
   - Unmounts FAT from Phase 2 before USB MSC init
   - Calls usb_msc_init()
   - Shows USB connection instructions

### USB MSC Implementation Details

The implementation uses esp_tinyusb component which handles TinyUSB callbacks internally:
- tud_msc_capacity_cb() - Reports SD card sector count and size
- tud_msc_read10_cb() - Reads sectors using sdmmc_read_sectors()
- tud_msc_write10_cb() - Writes sectors using sdmmc_write_sectors()
- tud_msc_scsi_cb() - Handles SCSI commands (INQUIRY, TEST UNIT READY, etc.)

This approach satisfies the 'no POSIX' requirement - USB operations use direct sector access, not file I/O.

### Files Modified
- main/main.c: Added USB MSC implementation
- main/idf_component.yml: Added esp_tinyusb dependency
- sdkconfig.defaults: Created with USB MSC configuration

### Build Result
DONE - Build successful (sd_usb_lcd.bin: 0x58590 bytes, 65% free)

### Next Steps
Phase 5: Implement I2C LCD display


## Phase 5: I2C LCD Display Setup - COMPLETED

### Prompt 5.1 - I2C Interface Configuration
- Defined I2C GPIO pins (SDA=GPIO 8, SCL=GPIO 9)
- Added detailed comments about 1k Ohm external pull-up resistor requirement
- Added hardware wiring diagrams in comments showing pull-up connections
- Implemented i2c_init() with i2c_param_config() and i2c_driver_install()
- Configured I2C clock speed at 100kHz (standard for LCD)
- Enabled internal pull-ups as backup (external 1k required)

### Prompt 5.2 - 4-bit LCD Driver Integration
- Implemented PCF8574 I2C backpack communication
- Defined bit mapping: RS(bit0), RW(bit1), E(bit2), BL(bit3), D4-D7(bits4-7)
- Implemented HD44780 initialization sequence for 4-bit mode
- Documented timing requirements from HD44780 datasheet
- Configured for 16x2 LCD dimensions (LCD_COLS=16, LCD_ROWS=2)
- LCD I2C address set to 0x27 (common PCF8574 address)

### Prompt 5.3 - LCD Functions Implemented
- lcd_clear(): Sends clear command (0x01) with proper delay
- lcd_set_cursor(row, col): Sets DDRAM address using row offset table
- lcd_print(text): Sends ASCII characters as data (RS=1)
- lcd_backlight(on/off): Controls bit 3 of PCF8574 output

### Helper Functions
- lcd_i2c_write_byte(): Sends byte to PCF8574 via I2C
- lcd_pulse_enable(): Generates enable pulse for data latch
- lcd_write_nibble(): Sends 4-bit nibble in 4-bit mode
- lcd_write_byte(): Sends byte as two nibbles (high first)
- lcd_send_command(): Sends command byte (RS=0)
- lcd_send_data(): Sends data byte (RS=1)

### Build Status
DONE - Build successful, 65% free space


## Phase 6: Real-Time Clock Implementation - COMPLETED

### Prompt 6.1 - Software RTC with FreeRTOS
- Created datetime_t structure (year, month, day, hour, minute, second)
- Implemented FreeRTOS timer (rtc_timer) with 1-second period
- Timer callback (rtc_timer_callback) increments time each second
- Implemented datetime_increment_second() with full calendar logic:
  - Second->minute->hour->day->month->year cascading
  - Days per month lookup table (28/29/30/31)
  - Leap year calculation (divisible by 4, except 100, except 400)
- is_leap_year() and get_days_in_month() helper functions
- datetime_set() for setting time with validation
- rtc_init() creates and starts the FreeRTOS timer

### Prompt 6.2 - LCD Clock Display Task
- Implemented clock_task() FreeRTOS task for LCD updates
- Formats date as YYYY-MM-DD (ISO 8601) on row 0
- Formats time as HH:MM:SS (24-hour) on row 1
- Updates only when second changes (reduces I2C traffic/flicker)
- 100ms polling interval for responsiveness
- clock_task_start() creates the task at priority 2

### Prompt 6.3 - Initial Time Setting
- Default time set in global datetime initialization (2026-02-18 12:00:00)
- rtc_load_from_sd() attempts to restore time from /sdcard/time.txt
- rtc_save_to_sd() persists time to SD card (simple text format)
- Falls back to compile-time default if no saved time found
- Validates time values before applying

### Build Status
DONE - Build successful, 65% free space


## Phase 7: Main Application Integration - COMPLETED

### Prompt 7.1 - Component Integration in app_main
- Restructured app_main() with 7-step initialization sequence
- Added detailed comments explaining startup order and dependencies
- Step 1: Create mutex for SD card access coordination
- Step 2: Initialize SD card (SPI, mount FAT)
- Step 3: Run file operations demo
- Step 4: Initialize I2C and LCD (Phase 5)
- Step 5: Initialize software RTC and try loading saved time (Phase 6)
- Step 6: Initialize USB Mass Storage (Phase 4)
- Step 7: Start clock display task (Phase 6)
- Added comprehensive error handling at each step
- Non-fatal errors log warnings and continue
- Fatal errors (SD, mutex) halt with detailed diagnostics
- LCD shows status during initialization

### Prompt 7.2 - LCD Status Indicators
- Modified clock_task() to display status alongside time
- LCD Layout (16x2):
  Row 0: "MM/DD SD:Y USB:N" - Date + SD status + USB status
  Row 1: "    HH:MM:SS    " - Time centered
- Added sd_card_initialized flag to track SD status
- Uses existing usb_msc_active flag for USB status
- Status updates immediately when state changes
- SD:Y/N shows SD card initialization status
- USB:Y/N shows USB host connection status
- Logging of status changes for debugging

### Build Status
DONE - Build successful, 61% free space (binary 0x62dd0 bytes)


## Phase 8: Build, Flash, and Test - IN PROGRESS

### Prompt 8.1 - Build and Flash
DONE - Build and flash successful
- Build completed without errors (warnings only for unused functions)
- Binary size: 0x62dd0 bytes (61% free space)
- Flash completed successfully to /dev/ttyACM0
- ESP32-S3 detected: chip revision v0.2, 8MB PSRAM
- App booted correctly, initialization sequence started

### Hardware Status
- SD Card: NOT DETECTED (ESP_ERR_TIMEOUT)
  - Requires: SD card module wired to GPIO 10-13
  - MOSI=GPIO11, MISO=GPIO13, CLK=GPIO12, CS=GPIO10
- LCD: Not yet tested (requires I2C wiring)
  - Requires: I2C LCD on GPIO 8/9 with 1k pull-ups

### Prompts 8.2-8.4 - Pending Hardware Connection
- Waiting for SD card module and LCD to be wired
- Serial monitor running, will show output when hardware connected


---

## Phase 8 - COMPLETED (Feb 18, 2026)

### Hardware Configuration Changes
- SD Card: Changed from SPI mode (GPIO 10-13) to SDMMC mode for onboard slot
  - Freenove ESP32-S3-WROOM uses SDMMC interface, not SPI
  - SDMMC pins: CMD=GPIO38, CLK=GPIO39, D0=GPIO40
  - 1-line SDMMC mode enabled

### Issues Resolved
1. SD Card Timeout (ESP_ERR_TIMEOUT)
   - Root cause: Wrong interface (SPI vs SDMMC) and wrong GPIO pins
   - Solution: Rewrote sd_card_init() to use SDMMC host instead of SPI
   - Changed from esp_vfs_fat_sdspi_mount() to esp_vfs_fat_sdmmc_mount()

2. LCD Garbage Display
   - Root cause: Servo and potentiometer were wired to I2C bus (GPIO 8/9)
   - Solution: Moved servo to GPIO 4, potentiometer to ADC pins
   - I2C bus now only has LCD (0x27) and MPU6050 (0x68)

3. USB MSC Not Mounting as Drive
   - Root cause: esp_vfs_fat_sdcard_unmount() was deinitializing SDMMC driver
   - Solution: Keep FAT mounted - USB MSC uses sector-level access which coexists

4. DMA Memory Error
   - Root cause: vTaskDelay() in LCD pulse_enable was consuming resources
   - Solution: Changed to esp_rom_delay_us() for microsecond delays

### Verification Results
- [DONE] Phase 8.1: Build and flash - Binary 0x5c4d0 bytes, 64% free
- [DONE] Phase 8.2: SD card operations
  - SD card mounted successfully (960 MB, FAT32)
  - File write/read/append operations verified
  - Directory listing works
- [DONE] Phase 8.3: USB Mass Storage
  - PC detects ESP32-S3 as "SD Card Reader" (VID:303a, PID:4002)
  - Storage accessible, files readable
- [DONE] Phase 8.4: LCD Clock Display
  - I2C LCD at address 0x27 detected
  - Date/time display updating correctly
  - Format: Row 0 = "MM/DD SD:Y USB:N", Row 1 = "    HH:MM:SS    "

### Final Hardware Configuration
- GPIO 8: I2C SDA (LCD + MPU6050) with 1k pull-up to 3.3V
- GPIO 9: I2C SCL (LCD + MPU6050) with 1k pull-up to 3.3V
- GPIO 4: Servo PWM signal
- GPIO 38/39/40: SDMMC interface (onboard SD card slot)
- ADC pins (1/2/3): Available for potentiometer

### Code Files Modified
- main/main.c: ~2544 lines with all Phase 1-8 implementations
  - SD card via SDMMC (not SPI)
  - USB MSC with TinyUSB
  - I2C LCD with 4-bit driver
  - Software RTC with FreeRTOS timer
  - Clock display task


---

## Phase 10 - Documentation and Submission (Feb 18, 2026)

### Note: Phase 9 (MPU6050 + Servo) skipped per user request

### Prompt 10.1: Demo Video Requirements
User must record video showing:
1. Hardware setup (ESP32-S3, SD card, LCD, wiring)
2. SD card mount and file operations in serial monitor
3. USB MSC drive appearing on PC
4. File create/modify from PC shown in console
5. LCD displaying date/time updating

### Prompt 10.2: GitHub Repository
Created README.md in sd_usb_lcd/ directory containing:
- Project description and features
- Hardware requirements table
- GPIO pin configuration tables
- ASCII wiring diagram
- Build and flash instructions
- Usage guide (serial output, USB MSC, LCD format)
- Project structure
- Configuration options
- Troubleshooting guide
- Success criteria checklist (all items marked complete)

### Files Ready for Submission
- sd_usb_lcd/CMakeLists.txt
- sd_usb_lcd/main/CMakeLists.txt
- sd_usb_lcd/main/main.c (~2541 lines with detailed comments)
- sd_usb_lcd/README.md (newly created)

### Phase 10 Status
- [DONE] Prompt 10.1: Demo video requirements documented
- [DONE] Prompt 10.2: README.md created with full documentation
- [TODO] User: Record demo video
- [TODO] User: Push to GitHub repository
- [TODO] User: Submit repository URL

---

## Additional Documentation (Feb 18, 2026)

### Created topology.md
Added sd_usb_lcd/topology.md with:
- File structure diagram
- System block diagram (ASCII)
- Initialization sequence flowchart
- Function groups organized by module:
  - SD Card Module (Phase 2)
  - File Operations Module (Phase 3)
  - USB MSC Module (Phase 4)
  - LCD Display Module (Phase 5)
  - RTC Clock Module (Phase 6)
  - Clock Display Task (Phase 6)
- Data flow diagram
- Concurrency model documentation
- GPIO summary table
- Memory map approximation
- Error handling strategy
- Build dependencies list

### Git Cleanup
- Removed large file (Freenove zip 233MB) from git history
- Used git filter-branch to rewrite commits
- Repository size reduced for GitHub push

## 2026-02-25 - week5prompts.md Creation

### Request
User requested creation of week5prompts.md file mirroring the topology of week4Prompts.md. The file contains steps and tests for two new features:
1. Wi-Fi connectivity to hidden SSID (Embedded / class2026-embedded) with NTP time sync from time.nist.gov for Los Angeles time zone
2. Telegram bot integration (via BotFather) to receive messages/photos, displayed on an ESP32-S3 HTTP web server

### Actions Taken
1. Read AgentConstraints.md to understand constraints
2. Read week4Prompts.md (all 706 lines) to understand structure:
   - Summary section with feature list and hardware requirements
   - Multiple phases with numbered prompts
   - Manual test checklists after each prompt
   - Success criteria section
   - Additional notes section
3. Read existing main.c and AI_Interaction.md for hardware and project context
4. Created week5prompts.md with 7 phases:
   - Phase 1: Wi-Fi Configuration and Connection (3 prompts)
     - Wi-Fi headers and component setup
     - Station init with hidden SSID support (WIFI_ALL_CHANNEL_SCAN)
     - Reconnection logic and LCD status display
   - Phase 2: NTP Time Synchronization (4 prompts)
     - SNTP config with time.nist.gov, TZ=PST8PDT,M3.2.0,M11.1.0
     - SNTP init function with Wi-Fi wait
     - Replace software RTC with NTP-synced system time on LCD
     - NTP sync status indicators
   - Phase 3: Telegram Bot Setup (5 prompts)
     - BotFather setup (manual step)
     - API token config and TLS dependencies
     - getUpdates polling with cJSON parsing
     - Photo download (getFile + file download)
     - FreeRTOS polling task
   - Phase 4: HTTP Web Server (4 prompts)
     - Basic HTTP server on port 80
     - Main page displaying Telegram text/photo with auto-refresh
     - /photo endpoint serving images from SD card
     - /status JSON endpoint with device info
   - Phase 5: Integration and Main Application Update (2 prompts)
     - Updated app_main() initialization order
     - LCD display mode cycling
   - Phase 6: Build, Flash, and Test (5 prompts)
     - Build and flash
     - Wi-Fi verification
     - NTP verification
     - Telegram bot verification
     - Web server verification
   - Phase 7: Documentation and Submission (2 prompts)
     - Demo video
     - GitHub repository update

### Wi-Fi Configuration
- SSID: Embedded (hidden)
- Password: class2026-embedded
- scan_method: WIFI_ALL_CHANNEL_SCAN (required for hidden SSID)

### NTP Configuration
- Server: time.nist.gov
- Time zone: America/Los_Angeles
- TZ string: PST8PDT,M3.2.0,M11.1.0

### Files Created
- /home/nordiffico/Documents/esp/projects/week5/week5prompts.md

### Result
DONE - week5prompts.md created successfully with full prompt/test structure mirroring week4Prompts.md

## 2026-02-25 - Phase 1: Wi-Fi Configuration and Connection

### Request
User requested to proceed with Phase 1 of week5prompts.md (Prompts 1.1, 1.2, 1.3)

### Actions Taken (Prompt 1.1 - Wi-Fi Headers and Dependencies)
1. Updated main/CMakeLists.txt REQUIRES to add:
   - esp_wifi: Wi-Fi driver for station mode
   - esp_event: Default event loop for Wi-Fi and IP events
   - esp_netif: Network interface abstraction layer
   - nvs_flash: Non-volatile storage for Wi-Fi credential caching
2. Added new includes to main.c:
   - esp_wifi.h, esp_event.h, esp_netif.h, nvs_flash.h
   - freertos/event_groups.h (for signaling Wi-Fi connection state)
3. Added Wi-Fi credential defines:
   - WIFI_SSID: "Embedded" (hidden SSID)
   - WIFI_PASS: "class2026-embedded"
   - WIFI_MAX_RETRY: 10
   - WIFI_CONNECTED_BIT and WIFI_FAIL_BIT for event group

### Actions Taken (Prompt 1.2 - Wi-Fi Station Init and Connection)
1. Added Wi-Fi global variables:
   - wifi_event_group: EventGroupHandle_t for connection signaling
   - wifi_retry_count: Tracks reconnection attempts
   - wifi_connected: Flag for LCD status display
   - wifi_ip_str: Stores IP address string for display
2. Implemented wifi_event_handler() callback handling:
   - WIFI_EVENT_STA_START: Initiates first connection
   - WIFI_EVENT_STA_DISCONNECTED: Auto-reconnect with retry counter
   - IP_EVENT_STA_GOT_IP: Stores IP, resets retry counter, signals event group
3. Implemented wifi_init_sta() with 8-step initialization:
   - NVS flash init (with erase/retry on corruption)
   - Event group creation
   - Network interface and event loop initialization
   - Wi-Fi driver init with default config
   - Event handler registration (WIFI_EVENT and IP_EVENT)
   - Wi-Fi station config with hidden SSID support:
     * scan_method = WIFI_ALL_CHANNEL_SCAN (required for hidden SSID)
     * threshold.authmode = WIFI_AUTH_WPA2_PSK
   - Wi-Fi start
   - Blocking wait on event group for connection or failure

### Actions Taken (Prompt 1.3 - Reconnection and LCD Status)
1. Auto-reconnect implemented in wifi_event_handler:
   - On disconnect, retries up to WIFI_MAX_RETRY (10) times
   - Logs each attempt with attempt number
   - Sets WIFI_FAIL_BIT after all retries exhausted
2. LCD status display updated:
   - Changed clock_task LCD row 0 from "SD:Y USB:N" to "SD:Y WF:Y"
   - WF:Y = Wi-Fi connected, WF:N = Wi-Fi disconnected
   - clock_task now tracks wifi_connected state changes
3. Updated app_main():
   - Added Wi-Fi init as step 5/8 (after LCD init, before RTC)
   - Shows "WiFi Connect.." on LCD during connection
   - On success, briefly displays IP address for 2 seconds
   - On failure, shows "WiFi FAILED!" and continues without Wi-Fi
   - Renumbered all steps from 1-7 to 1-8
   - Added Wi-Fi status and IP to final status log

### Build Verification
- Build completed successfully with exit code 0
- No warnings or errors in main.c
- Binary size: 0xdcaa0 bytes (14% free in app partition)

### Files Modified
- /home/nordiffico/Documents/esp/projects/week5/sd_usb_lcd/main/CMakeLists.txt (added Wi-Fi REQUIRES)
- /home/nordiffico/Documents/esp/projects/week5/sd_usb_lcd/main/main.c (Wi-Fi init, connect, LCD status)

### Result
DONE - Phase 1 completed. Wi-Fi station mode with hidden SSID support, auto-reconnect, and LCD status display. Build passes clean.

## 2026-02-18 - Week 5 Phase 2: NTP Time Synchronization

### Request
Implement Phase 2 from week5prompts.md: NTP time synchronization using SNTP with time.nist.gov, Los Angeles timezone (PST/PDT), replacing the Week 4 software RTC.

### Actions Taken (Prompt 2.1 - SNTP Headers and Configuration)
1. Added SNTP-related includes to main.c:
   - `#include "esp_sntp.h"` - ESP-IDF SNTP client API
   - `#include <time.h>` - POSIX time functions (time, localtime_r, strftime)
   - `#include <sys/time.h>` - settimeofday/gettimeofday support
2. Added NTP configuration defines:
   - NTP_SERVER "time.nist.gov" - NIST's public NTP server
   - NTP_TZ_STRING "PST8PDT,M3.2.0,M11.1.0" - POSIX TZ for LA with DST rules
   - NTP_SYNC_INTERVAL (15 * 60 * 1000) - 15-minute re-sync interval
3. Added global variable: `ntp_synced` (volatile bool, default false)
4. Added function prototypes for ntp_time_sync_notification_cb() and sntp_init_time()

### Actions Taken (Prompt 2.2 - SNTP Initialization Function)
1. Implemented sntp_init_time():
   - Sets POSIX TZ via setenv("TZ")/tzset() for automatic DST handling
   - Configures SNTP in poll mode (SNTP_OPMODE_POLL) for periodic re-sync
   - Sets NTP server to time.nist.gov via esp_sntp_setservername()
   - Registers notification callback for sync events
   - Calls esp_sntp_init() to start background SNTP client
   - Waits up to 30 seconds for first sync (polling esp_sntp_get_sync_status)
   - Returns ESP_OK on success, ESP_FAIL on timeout

### Actions Taken (Prompt 2.3 - NTP Callback and Sync Status)
1. Implemented ntp_time_sync_notification_cb():
   - Called by SNTP on every successful time sync
   - First sync: sets ntp_synced=true, logs human-readable time
   - Subsequent syncs: logs re-sync for debugging
   - Uses localtime_r() + strftime() for thread-safe time formatting

### Actions Taken (Prompt 2.4 - Clock Task NTP Integration)
1. Replaced clock_task() to use NTP time instead of software RTC:
   - Uses time(NULL) + localtime_r() to get current LA local time
   - Uses strftime() for time formatting (%H:%M:%S)
   - Shows "--:--:--" and "--/--" when ntp_synced is false
   - Shows real date/time after NTP sync completes
2. Changed LCD row 0 status from "WF:Y/N" to "NT:Y/N" (NTP status):
   - NT:Y = NTP time synchronized
   - NT:N = NTP not yet synchronized
3. Updated app_main():
   - Replaced step 6 (software RTC init) with NTP init step
   - Calls sntp_init_time() after Wi-Fi connection
   - Shows "NTP Syncing.." on LCD during sync
   - Briefly displays synced time on success
   - Skips NTP gracefully if Wi-Fi not connected
   - Updated final status log to show NTP status instead of RTC
4. Updated app_main docstring to reflect NTP replacing RTC in startup sequence

### Technical Details
- TZ string "PST8PDT,M3.2.0,M11.1.0" means:
  - PST (UTC-8) standard time
  - PDT (UTC-7) daylight saving time
  - DST starts: 2nd Sunday of March at 02:00
  - DST ends: 1st Sunday of November at 02:00
- SNTP poll mode: ESP-IDF SNTP client automatically re-queries the NTP server periodically
- System clock: ESP-IDF maintains via settimeofday(); time() reads it
- Old software RTC code (datetime_t, rtc_timer, etc.) left in place but no longer called from app_main

### Build Verification
- Build completed successfully with exit code 0
- No warnings or errors in main.c
- Binary size: 0xdb930 bytes (14% free in app partition)

### Files Modified
- /home/nordiffico/Documents/esp/projects/week5/sd_usb_lcd/main/main.c (NTP init, clock task rewrite, app_main update)

### Result
DONE - Phase 2 completed. NTP time synchronization via time.nist.gov with Los Angeles timezone. Clock display uses time()/localtime()/strftime() for accurate local time with DST support. Shows "--:--:--" until first NTP sync. Build passes clean.

## 2026-02-25 - Week 5 Phase 3: Telegram Bot Setup

### Request
Implement Phase 3 from week5prompts.md: Telegram Bot API integration for receiving text messages and photos, with getUpdates long polling, photo download to SD card, and a background polling task.

### Actions Taken (Prompt 3.1 - Bot Creation via BotFather)
1. User provided Telegram Bot token: 8745173474:AAGRhtDMuCoIJmNWbcv768wsyXLurzzV1Ak
2. Token stored as a #define in main.c (TELEGRAM_BOT_TOKEN)
3. This was a manual step - bot created by user via @BotFather in Telegram

### Actions Taken (Prompt 3.2 - Telegram Config and Dependencies)
1. Added Telegram/HTTP headers to main.c:
   - esp_http_client.h - HTTP client for HTTPS requests to Telegram API
   - esp_tls.h - TLS/SSL for secure connections
   - esp_crt_bundle.h - Certificate bundle for TLS verification
   - cJSON.h - JSON parser for API responses
2. Added configuration defines:
   - TELEGRAM_BOT_TOKEN - Bot API token
   - TELEGRAM_API_URL - Base URL for API calls (https://api.telegram.org/bot<TOKEN>)
   - TELEGRAM_FILE_URL - Base URL for file downloads
   - TELEGRAM_POLL_TIMEOUT (10s) - Long polling timeout
   - TELEGRAM_RETRY_DELAY_MS (5000ms) - Error retry delay
   - TELEGRAM_MAX_MSG_LEN (256) - Max stored message length
   - TELEGRAM_PHOTO_PATH - SD card path for photo saves
   - TELEGRAM_HTTP_BUF_SIZE (4096) - HTTP response buffer size
3. Updated CMakeLists.txt REQUIRES:
   - Added esp_http_client, esp-tls, json components
   - Note: component is "esp-tls" (hyphen) not "esp_tls" (underscore) in ESP-IDF v5.3
4. Added global variables:
   - telegram_update_offset - tracks last processed update_id
   - telegram_last_message[256] - latest text message
   - telegram_photo_path[128] - path to saved photo
   - telegram_new_content - flag for new content
   - telegram_mutex - mutex for shared state protection
   - telegram_task_handle - FreeRTOS task handle

### Actions Taken (Prompt 3.3 - getUpdates Polling Implementation)
1. Implemented telegram_http_event_handler():
   - Accumulates HTTP response chunks into a buffer
   - Handles buffer overflow protection
2. Implemented telegram_get_updates():
   - Builds getUpdates URL with offset and timeout parameters
   - Uses esp_http_client with TLS certificate bundle for HTTPS
   - Parses JSON response with cJSON
   - Iterates over result array to process each update
   - Extracts update_id and increments offset (prevents duplicate processing)
   - For text messages: stores in telegram_last_message (mutex protected)
   - For photos: extracts file_id from largest photo in array
   - Calls telegram_download_photo() for photo messages

### Actions Taken (Prompt 3.4 - Photo Download Implementation)
1. Implemented telegram_download_photo() with two-step download:
   - Step 1: Calls getFile API to convert file_id into file_path
   - Step 2: Downloads binary photo data from file download URL
2. Chunked download approach:
   - Uses esp_http_client_open/read/close for streaming download
   - 2KB download buffer written incrementally to SD card
   - Avoids buffering entire photo in RAM
3. Error handling:
   - Checks USB MSC active state before writing to SD card
   - Removes incomplete files on download failure
   - Separate timeout for getFile (10s) and download (30s)

### Actions Taken (Prompt 3.5 - Polling Task)
1. Implemented telegram_poll_task():
   - Runs in infinite loop calling telegram_get_updates()
   - Pauses when Wi-Fi is disconnected
   - On error: waits TELEGRAM_RETRY_DELAY_MS before retry
   - On success: loops immediately (long polling provides delay)
2. Implemented telegram_poll_task_start():
   - Creates telegram_mutex for shared state protection
   - Creates FreeRTOS task with 8192 byte stack (TLS needs substantial stack)
   - Priority 3 (above clock task, below time-critical tasks)
3. Updated app_main():
   - Added step 8/9: Start Telegram Bot polling task
   - Renumbered all steps from /8 to /9
   - Updated final status log to include Telegram status

### Partition Table Update
- Binary grew from 0xdb930 (Phase 2) to 0x10e9a0 (~1074KB) due to TLS/HTTP client
- Default single app partition (1MB/0x100000) was too small
- Switched to CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE (1500KB/0x177000)
- Updated both sdkconfig and sdkconfig.defaults
- 28% free space remaining in app partition

### Build Verification
- Build completed successfully with exit code 0
- No warnings or errors in main.c
- Binary size: 0x10e9a0 bytes, partition: 0x177000 bytes (28% free)

### Files Modified
- main/main.c - Telegram headers, defines, globals, prototypes, and full implementation
- main/CMakeLists.txt - Added esp_http_client, esp-tls, json to REQUIRES
- sdkconfig - Switched to large single app partition table
- sdkconfig.defaults - Added CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y

### Result
DONE - Phase 3 completed. Telegram Bot polling with getUpdates, text message storage, two-step photo download to SD card, and background FreeRTOS polling task. Build passes clean with no warnings.
