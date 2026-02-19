# Prompts for Embedded Firmware Essentials Week 4 Assignment

## Summary

This document contains all prompts necessary to complete the Week 4 assignment for the UCSC Silicon Valley Embedded Firmware Essentials Class. The assignment involves implementing SD card file management, USB Mass Storage, and an RTC clock display on the ESP32-S3.

The main features to implement:
- **SD Card FAT File System**: Mount SD card and perform read/write/modify operations using FAT file system
- **USB Mass Storage (MSC)**: Expose the SD card to a PC for direct file management (no POSIX)
- **LCD Clock Display**: Show current date/time on an I2C LCD display using the 4-bit driver
- **Optional - Servo Control**: Use accelerometer angle readings to control a servo motor

### Hardware Requirements (In Addition to Week 3)

- ESP32-S3 development board
- USB cable (data capable)
- SD card module (SPI interface)
- SD card (FAT32 formatted)
- I2C LCD display (16x2 or 20x4, HD44780-compatible with I2C backpack)
- 1k Ohm resistors (pull-up for I2C SDA and SCL lines)
- Optional: Servo motor and accelerometer (e.g., MPU6050)

---

## Phase 1: Project Setup and SD Card Hardware Configuration

### Prompt 1.1
"Create a new ESP-IDF project for the Week 4 SD card and USB MSC example:
```
get_idf
mkdir -p ~/esp/projects/week4/sd_usb_lcd
cd ~/esp/projects/week4/sd_usb_lcd
```
Create the project structure with CMakeLists.txt, main/CMakeLists.txt, and main/main.c files. The project will demonstrate SD card file operations, USB Mass Storage, and LCD clock display."

### Manual Tests to Verify Completion of Prompt 1.1
- [ ] Directory ~/esp/projects/week4/sd_usb_lcd exists
- [ ] Project CMakeLists.txt file exists with correct cmake_minimum_required and project() calls
- [ ] main/CMakeLists.txt file exists with idf_component_register() call
- [ ] main/main.c file exists with basic structure and includes

---

### Prompt 1.2
"Configure the ESP-IDF project for ESP32-S3 and enable required components:
```
get_idf
cd ~/esp/projects/week4/sd_usb_lcd
idf.py set-target esp32s3
idf.py menuconfig
```
Enable the following in menuconfig:
- FATFS component (Component config -> FAT Filesystem support)
- USB TinyUSB stack for MSC (Component config -> TinyUSB Stack)
- Set USB device as MSC (Mass Storage Class)
Document the GPIO pins used for SD card SPI interface (MOSI, MISO, CLK, CS)."

### Manual Tests to Verify Completion of Prompt 1.2
- [ ] Target set to ESP32-S3 without errors
- [ ] sdkconfig file generated in project directory
- [ ] FATFS component enabled in configuration
- [ ] TinyUSB stack enabled for USB MSC
- [ ] SD card SPI GPIO pins documented

---

### Prompt 1.3
"Define the GPIO pin mappings for SD card SPI interface in main/main.c:
- Select appropriate GPIO pins for SPI: MOSI, MISO, CLK, and CS
- Typical pins: MOSI (GPIO 11), MISO (GPIO 13), CLK (GPIO 12), CS (GPIO 10)
- Adjust based on your specific hardware configuration
- Include detailed comments explaining the pin assignments and SPI bus configuration"

### Manual Tests to Verify Completion of Prompt 1.3
- [ ] GPIO defines for MOSI, MISO, CLK, CS are present
- [ ] Comments explain the pin selection rationale
- [ ] Pins do not conflict with other peripherals (BOOT button, LEDs from Week 3)

---

## Phase 2: SD Card Initialization and FAT File System Mounting

### Prompt 2.1
"Implement SD card SPI initialization in main/main.c:
- Configure the SPI bus using spi_bus_initialize()
- Use the appropriate SPI host (SPI2_HOST or SPI3_HOST)
- Set SPI clock frequency appropriate for SD card (start with 400kHz for initialization, can increase later)
- Include detailed comments explaining the SPI initialization process"

### Manual Tests to Verify Completion of Prompt 2.1
- [ ] SPI bus configuration struct is properly initialized
- [ ] spi_bus_initialize() is called with correct parameters
- [ ] Error checking is in place for SPI initialization
- [ ] Comments explain the SPI configuration choices

---

