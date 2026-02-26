# SD_USB_LCD Project Topology

## Overview

This document describes the high-level architecture, function organization, and data flow for the ESP32-S3 SD Card, USB Mass Storage, and LCD Clock Display project.

---

## File Structure

```
sd_usb_lcd/
|
+-- CMakeLists.txt          # Top-level CMake configuration
|                           # - Sets minimum CMake version (3.16)
|                           # - Includes ESP-IDF project cmake
|                           # - Declares project name
|
+-- README.md               # User documentation
|                           # - Build instructions
|                           # - Hardware wiring
|                           # - Usage guide
|
+-- sdkconfig               # ESP-IDF SDK configuration
|                           # - TinyUSB enabled for USB MSC
|                           # - FATFS component enabled
|                           # - SDMMC peripheral enabled
|                           # - I2C driver enabled
|
+-- sdkconfig.defaults      # Default SDK config overrides
|
+-- dependencies.lock       # ESP-IDF component version locks
|
+-- managed_components/     # Downloaded ESP-IDF components
|
+-- build/                  # Build output directory
|   +-- task_priority.elf   # Compiled binary
|   +-- bootloader/         # Second-stage bootloader
|   +-- partition_table/    # Flash partition info
|
+-- main/
    +-- CMakeLists.txt      # Component CMake
    |                       # - Registers main.c
    |                       # - Links required components
    |
    +-- main.c              # ALL application code (~2541 lines)
                            # - Single monolithic source file
                            # - Well-commented with section headers
```

---

## System Block Diagram

```
+------------------------------------------------------------------+
|                         ESP32-S3 SOC                             |
|                                                                  |
|  +------------------+      +------------------+                  |
|  |   FreeRTOS       |      |   Hardware       |                  |
|  |   Scheduler      |      |   Peripherals    |                  |
|  +--------+---------+      +--------+---------+                  |
|           |                         |                            |
|  +--------v---------+      +--------v---------+                  |
|  |                  |      |                  |                  |
|  |   clock_task     |      |   SDMMC Host     +---> SD Card      |
|  |   (1 sec loop)   |      |   (GPIO 38,39,40)|     (FAT32)      |
|  |                  |      |                  |                  |
|  +--------+---------+      +--------+---------+                  |
|           |                         ^                            |
|           v                         |                            |
|  +------------------+      +--------+---------+                  |
|  |                  |      |                  |                  |
|  |   I2C Master     |      |   TinyUSB Stack  +---> USB Host PC  |
|  |   (GPIO 8, 9)    |      |   (USB MSC)      |     (Mass Storage)
|  |                  |      |                  |                  |
|  +--------+---------+      +------------------+                  |
|           |                                                      |
|           v                                                      |
|  +------------------+                                            |
|  |   LCD 16x2       |                                            |
|  |   (PCF8574 0x27) |                                            |
|  +------------------+                                            |
|                                                                  |
+------------------------------------------------------------------+
```

---

## Initialization Sequence (app_main)

```
app_main()
    |
    +--[1]-- Create mutex (sd_access_mutex)
    |        Thread synchronization for SD access
    |
    +--[2]-- sd_card_init()
    |        Initialize SDMMC, mount FAT filesystem
    |        |
    |        +-- sd_card_print_info()
    |            Display card details
    |
    +--[3]-- file_operations_demo()
    |        Verify SD card with test writes/reads
    |
    +--[4]-- i2c_init()
    |        Configure I2C master for LCD
    |        |
    |        +-- lcd_init()
    |            Initialize HD44780 in 4-bit mode
    |
    +--[5]-- rtc_load_from_sd() [optional]
    |        Restore saved time if available
    |        |
    |        +-- rtc_init()
    |            Create FreeRTOS timer (1 second tick)
    |
    +--[6]-- usb_msc_init()
    |        Initialize TinyUSB mass storage
    |        Keep FAT mounted (coexists with sector access)
    |
    +--[7]-- clock_task_start()
             Create FreeRTOS task for LCD updates
```

---

## Function Groups

### 1. SD Card Module (Phase 2)

```
+-------------------------+
|     SD CARD MODULE      |
+-------------------------+
|                         |
| sd_card_init()          | --> Initialize SDMMC host
|   - Configure GPIO      |     Mount FAT at /sdcard
|   - Mount FAT32         |     Set sd_card_initialized flag
|                         |
| sd_card_print_info()    | --> Log card name, type, size
|                         |
| sd_card_unmount()       | --> Unmount FAT (used for cleanup)
|                         |
+-------------------------+
     Uses: sdmmc_host.h, esp_vfs_fat.h
     GPIOs: 38 (CMD), 39 (CLK), 40 (D0)
```

### 2. File Operations Module (Phase 3)

