
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