### Prompt 2.2
"Mount the SD card with FAT file system using ESP-IDF sdmmc and fatfs components:
- Use esp_vfs_fat_sdspi_mount() to mount the SD card
- Configure the mount point (e.g., '/sdcard')
- Set appropriate mount configuration options:
  - format_if_mount_failed: false (do not auto-format)
  - max_files: 5 or appropriate value
- Handle mount failures gracefully with error messages
- Include detailed comments explaining the FAT mount process"

### Manual Tests to Verify Completion of Prompt 2.2
- [ ] sdmmc_card_t structure is properly defined
- [ ] esp_vfs_fat_sdspi_mount() is called with correct parameters
- [ ] Mount point is defined (e.g., "/sdcard")
- [ ] Error handling reports mount failures clearly
- [ ] Comments explain FAT file system mounting

---

### Prompt 2.3
"Create a function to verify SD card mount and display card information:
- Print SD card name, type (SDSC, SDHC, SDXC)
- Print card capacity and speed class if available
- Print mount status confirmation
- Call this function after successful mount
- Include detailed comments"

### Manual Tests to Verify Completion of Prompt 2.3
- [ ] Function prints SD card information
- [ ] Card name and type are displayed
- [ ] Capacity information is shown
- [ ] Success message confirms mount

---

## Phase 3: FAT File System Read/Write/Modify Operations

### Prompt 3.1
"Implement a function to create and write a new file on the SD card:
- Create a file at a specified path (e.g., '/sdcard/test.txt')
- Write sample text content to the file
- Use standard C file I/O (fopen, fprintf, fclose)
- Include error handling for file operations
- Include detailed comments explaining file write process"

### Manual Tests to Verify Completion of Prompt 3.1
- [ ] Function accepts filename and content parameters
- [ ] File is created using fopen() with "w" mode
- [ ] Content is written using fprintf() or fwrite()
- [ ] File is properly closed with fclose()
- [ ] Error handling reports failures
- [ ] Comments explain the file operations

---

### Prompt 3.2
"Implement a function to read a file from the SD card:
- Open an existing file for reading
- Read the file content into a buffer
- Print the file content to the serial console
- Handle file not found and other errors
- Include detailed comments"

### Manual Tests to Verify Completion of Prompt 3.2
- [ ] Function accepts filename parameter
- [ ] File is opened with fopen() in "r" mode
- [ ] Content is read using fgets() or fread()
- [ ] Content is printed to console
- [ ] File not found error is handled gracefully
- [ ] Comments explain the read process

---

### Prompt 3.3
"Implement a function to modify/append content to an existing file:
- Open a file in append mode ('a')
- Add new content to the end of the file
- Close the file properly
- Verify the append by reading the file again
- Include detailed comments explaining append operations"

### Manual Tests to Verify Completion of Prompt 3.3
- [ ] Function opens file in append mode
- [ ] New content is written to end of file
- [ ] File is properly closed
- [ ] Comments explain append vs write modes

---

### Prompt 3.4
"Implement a function to list files in a directory on the SD card:
- Open a directory using opendir()
- Iterate through directory entries using readdir()
- Print file names, sizes, and types (file/directory)
- Handle empty directories
- Include detailed comments"

### Manual Tests to Verify Completion of Prompt 3.4
- [ ] Function accepts directory path parameter
- [ ] Uses opendir() and readdir() correctly
- [ ] Prints file information for each entry
- [ ] Handles empty directories gracefully
- [ ] Closes directory with closedir()
- [ ] Comments explain directory iteration

---

## Phase 4: USB Mass Storage (MSC) Implementation

### Prompt 4.1
"Configure USB TinyUSB stack for Mass Storage Class (MSC) device:
- Include TinyUSB headers and configuration
- Set USB device descriptors for MSC (VID, PID, manufacturer string, product string)
- Configure USB as Mass Storage device
- Include detailed comments explaining USB MSC configuration
- Important: Do not use POSIX VFS for USB MSC - use direct SD card access"

### Manual Tests to Verify Completion of Prompt 4.1
- [ ] TinyUSB headers are included
- [ ] USB device descriptors are configured
- [ ] MSC class is selected
- [ ] VID/PID are set to valid values
- [ ] Comments explain USB configuration

---

### Prompt 4.2
"Implement USB MSC callbacks for SD card access:
- Implement tud_msc_capacity_cb() to report SD card capacity
- Implement tud_msc_read10_cb() to read sectors from SD card
- Implement tud_msc_write10_cb() to write sectors to SD card
- Implement tud_msc_scsi_cb() for SCSI commands
- Use direct SD card sector access (sdmmc_read_sectors, sdmmc_write_sectors), not POSIX
- Include detailed comments explaining each callback"