```
+-------------------------+
|   FILE OPS MODULE       |
+-------------------------+
|                         |
| file_write()            | --> Create/overwrite file
|   - path, content       |     Uses fopen("w")
|                         |
| file_read()             | --> Read and log file content
|   - path                |     Uses fopen("r")
|                         |
| file_append()           | --> Add to existing file
|   - path, content       |     Uses fopen("a")
|                         |
| directory_list()        | --> List dir contents
|   - path                |     Uses opendir/readdir
|                         |
| file_operations_demo()  | --> Run all tests
|   - Write test.txt      |
|   - Read test.txt       |
|   - Append to test.txt  |
|   - List /sdcard        |
|                         |
+-------------------------+
     Uses: stdio.h (fopen, fread, fwrite, fclose)
     Mount: /sdcard/
```

### 3. USB MSC Module (Phase 4)

```
+---------------------------+
|     USB MSC MODULE        |
+---------------------------+
|                           |
| usb_msc_init()            | --> Configure TinyUSB
|   - Set VID/PID           |     Register callbacks
|   - Register storage      |     Start USB task
|                           |
| usb_msc_mount_changed_cb()| --> Called by TinyUSB
|   - Set usb_msc_active    |     On PC mount/unmount
|                           |
| usb_msc_task()            | --> Periodic USB handling
|   - tud_task()            |     (if needed)
|                           |
| usb_msc_expose_to_host()  | --> Make SD visible to PC
|                           |     (mode switching)
|                           |
| usb_msc_mount_locally()   | --> Re-enable local access
|                           |     (mode switching)
|                           |
+---------------------------+
     Uses: tinyusb.h, tusb_msc_storage.h
     USB Device: VID 0x303A, PID 0x4002
     Product: "SD Card Reader"
```

### 4. LCD Display Module (Phase 5)

```
+---------------------------+
|     LCD MODULE            |
+---------------------------+
|                           |
| i2c_init()                | --> Configure I2C master
|   - GPIO 8 (SDA)          |     100kHz, external pullups
|   - GPIO 9 (SCL)          |
|                           |
| lcd_init()                | --> HD44780 4-bit init
|   - 8-to-4 bit switch     |     sequence per datasheet
|   - Set display mode      |
|   - Turn on, clear        |
|                           |
| lcd_i2c_write_byte()      | --> Write byte to PCF8574
|                           |
| lcd_pulse_enable()        | --> Strobe E pin
|   - esp_rom_delay_us()    |     (microsecond timing)
|                           |
| lcd_write_nibble()        | --> Send 4 bits to LCD
|                           |
| lcd_write_byte()          | --> Send 8 bits as 2 nibbles
|                           |
| lcd_send_command()        | --> HD44780 command (RS=0)
|                           |
| lcd_send_data()           | --> HD44780 data (RS=1)
|                           |
| lcd_clear()               | --> Clear display, home cursor
|                           |
| lcd_set_cursor()          | --> Position cursor (row, col)
|                           |
| lcd_print()               | --> Write string to display
|                           |
| lcd_backlight()           | --> Control backlight LED
|                           |
+---------------------------+
     I2C Address: 0x27 (PCF8574)
     Display: HD44780-compatible 16x2
     
     PCF8574 Bit Mapping:
     +---+---+---+---+---+---+---+---+
     | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
     | D7| D6| D5| D4| BL| E | RW| RS|
     +---+---+---+---+---+---+---+---+
```

### 5. RTC Clock Module (Phase 6)

```
+---------------------------+
|     RTC MODULE            |
+---------------------------+
|                           |
| rtc_init()                | --> Create FreeRTOS timer
|   - 1 second period       |     Auto-reload mode
|   - Start timer           |
|                           |
| rtc_timer_callback()      | --> Called every second
|   - datetime_increment_   |     by FreeRTOS timer
|     second()              |     service
|                           |
| datetime_set()            | --> Set date/time values
|   - year, month, day      |
|   - hour, minute, second  |
|                           |
| datetime_increment_       | --> Add 1 second
|   second()                |     Handle rollover:
|   - 60s -> minute++       |     second->minute->hour
|   - 60m -> hour++         |     ->day->month->year
|   - 24h -> day++          |
|   - days -> month++       |
|   - 12m -> year++         |
|                           |
| is_leap_year()            | --> Check divisibility
|                           |     by 4/100/400
|                           |
| get_days_in_month()       | --> Return 28-31
|                           |     (handles Feb)
|                           |
| rtc_save_to_sd()          | --> Write time to file
|                           |     /sdcard/rtc.txt
|                           |
| rtc_load_from_sd()        | --> Read time from file
|                           |     on boot
|                           |
+---------------------------+
     Timer: FreeRTOS software timer
     Tick: 1 second
     Persistence: /sdcard/rtc.txt
```

### 6. Clock Display Task (Phase 6)

```
+---------------------------+
|     CLOCK TASK            |
+---------------------------+
|                           |
| clock_task()              | --> FreeRTOS task
|   - Infinite loop         |     Priority: 5
|   - Format date string    |     Stack: 4096
|   - Format time string    |
|   - Update LCD row 0      |     Format:
|   - Update LCD row 1      |     "MM/DD SD:Y USB:N"
|   - vTaskDelay(1000ms)    |     "    HH:MM:SS    "
|                           |
| clock_task_start()        | --> Create the task
|                           |
+---------------------------+
     LCD Row 0: Date + status indicators
     LCD Row 1: Time (centered)
     Update Rate: 1 Hz
```