### Manual Tests to Verify Completion of Prompt 4.2
- [ ] tud_msc_capacity_cb() returns correct sector count and size
- [ ] tud_msc_read10_cb() reads sectors using sdmmc_read_sectors()
- [ ] tud_msc_write10_cb() writes sectors using sdmmc_write_sectors()
- [ ] tud_msc_scsi_cb() handles basic SCSI commands
- [ ] No POSIX file I/O used in USB MSC callbacks
- [ ] Comments explain the block-level access

---

### Prompt 4.3
"Create a task to handle USB device operations:
- Initialize USB device stack with tud_init()
- Create a FreeRTOS task that calls tud_task() periodically
- Handle USB connect/disconnect events
- Log USB status changes to console
- Include detailed comments"

### Manual Tests to Verify Completion of Prompt 4.3
- [ ] tud_init() is called during initialization
- [ ] FreeRTOS task runs tud_task() in a loop
- [ ] Task has appropriate priority (higher than idle)
- [ ] USB events are logged
- [ ] Comments explain USB task requirements

---

### Prompt 4.4
"Implement logic to handle simultaneous SD card access:
- When USB MSC is active, disable local file operations to prevent conflicts
- Use a flag or mutex to track MSC connection state
- Unmount FAT VFS when USB connects, remount when USB disconnects
- Include detailed comments explaining the access coordination"

### Manual Tests to Verify Completion of Prompt 4.4
- [ ] Flag or mutex tracks USB MSC state
- [ ] Local file operations check MSC state before proceeding
- [ ] FAT VFS is unmounted during USB MSC sessions
- [ ] FAT VFS is remounted when USB disconnects
- [ ] Comments explain the mutual exclusion strategy

---

## Phase 5: I2C LCD Display Setup

### Prompt 5.1
"Configure I2C interface for the LCD display:
- Define I2C GPIO pins (SDA and SCL)
- Note: Use 1k Ohm external pull-up resistors on SDA and SCL lines
- Initialize I2C driver with i2c_driver_install() and i2c_param_config()
- Set I2C clock speed (typically 100kHz for LCD)
- Include detailed comments about the I2C configuration and pull-up requirements"

### Manual Tests to Verify Completion of Prompt 5.1
- [ ] I2C SDA and SCL GPIO pins are defined
- [ ] Comment notes 1k Ohm pull-up resistor requirement
- [ ] I2C driver is initialized correctly
- [ ] Clock speed is set appropriately
- [ ] Comments explain I2C setup

---

### Prompt 5.2
"Implement or integrate a 4-bit LCD driver for HD44780-compatible display:
- Use the 4-bit LCD driver from the repository (as noted in assignment)
- Configure for I2C PCF8574 backpack (common I2C LCD interface)
- Initialize LCD in 4-bit mode
- Set LCD dimensions (16x2 or 20x4 as appropriate)
- Include detailed comments referencing the driver source"

### Manual Tests to Verify Completion of Prompt 5.2
- [ ] 4-bit LCD driver code is integrated
- [ ] I2C address for LCD/PCF8574 is configured (typically 0x27 or 0x3F)
- [ ] LCD initialization sequence is correct (4-bit mode)
- [ ] LCD dimensions are set
- [ ] Comments reference the driver source

---

### Prompt 5.3
"Implement basic LCD functions:
- lcd_clear(): Clear the display
- lcd_set_cursor(row, col): Position the cursor
- lcd_print(text): Print a string at current cursor position
- lcd_backlight(on/off): Control backlight
- Include detailed comments for each function"

### Manual Tests to Verify Completion of Prompt 5.3
- [ ] lcd_clear() clears the display
- [ ] lcd_set_cursor() positions cursor correctly
- [ ] lcd_print() displays text at cursor
- [ ] lcd_backlight() controls backlight
- [ ] Comments explain each function

---

## Phase 6: Real-Time Clock Implementation

### Prompt 6.1
"Implement a software-based real-time clock using FreeRTOS:
- Create a structure to hold date/time (year, month, day, hour, minute, second)
- Use a FreeRTOS timer or task to increment time every second
- Implement proper calendar logic (days per month, leap years)
- Provide a function to set the initial time
- Include detailed comments explaining the RTC logic"

### Manual Tests to Verify Completion of Prompt 6.1
- [ ] DateTime structure is defined
- [ ] Timer or task increments time every second
- [ ] Month rollover is correct (28/29/30/31 days)
- [ ] Year rollover and leap year logic works
- [ ] Function to set time is provided
- [ ] Comments explain calendar calculations

---

### Prompt 6.2
"Create a task to update the LCD display with current time:
- Task runs periodically (every second or more frequently)
- Formats date as 'YYYY-MM-DD' or regional format
- Formats time as 'HH:MM:SS' (24-hour format)
- Updates LCD line 1 with date, line 2 with time (or combined)
- Include detailed comments"

### Manual Tests to Verify Completion of Prompt 6.2
- [ ] Task runs at appropriate interval
- [ ] Date is formatted correctly
- [ ] Time is formatted correctly
- [ ] LCD updates without flicker
- [ ] Comments explain display update strategy

---

### Prompt 6.3
"Add a way to set the initial time at startup:
- Option 1: Hardcode a compile-time timestamp
- Option 2: Read last saved time from SD card file
- Option 3: Accept time via serial console input
- Implement at least one method
- Include detailed comments"

### Manual Tests to Verify Completion of Prompt 6.3
- [ ] Initial time is set at startup
- [ ] Method of setting time is documented
- [ ] Time persists across reboots if using SD card method
- [ ] Comments explain the chosen approach

---

## Phase 7: Main Application Integration

### Prompt 7.1
"Implement the app_main() function to integrate all components:
- Initialize SD card (SPI, mount FAT)
- Perform file operation demos (create, read, modify, list)
- Initialize USB MSC
- Initialize I2C and LCD
- Start the clock display task
- Start USB task
- Include error handling at each step
- Include detailed comments explaining the startup sequence"

### Manual Tests to Verify Completion of Prompt 7.1
- [ ] app_main() initializes all components in correct order
- [ ] SD card mounts successfully
- [ ] File operations demo runs
- [ ] USB MSC initializes
- [ ] LCD initializes and displays
- [ ] Clock task starts
- [ ] Error handling at each step
- [ ] Comments explain initialization order

---

### Prompt 7.2
"Add status indicators to LCD display:
- Show SD card status (mounted/unmounted)
- Show USB connection status (connected/disconnected)
- Update status when state changes
- Reserve appropriate LCD area for status
- Include detailed comments"

### Manual Tests to Verify Completion of Prompt 7.2
- [ ] SD card status shown on LCD
- [ ] USB status shown on LCD
- [ ] Status updates on state change
- [ ] Layout is readable
- [ ] Comments explain status display

---

## Phase 8: Build, Flash, and Test

### Prompt 8.1
"Build and flash the Week 4 project to the ESP32-S3:
```
get_idf
cd ~/esp/projects/week4/sd_usb_lcd
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```
(Replace /dev/ttyACM0 with your actual port if different)"

### Manual Tests to Verify Completion of Prompt 8.1
- [ ] Build completes without errors or warnings
- [ ] Flash completes successfully
- [ ] Serial monitor shows startup messages
- [ ] No crash or panic messages on boot

---

### Prompt 8.2
"Verify SD card file operations:
1. Confirm SD card mounts successfully
2. Verify file creation (check for created file message)
3. Verify file read (content printed to console)
4. Verify file append (content updated)
5. Verify directory listing shows all files"

### Manual Tests to Verify Completion of Prompt 8.2
- [ ] SD card mounts and card info is displayed
- [ ] File creation succeeds (test.txt created)
- [ ] File content is read and printed
- [ ] File append adds content
- [ ] Directory listing shows files

---

### Prompt 8.3
"Verify USB Mass Storage functionality:
1. Connect ESP32-S3 USB to PC
2. Verify PC recognizes a removable drive
3. Open the drive and browse files
4. Create/modify/delete a file from PC
5. Safely eject the drive
6. Verify changes are visible when re-mounted locally"

### Manual Tests to Verify Completion of Prompt 8.3
- [ ] PC detects new removable drive
- [ ] Drive shows correct capacity
- [ ] Files created by ESP32 are visible
- [ ] PC can create/modify/delete files
- [ ] Safe eject works without errors
- [ ] Changes persist after disconnect

---

### Prompt 8.4
"Verify LCD clock display:
1. Confirm LCD initializes and backlight is on
2. Verify date is displayed correctly
3. Verify time updates every second
4. Check for display flicker or artifacts
5. Verify status indicators update correctly"