---

## Data Flow Diagram

```
                                  +-------------+
                                  |   PC Host   |
                                  +------+------+
                                         |
                                    USB Cable
                                         |
                                         v
+------------+    SDMMC     +------------+------------+
|  SD Card   |<------------>|        ESP32-S3        |
|  (FAT32)   |    Sector    |                        |
+------------+    Access    |  +------------------+  |
                            |  |    TinyUSB       |  |
                            |  |    USB MSC       |  |
                            |  +------------------+  |
                            |           |            |
                            |           v            |
                            |  +------------------+  |
                            |  |   SDMMC Driver   |  |
                            |  +------------------+  |
                            |           |            |
                            |  +--------v---------+  |
                            |  |   FAT Filesystem |  |
                            |  |   (VFS mounted)  |  |
                            |  +------------------+  |
                            |                        |
+-----------+    I2C        |  +------------------+  |
| LCD 16x2  |<--------------|  |   I2C Master     |  |
| (PCF8574) |   GPIO 8,9    |  +------------------+  |
+-----------+               |          ^             |
                            |          |             |
                            |  +-------+--------+    |
                            |  |  clock_task()  |    |
                            |  |  (FreeRTOS)    |    |
                            |  +----------------+    |
                            |          ^             |
                            |          |             |
                            |  +-------+--------+    |
                            |  | rtc_timer_cb() |    |
                            |  | (1 sec tick)   |    |
                            |  +----------------+    |
                            |                        |
                            +------------------------+
```

---

## Concurrency Model

```
+------------------+     +------------------+     +------------------+
|    app_main      |     |   clock_task     |     |  rtc_timer_cb    |
|  (startup only)  |     |  (task context)  |     | (timer context)  |
+------------------+     +------------------+     +------------------+
         |                        |                        |
         |   Creates              |   Reads                |   Updates
         v                        v                        v
+------------------------------------------------------------------------+
|                          Global Variables                              |
|                                                                        |
|  datetime_t current_time     - Updated by timer, read by clock_task   |
|  bool sd_card_initialized    - Set once at init, read by clock_task   |
|  bool usb_msc_active         - Set by USB callback, read by clock_task|
|  sdmmc_card_t *sd_card       - Set at init, used by USB MSC           |
|  SemaphoreHandle_t mutex     - Protects SD access transitions         |
|                                                                        |
+------------------------------------------------------------------------+

Tasks:
  - app_main: Runs once, initializes everything
  - clock_task: Runs continuously, updates LCD every second
  - rtc_timer_callback: Runs in timer daemon context, increments time

Synchronization:
  - sd_access_mutex: Protects SD card mode transitions
  - usb_msc_active: Volatile flag, atomic reads (no mutex needed)
```

---

## GPIO Summary

```
+--------+----------+------------------+-------------+
|  GPIO  |  Signal  |     Function     |    Notes    |
+--------+----------+------------------+-------------+
|   38   |   CMD    | SDMMC Command    | Onboard SD  |
|   39   |   CLK    | SDMMC Clock      | Onboard SD  |
|   40   |   D0     | SDMMC Data 0     | Onboard SD  |
+--------+----------+------------------+-------------+
|    8   |   SDA    | I2C Data         | 1k pullup   |
|    9   |   SCL    | I2C Clock        | 1k pullup   |
+--------+----------+------------------+-------------+
|   N/A  |   USB    | USB D+/D-        | Native USB  |
+--------+----------+------------------+-------------+
```

---

## Memory Map (Approximate)

```
Flash Partitions:
+------------------+  0x000000
|    Bootloader    |  64 KB
+------------------+  0x010000
|  Partition Table |  4 KB
+------------------+  0x010000
|    Application   |  ~1.2 MB
|   (task_priority |
|       .elf)      |
+------------------+
|    NVS Storage   |  24 KB
+------------------+

RAM Usage:
+------------------+
|   FreeRTOS heap  |  - Tasks, queues, timers
|                  |  - clock_task: 4KB stack
+------------------+
|   Static vars    |  - sd_card structure
|                  |  - datetime_t
|                  |  - Flags and mutex
+------------------+
|   DMA buffers    |  - SDMMC transfers
|                  |  - USB transfers
+------------------+
```

---

## Error Handling Strategy

```
Critical Errors (halt):
  - Mutex creation failure
  - SD card init failure (no storage = no function)

Non-Critical Errors (continue with warning):
  - I2C init failure (no LCD display)
  - LCD init failure (no visual feedback)
  - RTC time load failure (use default time)
  - USB MSC init failure (no PC access, SD still works)
  - Clock task start failure (no auto-updates)
```

---

## Build Dependencies

```
ESP-IDF Components Used:
  - driver (i2c, sdmmc_host)
  - esp_vfs_fat (FAT filesystem)
  - fatfs (FAT library)
  - tinyusb (USB stack)
  - freertos (tasks, timers, mutex)
  - esp_log (logging macros)
```