### Manual Tests to Verify Completion of Prompt 8.4
- [ ] LCD displays content after startup
- [ ] Date format is correct and readable
- [ ] Time increments every second
- [ ] No flicker or display artifacts
- [ ] Status indicators work

---

## Phase 9: Optional - Servo Control with Accelerometer

### Hardware Information: MPU6050 Accelerometer/Gyroscope

The MPU6050 is a 6-axis motion tracking device with 3-axis accelerometer and 3-axis gyroscope.

**MPU6050 Module Pinout (8 pins):**
| Pin | Function | Connect To |
|-----|----------|------------|
| VCC | Power supply | 3.3V (NOT 5V - module has onboard regulator but ESP32 I2C is 3.3V) |
| GND | Ground | GND rail |
| SCL | I2C Clock | GPIO 9 (shared with LCD) |
| SDA | I2C Data | GPIO 8 (shared with LCD) |
| XDA | Auxiliary I2C Data | Leave unconnected |
| XCL | Auxiliary I2C Clock | Leave unconnected |
| AD0 | I2C Address select | GND (address = 0x68) or 3.3V (address = 0x69) |
| INT | Interrupt output | Leave unconnected (or connect to GPIO for motion interrupts) |

**I2C Configuration:**
- I2C Address: 0x68 (AD0 = GND) or 0x69 (AD0 = HIGH)
- Shares I2C bus with LCD (0x27) - both devices on GPIO 8/9
- Uses same 1k pull-up resistors on SDA/SCL as LCD
- I2C Speed: 100kHz (standard mode, compatible with both LCD and MPU6050)

**Servo Motor (SG90) Wiring:**
| Wire Color | Connect To |
|------------|------------|
| Red | 5V rail |
| Brown/Black | GND rail |
| Orange/Yellow | GPIO 4 (PWM signal) |

**Current Hardware Configuration:**
- GPIO 8: I2C SDA (LCD + MPU6050) with 1k pull-up to 3.3V
- GPIO 9: I2C SCL (LCD + MPU6050) with 1k pull-up to 3.3V
- GPIO 4: Servo PWM output
- GPIO 38/39/40: SDMMC (onboard SD card slot - CMD/CLK/D0)

### Prompt 9.1
"Configure I2C for MPU6050 accelerometer (if implementing optional feature):
- Share I2C bus with LCD (already initialized on GPIO 8/9)
- Configure MPU6050 I2C address (0x68 with AD0 to GND)
- Initialize MPU6050 with appropriate settings
- Include detailed comments"

### Manual Tests to Verify Completion of Prompt 9.1
- [ ] I2C configured for MPU6050
- [ ] Correct I2C address used (0x68)
- [ ] MPU6050 initialization succeeds
- [ ] Comments explain configuration

---

### Prompt 9.2
"Read accelerometer angle data from MPU6050:
- Read accelerometer X, Y, Z values
- Calculate tilt angle (pitch or roll as appropriate)
- Convert raw values to degrees
- Print angle to console for debugging
- Include detailed comments"

### Manual Tests to Verify Completion of Prompt 9.2
- [ ] Accelerometer data is read successfully
- [ ] Angle calculation is implemented
- [ ] Angle values are reasonable (e.g., 0-180 or -90 to +90)
- [ ] Values change when sensor is tilted
- [ ] Comments explain angle calculation

---

### Prompt 9.3
"Configure PWM for servo motor control:
- Select GPIO pin for servo PWM output
- Configure LEDC or MCPWM for servo (50Hz, 1-2ms pulse width)
- Implement function to set servo angle (0-180 degrees)
- Map angle to appropriate pulse width
- Include detailed comments"

### Manual Tests to Verify Completion of Prompt 9.3
- [ ] PWM GPIO pin selected
- [ ] PWM configured at 50Hz
- [ ] set_servo_angle() function works
- [ ] Servo moves to specified positions
- [ ] Comments explain PWM/servo mapping

---

### Prompt 9.4
"Create task to map accelerometer angle to servo position:
- Read accelerometer angle periodically
- Map tilt angle to servo angle
- Apply smoothing or dead zone if needed
- Update servo position based on accelerometer
- Include detailed comments"

### Manual Tests to Verify Completion of Prompt 9.4
- [ ] Task reads accelerometer and updates servo
- [ ] Servo follows accelerometer tilt
- [ ] Movement is smooth (not jittery)
- [ ] Range mapping is appropriate
- [ ] Comments explain the control loop

---

## Phase 10: Documentation and Submission

### Prompt 10.1
"Record a demo video showing all Week 4 features:
1. Show the ESP32-S3 board with SD card, LCD, and connections
2. Demonstrate SD card mount and file operations in serial monitor
3. Connect USB to PC and show Mass Storage drive appearing
4. Create/modify a file from PC, show it in serial console
5. Show LCD displaying date/time updating
6. (Optional) Demonstrate servo following accelerometer
Save the video file for submission."

### Manual Tests to Verify Completion of Prompt 10.1
- [ ] Video shows hardware setup clearly
- [ ] SD card operations demonstrated
- [ ] USB MSC shown working with PC
- [ ] LCD clock display visible
- [ ] Optional servo demo if implemented
- [ ] Video quality sufficient for grading

---

### Prompt 10.2
"Update GitHub repository with Week 4 code:
1. Create a new repository or add to existing Week 3 repo
2. Push all project files including:
   - CMakeLists.txt
   - main/CMakeLists.txt
   - main/main.c (with all detailed comments)
   - Any additional source files (lcd driver, etc.)
   - README.md explaining the project
3. Provide the repository URL for submission"

### Manual Tests to Verify Completion of Prompt 10.2
- [ ] Repository contains all source files
- [ ] All code has detailed comments
- [ ] README.md explains project and hardware setup
- [ ] Build instructions are included
- [ ] Repository URL is documented

---

## Success Criteria

### SD Card FAT File System
- [ ] SD card mounts successfully with FAT file system
- [ ] File create operation works
- [ ] File read operation works
- [ ] File modify/append operation works
- [ ] Directory listing works
- [ ] Card info is displayed

### USB Mass Storage
- [ ] USB MSC initializes without errors
- [ ] PC recognizes ESP32-S3 as removable drive
- [ ] PC can browse files on SD card
- [ ] PC can create/modify/delete files
- [ ] No POSIX used in MSC implementation (direct sector access)
- [ ] Concurrent access is handled safely

### LCD Clock Display
- [ ] I2C LCD initializes correctly
- [ ] 1k Ohm pull-up resistors used on I2C lines
- [ ] 4-bit LCD driver used as per assignment
- [ ] Date is displayed in readable format
- [ ] Time updates every second
- [ ] No display flicker

### Optional: Servo Control
- [ ] Accelerometer reads tilt angle
- [ ] Servo responds to accelerometer
- [ ] Movement is reasonably smooth

### Demo Video and GitHub
- [ ] Video demonstrates all features
- [ ] Code pushed to GitHub
- [ ] README documents the project

---

## Additional Notes

1. **SD Card SPI Interface**: ESP32-S3 uses GPIO matrix for flexible pin assignment. Choose pins that don't conflict with USB, I2C, or LED GPIOs from Week 3.

2. **FAT File System**: Use FAT32 for cards larger than 2GB. Format the SD card before first use if mount fails.

3. **USB MSC Without POSIX**: The assignment specifies no POSIX for USB MSC. Use sdmmc_read_sectors() and sdmmc_write_sectors() for direct block access in MSC callbacks instead of fopen/fread/fwrite.

4. **I2C Pull-up Resistors**: The assignment specifically mentions 1k Ohm resistors for I2C pull-ups. Do not rely on internal pull-ups as they may be too weak for reliable LCD communication.

5. **4-bit LCD Driver**: Use the existing 4-bit LCD driver in the repository. This is more efficient than 8-bit mode and is the standard for I2C LCD backpacks.

6. **LCD I2C Address**: Common addresses are 0x27 or 0x3F. Use an I2C scanner if unsure.

7. **Real-Time Clock**: A software RTC will drift over time. For production use, consider using an RTC chip (DS3231) or NTP synchronization.

8. **USB and Local Access Conflict**: When PC is using the SD card via USB MSC, the ESP32 should not access the file system to avoid corruption.

9. **TinyUSB Configuration**: ESP-IDF includes TinyUSB. Enable it in menuconfig under Component config -> TinyUSB.

10. **Servo PWM**: Standard servos use 50Hz PWM with 1ms (0 degrees) to 2ms (180 degrees) pulse width. Some servos have different ranges.

11. **MPU6050 Library**: Consider using an existing ESP-IDF component for MPU6050 to simplify accelerometer integration.

12. **Exit Serial Monitor**: Use Ctrl+] to exit the idf.py monitor.

