/**
 * @file main.c
 * @brief SD Card, USB Mass Storage, and LCD Clock Display for ESP32-S3
 * 
 * This program demonstrates:
 * 
 * 1. SD Card with FAT File System:
 *    - Mount SD card via SPI interface
 *    - Read, write, and modify files using FAT file system
 *    - List directory contents
 * 
 * 2. USB Mass Storage (MSC):
 *    - Expose SD card to PC as a removable drive
 *    - Direct sector access (no POSIX) for USB operations
 *    - Handle concurrent access between USB and local operations
 * 
 * 3. I2C LCD Clock Display:
 *    - Display current date and time on HD44780-compatible LCD
 *    - Use 4-bit mode via I2C PCF8574 backpack
 *    - Requires 1k Ohm external pull-up resistors on I2C lines
 * 
 * 4. Optional - Servo Control (if implemented):
 *    - Read accelerometer angle from MPU6050
 *    - Control servo motor position based on tilt angle
 * 
 * @author Generated with AI assistance
 * @date 2026
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "sd_protocol_defs.h"
#include "tinyusb.h"
#include "tusb_msc_storage.h"

/* ============================================================================
 * LOGGING TAG
 * ============================================================================ */

// Tag used for ESP_LOGx macros throughout this file
static const char *TAG = "SD_USB_LCD";

/* ============================================================================
 * SD CARD SDMMC INTERFACE GPIO CONFIGURATION (For onboard SD slot)
 * ============================================================================
 * 
 * The SD card is connected via SDMMC interface (1-line or 4-line mode).
 * This is used for boards with built-in SD card slots like Freenove ESP32-S3.
 * 
 * SDMMC is faster than SPI and uses the following signals:
 * - CLK:  Clock signal
 * - CMD:  Command/response line (bidirectional)
 * - D0:   Data line 0 (required for 1-line mode)
 * - D1-D3: Additional data lines (optional, for 4-line mode)
 * 
 * COMMON FREENOVE ESP32-S3 BOARD PINOUT:
 *   Signal    GPIO
 *   ------    ----
 *   CLK   --> GPIO 39
 *   CMD   --> GPIO 40
 *   D0    --> GPIO 41
 * 
 * NOTE: If your board has different pins, check the schematic and update
 * the GPIO definitions below. The board may also use 4-line mode with:
 *   D1    --> GPIO 42
 *   D2    --> GPIO 1
 *   D3    --> GPIO 2
 */

// SDMMC GPIO pins for Freenove ESP32-S3-WROOM (hardcoded on board - do not modify)
#define SD_MMC_CMD_GPIO     GPIO_NUM_38  // SDMMC Command
#define SD_MMC_CLK_GPIO     GPIO_NUM_39  // SDMMC Clock
#define SD_MMC_D0_GPIO      GPIO_NUM_40  // SDMMC Data 0

// Uncomment these for 4-line SDMMC mode (faster, but uses more pins)
// #define SD_MMC_D1_GPIO   GPIO_NUM_42  // SDMMC Data 1
// #define SD_MMC_D2_GPIO   GPIO_NUM_1   // SDMMC Data 2
// #define SD_MMC_D3_GPIO   GPIO_NUM_2   // SDMMC Data 3

// Use 1-line mode by default (more compatible, fewer pins)
#define SDMMC_USE_1_LINE_MODE  1

// SD card mount point in the virtual file system
// All file operations will use this path prefix (e.g., "/sdcard/test.txt")
#define SD_MOUNT_POINT      "/sdcard"

/* ============================================================================
 * I2C INTERFACE GPIO CONFIGURATION FOR LCD
 * ============================================================================
 * 
 * The LCD display uses an I2C interface via a PCF8574 I/O expander (backpack).
 * This allows controlling the HD44780-compatible LCD with only 2 wires.
 * 
 * IMPORTANT: External pull-up resistors required!
 * The assignment specifies 1k Ohm pull-up resistors on SDA and SCL lines.
 * Internal pull-ups are too weak for reliable I2C communication with the LCD.
 * 
 * Wiring:
 *   LCD I2C Backpack    ESP32-S3 GPIO    Pull-up
 *   ----------------    -------------    -------
 *   SDA              --> GPIO 8       --> 1k to 3.3V
 *   SCL              --> GPIO 9       --> 1k to 3.3V
 *   VCC              --> 5V (or 3.3V if compatible)
 *   GND              --> GND
 */
#define I2C_SDA_GPIO        GPIO_NUM_8   // I2C data line
#define I2C_SCL_GPIO        GPIO_NUM_9   // I2C clock line

// I2C port number (ESP32-S3 has two I2C controllers: I2C_NUM_0 and I2C_NUM_1)
#define I2C_PORT_NUM        I2C_NUM_0

// I2C clock frequency - 100kHz is standard for most I2C LCDs
// Some displays support 400kHz (Fast Mode) but 100kHz is more reliable
#define I2C_FREQ_HZ         100000

// LCD I2C slave address
// Common addresses for PCF8574 backpack: 0x27 or 0x3F
// Use an I2C scanner if unsure about your LCD's address
#define LCD_I2C_ADDR        0x27

// LCD dimensions (16 columns x 2 rows is most common)
#define LCD_COLS            16
#define LCD_ROWS            2

/* ============================================================================
 * OPTIONAL: SERVO AND ACCELEROMETER GPIO CONFIGURATION
 * ============================================================================
 * 
 * For the optional servo control feature:
 * - Servo uses PWM via LEDC peripheral
 * - Accelerometer (MPU6050) shares I2C bus with LCD
 */

// Servo PWM configuration
#define SERVO_GPIO          GPIO_NUM_18  // PWM output for servo
#define SERVO_LEDC_CHANNEL  LEDC_CHANNEL_0
#define SERVO_LEDC_TIMER    LEDC_TIMER_0
#define SERVO_PWM_FREQ_HZ   50           // 50Hz for standard servos

// MPU6050 accelerometer I2C address (same I2C bus as LCD)
#define MPU6050_I2C_ADDR    0x68

/* ============================================================================
 * USB MASS STORAGE CONFIGURATION
 * ============================================================================
 * 
 * USB Mass Storage Class (MSC) configuration for exposing SD card to PC.
 * Uses TinyUSB stack provided by ESP-IDF.
 * 
 * IMPORTANT: USB MSC uses direct sector access (sdmmc_read_sectors/write_sectors)
 * NOT POSIX file I/O (fopen/fread/fwrite) as specified in the assignment.
 */

// USB device descriptors
#define USB_VID             0x303A  // Espressif VID
#define USB_PID             0x4002  // Custom PID for MSC device
#define USB_MANUFACTURER    "ESP32-S3"
#define USB_PRODUCT         "SD Card Reader"

/* ============================================================================
 * GLOBAL VARIABLES
 * ============================================================================ */

// SD card handle - used for direct sector access in USB MSC callbacks
static sdmmc_card_t *sd_card = NULL;

// Flag to track if USB MSC is actively connected
// When true, local file operations should be disabled to prevent conflicts
static volatile bool usb_msc_active = false;

// Flag to track if SD card initialized successfully (for status display)
static volatile bool sd_card_initialized = false;

// Mutex for coordinating access between USB and local operations
static SemaphoreHandle_t sd_access_mutex = NULL;

/* ============================================================================
 * FUNCTION PROTOTYPES
 * ============================================================================ */

// SD card functions
static esp_err_t sd_card_init(void);
static void sd_card_print_info(void);
static esp_err_t sd_card_unmount(void);

// File operation functions
static esp_err_t file_write(const char *path, const char *content);
static esp_err_t file_read(const char *path);
static esp_err_t file_append(const char *path, const char *content);
static esp_err_t directory_list(const char *path);

// LCD functions
static esp_err_t i2c_init(void);
static esp_err_t lcd_init(void);
static void lcd_clear(void);
static void lcd_set_cursor(uint8_t row, uint8_t col);
static void lcd_print(const char *text);
static void lcd_backlight(bool on);

// RTC Clock functions
static bool is_leap_year(uint16_t year);
static uint8_t get_days_in_month(uint8_t month, uint16_t year);
static void datetime_increment_second(void);
static esp_err_t datetime_set(uint16_t year, uint8_t month, uint8_t day,
                              uint8_t hour, uint8_t minute, uint8_t second);
static void rtc_timer_callback(TimerHandle_t xTimer);
static esp_err_t rtc_init(void);
static esp_err_t rtc_save_to_sd(void);
static esp_err_t rtc_load_from_sd(void);
static void clock_task(void *pvParameters);
static esp_err_t clock_task_start(void);

// USB MSC functions
static void usb_msc_task(void *pvParameters);

/* ============================================================================
 * SD CARD INITIALIZATION AND MOUNTING (Phase 2)
 * ============================================================================ */

/**
 * @brief Initialize SD card via SDMMC and mount FAT file system
 * 
 * This function performs the complete SD card initialization sequence:
 * 
 * 1. SDMMC Host Configuration:
 *    - Configures the SDMMC peripheral for 1-line or 4-line mode
 *    - Sets up clock, command, and data GPIO pins
 *    - Faster than SPI mode, commonly used for onboard SD slots
 * 
 * 2. Slot Configuration:
 *    - Configures GPIO pins for SDMMC signals
 *    - Sets bus width (1-line or 4-line)
 *    - Enables internal pull-ups if needed
 * 
 * 3. FAT File System Mount:
 *    - Uses esp_vfs_fat_sdmmc_mount() to mount the SD card
 *    - Creates Virtual File System (VFS) at mount point "/sdcard"
 *    - Does NOT auto-format if mount fails (preserves existing data)
 * 
 * After successful initialization:
 * - Files can be accessed using standard C file I/O (fopen, fread, etc.)
 * - All paths must be prefixed with mount point (e.g., "/sdcard/test.txt")
 * - The global sd_card pointer is set for direct sector access (USB MSC)
 * 
 * @return ESP_OK on success, error code on failure
 */
static esp_err_t sd_card_init(void)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Initializing SD card via SDMMC interface...");
    ESP_LOGI(TAG, "SDMMC pins: CLK=%d, CMD=%d, D0=%d", 
             SD_MMC_CLK_GPIO, SD_MMC_CMD_GPIO, SD_MMC_D0_GPIO);
    
    /* ========================================================================
     * STEP 1: Configure SDMMC host
     * ========================================================================
     * 
     * SDMMC_HOST_DEFAULT() configures the SDMMC peripheral with default settings.
     * We'll use slot 1 which is available on ESP32-S3.
     * 
     * The SDMMC interface is faster than SPI:
     * - 1-line mode: Up to 50 MHz, one data line
     * - 4-line mode: Up to 50 MHz, four data lines (4x faster throughput)
     */
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    
    // Use 1-line mode for better compatibility (fewer pins required)
    // 4-line mode is faster but needs D1, D2, D3 pins
#if SDMMC_USE_1_LINE_MODE
    host.flags = SDMMC_HOST_FLAG_1BIT;  // 1-line mode
    ESP_LOGI(TAG, "Using 1-line SDMMC mode");
#else
    ESP_LOGI(TAG, "Using 4-line SDMMC mode");
#endif
    
    /* ========================================================================
     * STEP 2: Configure SDMMC slot GPIO pins
     * ========================================================================
     * 
     * Configure which GPIO pins are used for SDMMC signals.
     * The Freenove ESP32-S3 uses GPIO 39/40/41 for CLK/CMD/D0.
     * Adjust these if your board uses different pins.
     */
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    
    // Set GPIO pins for SDMMC signals (Freenove ESP32-S3-WROOM)
    slot_config.clk = SD_MMC_CLK_GPIO;  // GPIO 39 - Clock
    slot_config.cmd = SD_MMC_CMD_GPIO;  // GPIO 38 - Command
    slot_config.d0 = SD_MMC_D0_GPIO;    // GPIO 40 - Data 0
    
#if !SDMMC_USE_1_LINE_MODE
    // 4-line mode requires additional data lines
    slot_config.d1 = SD_MMC_D1_GPIO;    // Data 1
    slot_config.d2 = SD_MMC_D2_GPIO;    // Data 2  
    slot_config.d3 = SD_MMC_D3_GPIO;    // Data 3
#endif
    
    // Set bus width based on mode
#if SDMMC_USE_1_LINE_MODE
    slot_config.width = 1;  // 1-line mode
#else
    slot_config.width = 4;  // 4-line mode
#endif
    
    // Enable internal pull-ups (external pull-ups recommended for reliability)
    // Some boards have them built-in, some don't
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    
    /* ========================================================================
     * STEP 3: Configure FAT file system mount options
     * ========================================================================
     * 
     * These options control how the FAT file system is mounted:
     * - format_if_mount_failed: If true, formats the card if mount fails
     *   We set this to false to avoid accidentally erasing data
     * - max_files: Maximum number of files that can be open simultaneously
     *   Set to 5 which is sufficient for most applications
     * - allocation_unit_size: Cluster size for formatting (only used if formatting)
     *   16KB is a good balance between space efficiency and performance
     */
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,  // Do NOT auto-format - preserve data
        .max_files = 5,                   // Max 5 files open at once
        .allocation_unit_size = 16 * 1024 // 16KB clusters (for formatting only)
    };
    
    /* ========================================================================
     * STEP 4: Mount the SD card with FAT file system
     * ========================================================================
     * 
     * esp_vfs_fat_sdmmc_mount() does several things:
     * 1. Initializes the SD card using SDMMC protocol
     * 2. Detects card type (SDSC, SDHC, SDXC)
     * 3. Reads card information (name, capacity, speed)
     * 4. Mounts FAT file system at the specified mount point
     * 5. Registers with VFS so standard file I/O works
     * 
     * After this call succeeds:
     * - sd_card points to card information structure
     * - Files can be accessed at "/sdcard/filename"
     */
    ESP_LOGI(TAG, "Mounting SD card FAT filesystem at %s...", SD_MOUNT_POINT);
    
    ret = esp_vfs_fat_sdmmc_mount(
        SD_MOUNT_POINT,     // Mount point in VFS (e.g., "/sdcard")
        &host,              // SDMMC host configuration
        &slot_config,       // SDMMC slot configuration (GPIO pins)
        &mount_config,      // FAT mount options
        &sd_card            // Output: pointer to card info structure
    );
    
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set format_if_mount_failed = true.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Check: 1) SD card inserted? 2) Correct GPIO pins? 3) FAT32 formatted?", 
                     esp_err_to_name(ret));
        }
        ESP_LOGE(TAG, "Expected SDMMC pins: CLK=%d, CMD=%d, D0=%d",
                 SD_MMC_CLK_GPIO, SD_MMC_CMD_GPIO, SD_MMC_D0_GPIO);
        return ret;
    }
    
    // Mark SD card as successfully initialized (for status display)
    sd_card_initialized = true;
    
    ESP_LOGI(TAG, "SD card mounted successfully at %s", SD_MOUNT_POINT);
    
    // Display card information
    sd_card_print_info();
    
    return ESP_OK;
}

/**
 * @brief Print SD card information to the console
 * 
 * Displays detailed information about the mounted SD card:
 * - Card name (manufacturer's name stored on card)
 * - Card type (SDSC = Standard Capacity <=2GB, SDHC = High Capacity <=32GB, 
 *              SDXC = Extended Capacity <=2TB)
 * - Card capacity in MB or GB
 * - Speed class if available
 * - Sector size and count (useful for USB MSC implementation)
 * 
 * This function should only be called after successful sd_card_init()
 * when the sd_card pointer is valid.
 */
static void sd_card_print_info(void)
{
    if (sd_card == NULL) {
        ESP_LOGW(TAG, "SD card not initialized - cannot print info");
        return;
    }
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "          SD CARD INFORMATION           ");
    ESP_LOGI(TAG, "========================================");
    
    // Card name - stored in CID (Card Identification) register
    // This is the product name set by the manufacturer
    ESP_LOGI(TAG, "Name: %s", sd_card->cid.name);
    
    // Card type detection based on OCR (Operation Conditions Register)
    // SDSC: Standard Capacity - up to 2GB, uses byte addressing
    // SDHC: High Capacity - 2GB to 32GB, uses block addressing
    // SDXC: Extended Capacity - 32GB to 2TB, uses block addressing
    const char *card_type;
    if (sd_card->ocr & SD_OCR_SDHC_CAP) {
        // Bit 30 in OCR indicates SDHC/SDXC
        // Capacity determines SDHC vs SDXC (32GB threshold)
        uint64_t capacity_bytes = (uint64_t)sd_card->csd.capacity * sd_card->csd.sector_size;
        if (capacity_bytes > 32ULL * 1024 * 1024 * 1024) {
            card_type = "SDXC (Extended Capacity)";
        } else {
            card_type = "SDHC (High Capacity)";
        }
    } else {
        card_type = "SDSC (Standard Capacity)";
    }
    ESP_LOGI(TAG, "Type: %s", card_type);
    
    // Calculate and display capacity
    // capacity is in sectors, sector_size is typically 512 bytes
    uint64_t capacity_bytes = (uint64_t)sd_card->csd.capacity * sd_card->csd.sector_size;
    uint32_t capacity_mb = capacity_bytes / (1024 * 1024);
    
    if (capacity_mb >= 1024) {
        // Display in GB for cards >= 1GB
        ESP_LOGI(TAG, "Capacity: %lu MB (%.2f GB)", 
                 (unsigned long)capacity_mb, 
                 (float)capacity_mb / 1024.0f);
    } else {
        ESP_LOGI(TAG, "Capacity: %lu MB", (unsigned long)capacity_mb);
    }
    
    // Sector information - important for USB MSC block device access
    ESP_LOGI(TAG, "Sector size: %d bytes", sd_card->csd.sector_size);
    ESP_LOGI(TAG, "Sector count: %lu", (unsigned long)sd_card->csd.capacity);
    
    // Display speed information
    // tr_speed is the maximum transfer speed from CSD register
    ESP_LOGI(TAG, "Max speed: %d kHz", sd_card->max_freq_khz);
    
    // Display CSD version (determines how capacity is calculated)
    ESP_LOGI(TAG, "CSD version: %d", sd_card->csd.csd_ver);
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "SD card ready for file operations");
    ESP_LOGI(TAG, "Mount point: %s", SD_MOUNT_POINT);
    ESP_LOGI(TAG, "========================================");
}

/**
 * @brief Unmount SD card and free resources
 * 
 * This function should be called before:
 * - Physically removing the SD card
 * - Switching to USB MSC mode (if needed)
 * - Shutting down the system
 * 
 * It ensures all file system buffers are flushed and the card
 * is safely unmounted to prevent data corruption.
 * 
 * @return ESP_OK on success, error code on failure
 */
static esp_err_t sd_card_unmount(void)
{
    if (sd_card == NULL) {
        ESP_LOGW(TAG, "SD card not mounted - nothing to unmount");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Unmounting SD card...");
    
    // Unmount the FAT filesystem
    // This flushes any pending writes and frees resources
    esp_err_t ret = esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, sd_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount SD card: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Note: SDMMC host is automatically cleaned up by the unmount function
    // (Unlike SPI mode, no separate bus free is needed)
    
    sd_card = NULL;
    sd_card_initialized = false;
    ESP_LOGI(TAG, "SD card unmounted successfully");
    
    return ESP_OK;
}

/* ============================================================================
 * FILE OPERATIONS (Phase 3)
 * ============================================================================ */

/**
 * @brief Create and write content to a new file on the SD card
 * 
 * This function creates a new file (or overwrites an existing one) and writes
 * the specified text content to it. Uses standard C file I/O functions which
 * work through the ESP-IDF VFS (Virtual File System) layer.
 * 
 * File path format:
 * - Must include mount point prefix: "/sdcard/filename.txt"
 * - Supports subdirectories: "/sdcard/folder/file.txt"
 * - Use forward slashes (/) as path separators
 * 
 * Write mode ("w"):
 * - Creates a new file if it doesn't exist
 * - Truncates (empties) the file if it already exists
 * - For appending to existing files, use file_append() instead
 * 
 * @param path Full path to the file (e.g., "/sdcard/test.txt")
 * @param content Text content to write to the file
 * @return ESP_OK on success, ESP_FAIL on error
 */
static esp_err_t file_write(const char *path, const char *content)
{
    ESP_LOGI(TAG, "Writing file: %s", path);
    
    // Check if USB MSC is active - avoid conflicts
    if (usb_msc_active) {
        ESP_LOGW(TAG, "USB MSC active - file write blocked to prevent corruption");
        return ESP_FAIL;
    }
    
    // Open file for writing
    // "w" mode: Create new file or truncate existing file
    // Returns NULL on failure (e.g., SD card full, invalid path)
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", path);
        return ESP_FAIL;
    }
    
    // Write content to file using fprintf
    // fprintf returns the number of characters written, or negative on error
    int written = fprintf(f, "%s", content);
    if (written < 0) {
        ESP_LOGE(TAG, "Failed to write content to file");
        fclose(f);
        return ESP_FAIL;
    }
    
    // Close the file to flush buffers and release resources
    // IMPORTANT: Always close files to ensure data is written to SD card
    fclose(f);
    
    ESP_LOGI(TAG, "File written successfully (%d bytes)", written);
    return ESP_OK;
}

/**
 * @brief Read and display the contents of a file from the SD card
 * 
 * This function opens an existing file, reads its contents line by line,
 * and prints them to the serial console. Useful for verifying file operations
 * and debugging.
 * 
 * Read mode ("r"):
 * - Opens existing file for reading
 * - File must exist, otherwise fopen returns NULL
 * - File position starts at beginning
 * 
 * Reading strategy:
 * - Uses fgets() to read one line at a time
 * - Handles lines up to 256 characters (adjust buffer if needed)
 * - Stops at end of file (EOF)
 * 
 * @param path Full path to the file (e.g., "/sdcard/test.txt")
 * @return ESP_OK on success, ESP_FAIL if file not found or read error
 */
static esp_err_t file_read(const char *path)
{
    ESP_LOGI(TAG, "Reading file: %s", path);
    
    // Check if USB MSC is active
    if (usb_msc_active) {
        ESP_LOGW(TAG, "USB MSC active - file read blocked to prevent corruption");
        return ESP_FAIL;
    }
    
    // Open file for reading
    // "r" mode: Open existing file for reading only
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading: %s (file may not exist)", path);
        return ESP_FAIL;
    }
    
    // Read and print file contents line by line
    // Buffer size of 256 should handle most text file lines
    char line[256];
    
    ESP_LOGI(TAG, "--- File contents ---");
    
    // fgets reads until newline, EOF, or buffer full
    // Returns NULL when no more data to read
    while (fgets(line, sizeof(line), f) != NULL) {
        // Print without adding extra newline (fgets keeps the \n)
        printf("%s", line);
    }
    
    // Ensure output ends with newline for clean formatting
    printf("\n");
    ESP_LOGI(TAG, "--- End of file ---");
    
    fclose(f);
    return ESP_OK;
}

/**
 * @brief Append content to an existing file on the SD card
 * 
 * This function opens a file in append mode and adds new content to the end
 * without modifying existing content. If the file doesn't exist, it creates
 * a new one.
 * 
 * Append mode ("a"):
 * - Opens file for writing at the end
 * - Creates file if it doesn't exist
 * - Preserves existing content (unlike "w" mode)
 * - All writes go to end of file, even if you seek elsewhere
 * 
 * Use cases:
 * - Log files that accumulate data over time
 * - Adding new records to a data file
 * - Appending timestamps or measurements
 * 
 * @param path Full path to the file (e.g., "/sdcard/log.txt")
 * @param content Text content to append to the file
 * @return ESP_OK on success, ESP_FAIL on error
 */
static esp_err_t file_append(const char *path, const char *content)
{
    ESP_LOGI(TAG, "Appending to file: %s", path);
    
    // Check if USB MSC is active
    if (usb_msc_active) {
        ESP_LOGW(TAG, "USB MSC active - file append blocked to prevent corruption");
        return ESP_FAIL;
    }
    
    // Open file in append mode
    // "a" mode: Open for appending; create if doesn't exist
    FILE *f = fopen(path, "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for appending: %s", path);
        return ESP_FAIL;
    }
    
    // Write content at end of file
    int written = fprintf(f, "%s", content);
    if (written < 0) {
        ESP_LOGE(TAG, "Failed to append content to file");
        fclose(f);
        return ESP_FAIL;
    }
    
    fclose(f);
    
    ESP_LOGI(TAG, "Content appended successfully (%d bytes)", written);
    return ESP_OK;
}

/**
 * @brief List all files and directories in a given directory path
 * 
 * This function opens a directory and iterates through all entries,
 * printing information about each file or subdirectory found.
 * Uses POSIX directory functions (opendir, readdir, closedir).
 * 
 * Directory entry information:
 * - d_name: Name of the file/directory (not full path)
 * - d_type: Type of entry (DT_REG for file, DT_DIR for directory)
 * - For file sizes, we use stat() to get detailed information
 * 
 * Note: FAT file system on SD card may not support all Unix file attributes.
 * Some fields like permissions may show default values.
 * 
 * @param path Full path to the directory (e.g., "/sdcard" or "/sdcard/folder")
 * @return ESP_OK on success, ESP_FAIL if directory not found or error
 */
static esp_err_t directory_list(const char *path)
{
    ESP_LOGI(TAG, "Listing directory: %s", path);
    
    // Check if USB MSC is active
    if (usb_msc_active) {
        ESP_LOGW(TAG, "USB MSC active - directory listing blocked");
        return ESP_FAIL;
    }
    
    // Open the directory
    // opendir returns NULL if directory doesn't exist or can't be opened
    DIR *dir = opendir(path);
    if (dir == NULL) {
        ESP_LOGE(TAG, "Failed to open directory: %s", path);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Directory: %s", path);
    ESP_LOGI(TAG, "----------------------------------------");
    
    // Read directory entries one at a time
    struct dirent *entry;
    int file_count = 0;
    int dir_count = 0;
    
    // readdir returns NULL when no more entries
    while ((entry = readdir(dir)) != NULL) {
        // Build full path for stat() call
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        // Get file/directory statistics
        struct stat entry_stat;
        if (stat(full_path, &entry_stat) == -1) {
            // If stat fails, just print name without size
            ESP_LOGI(TAG, "  %-20s (stat failed)", entry->d_name);
            continue;
        }
        
        // Check if entry is a directory or regular file
        if (S_ISDIR(entry_stat.st_mode)) {
            // Directory entry
            ESP_LOGI(TAG, "  [DIR]  %-20s", entry->d_name);
            dir_count++;
        } else {
            // Regular file - show size
            // Format size in bytes, KB, or MB depending on size
            if (entry_stat.st_size < 1024) {
                ESP_LOGI(TAG, "  [FILE] %-20s  %ld bytes", 
                         entry->d_name, (long)entry_stat.st_size);
            } else if (entry_stat.st_size < 1024 * 1024) {
                ESP_LOGI(TAG, "  [FILE] %-20s  %.1f KB", 
                         entry->d_name, (float)entry_stat.st_size / 1024.0f);
            } else {
                ESP_LOGI(TAG, "  [FILE] %-20s  %.2f MB", 
                         entry->d_name, (float)entry_stat.st_size / (1024.0f * 1024.0f));
            }
            file_count++;
        }
    }
    
    // Close the directory
    closedir(dir);
    
    ESP_LOGI(TAG, "----------------------------------------");
    ESP_LOGI(TAG, "Total: %d file(s), %d directory(ies)", file_count, dir_count);
    ESP_LOGI(TAG, "========================================");
    
    // Handle empty directory case
    if (file_count == 0 && dir_count == 0) {
        ESP_LOGI(TAG, "(Directory is empty)");
    }
    
    return ESP_OK;
}

/**
 * @brief Demonstrate file operations on the SD card
 * 
 * This function runs a series of file operation tests to verify
 * that the SD card and FAT file system are working correctly:
 * 
 * 1. List initial directory contents
 * 2. Create and write a test file
 * 3. Read the file back
 * 4. Append additional content
 * 5. Read again to verify append
 * 6. List directory to show new file
 * 
 * This demo is useful for:
 * - Verifying SD card setup
 * - Testing file system functionality
 * - Demonstrating the file operation API
 */
static void file_operations_demo(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "    FILE OPERATIONS DEMONSTRATION");
    ESP_LOGI(TAG, "========================================");
    
    // Test file path
    const char *test_file = SD_MOUNT_POINT "/test.txt";
    
    // Step 1: List initial directory contents
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "[Step 1] Listing initial directory contents...");
    directory_list(SD_MOUNT_POINT);
    
    // Step 2: Create and write a test file
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "[Step 2] Creating test file...");
    const char *initial_content = "Hello from ESP32-S3!\n"
                                   "This file was created by the SD card demo.\n"
                                   "Date: 2026-02-17\n";
    
    if (file_write(test_file, initial_content) != ESP_OK) {
        ESP_LOGE(TAG, "File write test FAILED");
        return;
    }
    
    // Step 3: Read the file back
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "[Step 3] Reading file contents...");
    if (file_read(test_file) != ESP_OK) {
        ESP_LOGE(TAG, "File read test FAILED");
        return;
    }
    
    // Step 4: Append additional content
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "[Step 4] Appending to file...");
    const char *append_content = "--- Appended content ---\n"
                                  "This line was added using file_append().\n";
    
    if (file_append(test_file, append_content) != ESP_OK) {
        ESP_LOGE(TAG, "File append test FAILED");
        return;
    }
    
    // Step 5: Read again to verify append
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "[Step 5] Reading file after append...");
    if (file_read(test_file) != ESP_OK) {
        ESP_LOGE(TAG, "File read after append FAILED");
        return;
    }
    
    // Step 6: List directory to show new file
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "[Step 6] Listing directory after file operations...");
    directory_list(SD_MOUNT_POINT);
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  FILE OPERATIONS DEMO COMPLETE");
    ESP_LOGI(TAG, "========================================");
}

/* ============================================================================
 * USB MASS STORAGE IMPLEMENTATION (Phase 4)
 * ============================================================================
 * 
 * USB Mass Storage Class (MSC) allows the ESP32-S3 to appear as a removable
 * USB drive to a connected PC. This enables direct file management through
 * the operating system's file explorer.
 * 
 * IMPORTANT: USB MSC vs POSIX File I/O
 * ------------------------------------
 * The assignment specifies "no POSIX" for USB MSC. This means the USB stack
 * must access the SD card at the block/sector level, NOT through file I/O.
 * 
 * How it works internally:
 * - PC sends SCSI commands (READ10, WRITE10) for specific sectors
 * - ESP32 translates these to sdmmc_read_sectors() / sdmmc_write_sectors()
 * - Data is transferred at the block level (typically 512 bytes per sector)
 * - No file system interpretation happens on ESP32 during USB operations
 * 
 * The esp_tinyusb component handles this automatically with callbacks:
 * - tud_msc_capacity_cb(): Reports SD card capacity (sector count and size)
 * - tud_msc_read10_cb(): Reads sectors using sdmmc_read_sectors()
 * - tud_msc_write10_cb(): Writes sectors using sdmmc_write_sectors()
 * - tud_msc_scsi_cb(): Handles other SCSI commands (inquiry, mode sense, etc.)
 * 
 * Concurrent Access Prevention:
 * - When USB is actively accessing storage, FAT VFS is unmounted
 * - Local file operations are blocked via usb_msc_active flag
 * - Storage is remounted when USB disconnects
 * ============================================================================ */

// TinyUSB MSC Descriptors
// These describe the USB device to the host PC
#define EPNUM_MSC       1   // Endpoint number for MSC
#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)

// Interface numbering
enum {
    ITF_NUM_MSC = 0,        // MSC interface number
    ITF_NUM_TOTAL           // Total number of interfaces
};

// Endpoint addresses
enum {
    EDPT_CTRL_OUT = 0x00,   // Control endpoint OUT
    EDPT_CTRL_IN  = 0x80,   // Control endpoint IN
    EDPT_MSC_OUT  = 0x01,   // MSC bulk endpoint OUT (PC -> ESP32)
    EDPT_MSC_IN   = 0x81,   // MSC bulk endpoint IN (ESP32 -> PC)
};

/**
 * USB Device Descriptor
 * 
 * This structure describes the USB device to the host:
 * - VID/PID: Vendor and Product ID (Espressif VID used here)
 * - Device class: MISC (miscellaneous) with IAD (Interface Association Descriptor)
 * - Strings: Manufacturer, product name, serial number indices
 */
static tusb_desc_device_t usb_device_descriptor = {
    .bLength = sizeof(usb_device_descriptor),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,                   // USB 2.0
    .bDeviceClass = TUSB_CLASS_MISC,    // Miscellaneous class
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USB_VID,                // Espressif VID (0x303A)
    .idProduct = USB_PID,               // Product ID (0x4002)
    .bcdDevice = 0x0100,                // Device version 1.00
    .iManufacturer = 0x01,              // String index for manufacturer
    .iProduct = 0x02,                   // String index for product
    .iSerialNumber = 0x03,              // String index for serial
    .bNumConfigurations = 0x01          // One configuration
};

/**
 * USB Configuration Descriptor
 * 
 * Describes the device configuration including:
 * - Power requirements (100mA)
 * - Interface descriptor for MSC
 * - Endpoint descriptors for bulk IN/OUT
 */
static uint8_t const usb_msc_configuration_desc[] = {
    // Configuration descriptor: config number, interface count, string index, total length, attributes, power
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    
    // MSC descriptor: interface number, string index, endpoint OUT, endpoint IN, endpoint size
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EDPT_MSC_OUT, EDPT_MSC_IN, 64),
};

/**
 * USB String Descriptors
 * 
 * Human-readable strings shown to the PC:
 * - Language (English US)
 * - Manufacturer name
 * - Product name
 * - Serial number
 * - MSC interface name
 */
static char const *usb_string_descriptors[] = {
    (const char[]) { 0x09, 0x04 },  // Index 0: Supported language (English 0x0409)
    USB_MANUFACTURER,               // Index 1: Manufacturer string
    USB_PRODUCT,                    // Index 2: Product string
    "ESP32S3-001",                  // Index 3: Serial number string
    "SD Card Storage",              // Index 4: MSC interface string
};

/**
 * @brief Callback when USB MSC mount state changes
 * 
 * This function is called by the TinyUSB stack when the PC mounts or unmounts
 * the USB storage device. We use this to coordinate access:
 * 
 * When mounted (PC accessing):
 * - Set usb_msc_active = true to block local file operations
 * - FAT VFS is automatically unmounted by tinyusb_msc_storage
 * 
 * When unmounted (PC disconnected/ejected):
 * - Set usb_msc_active = false to allow local file operations
 * - FAT VFS can be remounted for local access
 * 
 * @param event Mount event (MOUNT or UNMOUNT)
 * @param arg User argument (unused)
 */
static void usb_msc_mount_changed_cb(tinyusb_msc_event_t *event)
{
    if (event->mount_changed_data.is_mounted) {
        // PC has mounted the storage - block local access
        ESP_LOGI(TAG, "USB MSC: Storage mounted by host PC");
        usb_msc_active = true;
    } else {
        // PC has unmounted/ejected - allow local access
        ESP_LOGI(TAG, "USB MSC: Storage unmounted by host PC");
        usb_msc_active = false;
    }
}

/**
 * @brief Initialize USB Mass Storage device
 * 
 * Sets up the ESP32-S3 as a USB Mass Storage device that exposes the SD card
 * to a connected PC. The PC will see it as a removable drive.
 * 
 * Initialization steps:
 * 1. Configure TinyUSB MSC storage with SD card (uses direct sector access)
 * 2. Register mount change callback for access coordination
 * 3. Mount storage locally (for initial file operations demo)
 * 4. Install TinyUSB driver with MSC configuration
 * 
 * After initialization:
 * - Connect ESP32-S3 USB port to PC
 * - PC should detect a new removable drive
 * - Files can be browsed, created, modified from PC
 * - Local ESP32 file operations blocked while PC has storage mounted
 * 
 * @return ESP_OK on success, error code on failure
 */
static esp_err_t usb_msc_init(void)
{
    ESP_LOGI(TAG, "Initializing USB Mass Storage...");
    
    // Check if SD card is available
    if (sd_card == NULL) {
        ESP_LOGE(TAG, "SD card not initialized - cannot start USB MSC");
        return ESP_FAIL;
    }
    
    /* ========================================================================
     * STEP 1: Configure TinyUSB MSC storage with SD card
     * ========================================================================
     * 
     * The tinyusb_msc_sdmmc_config_t structure configures the MSC storage:
     * - card: Pointer to our SD card structure (works for both SPI and SDMMC modes)
     * - callback_mount_changed: Called when PC mounts/unmounts the drive
     * - mount_config: FAT mount options when remounting locally
     * 
     * IMPORTANT: This uses sdmmc_read_sectors() and sdmmc_write_sectors()
     * internally for USB operations, NOT POSIX file I/O (fopen/fread/fwrite).
     * This is the "no POSIX" approach required by the assignment.
     */
    const tinyusb_msc_sdmmc_config_t msc_sdmmc_config = {
        .card = sd_card,                                    // Our SPI-mounted SD card
        .callback_mount_changed = usb_msc_mount_changed_cb, // Mount state callback
        .mount_config = {
            .max_files = 5,                                 // Max open files when mounted locally
        },
    };
    
    // Initialize MSC storage - this registers the SD card with TinyUSB
    esp_err_t ret = tinyusb_msc_storage_init_sdmmc(&msc_sdmmc_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize MSC storage: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "MSC storage initialized with SD card");
    
    /* ========================================================================
     * STEP 2: Mount storage locally
     * ========================================================================
     * 
     * After MSC init, we need to mount the storage for local access.
     * This can be unmounted later when we want to expose to USB.
     * Note: We already have FAT mounted from Phase 2, so we might need 
     * to handle this carefully.
     */
    // Storage will be mounted by the MSC component when not in use by USB
    
    /* ========================================================================
     * STEP 3: Configure and install TinyUSB driver
     * ========================================================================
     * 
     * The tinyusb_config_t structure configures the USB device:
     * - device_descriptor: USB device descriptor (VID, PID, etc.)
     * - string_descriptor: Human-readable strings
     * - configuration_descriptor: Interfaces and endpoints
     * - external_phy: false = use internal USB PHY
     */
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = &usb_device_descriptor,
        .string_descriptor = usb_string_descriptors,
        .string_descriptor_count = sizeof(usb_string_descriptors) / sizeof(usb_string_descriptors[0]),
        .external_phy = false,  // Use ESP32-S3 internal USB PHY
        .configuration_descriptor = usb_msc_configuration_desc,
    };
    
    // Install TinyUSB driver
    ret = tinyusb_driver_install(&tusb_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install TinyUSB driver: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "USB MSC initialization complete!");
    ESP_LOGI(TAG, "Connect USB cable to PC to access SD card as removable drive");
    ESP_LOGI(TAG, "VID: 0x%04X, PID: 0x%04X", USB_VID, USB_PID);
    
    return ESP_OK;
}

/**
 * @brief USB MSC task (optional - TinyUSB handles this internally)
 * 
 * Note: In ESP-IDF with esp_tinyusb component, the TinyUSB task is created
 * and managed automatically by tinyusb_driver_install(). This function is
 * provided for completeness and could be used for additional USB-related
 * processing if needed.
 * 
 * The internal TinyUSB task:
 * - Runs tud_task() to process USB events
 * - Handles USB enumeration and MSC commands
 * - Calls our mount_changed callback when PC mounts/unmounts
 */
static void usb_msc_task(void *pvParameters)
{
    ESP_LOGI(TAG, "USB MSC task started (note: TinyUSB task runs automatically)");
    
    // The TinyUSB component manages its own task internally
    // This task can monitor USB status or perform other processing
    while (1) {
        // Check USB MSC status periodically
        if (tinyusb_msc_storage_in_use_by_usb_host()) {
            // PC is actively using the storage
            // usb_msc_active flag is already set by callback
        }
        
        // Sleep to avoid hogging CPU
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief Expose SD card to USB host (unmount local, allow USB access)
 * 
 * Call this function when you want to let the PC access the SD card.
 * Local file operations will be blocked until the PC ejects the drive.
 * 
 * @return ESP_OK on success, error code on failure
 */
static esp_err_t usb_msc_expose_to_host(void)
{
    ESP_LOGI(TAG, "Exposing SD card to USB host...");
    
    // Check if already exposed
    if (tinyusb_msc_storage_in_use_by_usb_host()) {
        ESP_LOGW(TAG, "Storage already in use by USB host");
        return ESP_OK;
    }
    
    // Unmount local FAT filesystem to allow USB access
    esp_err_t ret = tinyusb_msc_storage_unmount();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount storage: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "SD card now accessible via USB. Plug in USB cable to PC.");
    return ESP_OK;
}

/**
 * @brief Reclaim SD card from USB (mount local for ESP32 access)
 * 
 * Call this function after the PC has ejected the drive or when you want
 * to perform local file operations. The SD card will be mounted locally.
 * 
 * WARNING: Do not call this while PC still has the drive mounted!
 * Always safely eject from PC first.
 * 
 * @return ESP_OK on success, error code on failure
 */
static esp_err_t usb_msc_mount_locally(void)
{
    ESP_LOGI(TAG, "Mounting SD card locally...");
    
    // Check if USB host is still using storage
    if (tinyusb_msc_storage_in_use_by_usb_host()) {
        ESP_LOGW(TAG, "Storage still in use by USB host - cannot mount locally");
        return ESP_FAIL;
    }
    
    // Mount FAT filesystem locally
    esp_err_t ret = tinyusb_msc_storage_mount(SD_MOUNT_POINT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount storage locally: %s", esp_err_to_name(ret));
        return ret;
    }
    
    usb_msc_active = false;
    ESP_LOGI(TAG, "SD card mounted locally at %s", SD_MOUNT_POINT);
    return ESP_OK;
}

/* ============================================================================
 * I2C LCD DISPLAY IMPLEMENTATION (Phase 5)
 * ============================================================================
 * 
 * This section implements I2C communication with an HD44780-compatible LCD
 * display using a PCF8574 I/O expander backpack. The PCF8574 converts I2C
 * commands into parallel signals for the LCD controller.
 * 
 * HARDWARE REQUIREMENTS:
 * ----------------------
 * IMPORTANT: External 1k Ohm pull-up resistors are REQUIRED on SDA and SCL lines!
 * 
 * The ESP32-S3 internal pull-ups (typically 45k Ohm) are too weak for reliable
 * I2C communication, especially with the LCD backpack capacitance.
 * 
 * Pull-up calculation:
 * - I2C spec recommends rise time < 1us for 100kHz operation
 * - With ~10pF bus capacitance: R_pull-up <= 1us / (10pF * 2.3) = ~43k Ohm
 * - With LCD backpack (~100pF): R_pull-up <= 1us / (100pF * 2.3) = ~4.3k Ohm
 * - Using 1k Ohm provides adequate margin and faster rise times
 * 
 * Wiring diagram:
 * 
 *     3.3V
 *      |
 *     [1k]  <-- External pull-up resistor (REQUIRED)
 *      |
 *      +-------- GPIO 8 (SDA) -------- LCD Backpack SDA
 *      
 *     3.3V
 *      |
 *     [1k]  <-- External pull-up resistor (REQUIRED)
 *      |
 *      +-------- GPIO 9 (SCL) -------- LCD Backpack SCL
 *      
 *     5V/3.3V -------------- LCD Backpack VCC (check your module)
 *     GND ------------------- LCD Backpack GND
 * 
 * PCF8574 BACKPACK BIT MAPPING:
 * -----------------------------
 * The PCF8574 8-bit output connects to the LCD as follows:
 * 
 *   Bit 7 (P7): D7 (LCD data bit 7)
 *   Bit 6 (P6): D6 (LCD data bit 6)
 *   Bit 5 (P5): D5 (LCD data bit 5)
 *   Bit 4 (P4): D4 (LCD data bit 4)
 *   Bit 3 (P3): BL (Backlight control, active high)
 *   Bit 2 (P2): E  (Enable pulse)
 *   Bit 1 (P1): RW (Read/Write, 0=Write, 1=Read)
 *   Bit 0 (P0): RS (Register Select, 0=Command, 1=Data)
 * 
 * 4-BIT MODE OPERATION:
 * ---------------------
 * In 4-bit mode, each byte is sent as two nibbles (high nibble first):
 * 1. Send high nibble on D7-D4 with E=0
 * 2. Pulse E high then low (data latched on falling edge)
 * 3. Send low nibble on D7-D4 with E=0
 * 4. Pulse E high then low
 */

// PCF8574 backpack bit positions for LCD control
#define LCD_BIT_RS          (1 << 0)  // Register Select: 0=Command, 1=Data
#define LCD_BIT_RW          (1 << 1)  // Read/Write: 0=Write, 1=Read (always 0 for us)
#define LCD_BIT_E           (1 << 2)  // Enable pulse (data latched on falling edge)
#define LCD_BIT_BL          (1 << 3)  // Backlight control (1=on, 0=off)

// HD44780 LCD commands (from datasheet)
#define LCD_CMD_CLEAR       0x01      // Clear display (also returns cursor home)
#define LCD_CMD_HOME        0x02      // Return cursor to home position
#define LCD_CMD_ENTRY_MODE  0x06      // Entry mode: increment cursor, no shift
#define LCD_CMD_DISPLAY_ON  0x0C      // Display ON, cursor OFF, blink OFF
#define LCD_CMD_DISPLAY_OFF 0x08      // Display OFF
#define LCD_CMD_FUNCTION_4B 0x28      // 4-bit mode, 2 lines, 5x8 font
#define LCD_CMD_FUNCTION_8B 0x38      // 8-bit mode, 2 lines, 5x8 font
#define LCD_CMD_SET_DDRAM   0x80      // Set DDRAM address (OR with address)

// LCD timing delays (generous values for I2C-based LCD with PCF8574)
// Note: I2C communication adds ~100us per byte at 100kHz, so we use
// longer delays than the HD44780 datasheet minimums for reliability
#define LCD_DELAY_INIT_MS       200   // Power-on delay before initialization 
#define LCD_DELAY_INIT_1_MS     20    // First init command delay (>4.1ms)
#define LCD_DELAY_INIT_2_US     2000  // Second init command delay (>100us)
#define LCD_DELAY_CMD_US        200   // Standard command execution time
#define LCD_DELAY_CLEAR_MS      10    // Clear command needs longer (>1.52ms)
#define LCD_DELAY_ENABLE_US     100   // Enable pulse width (>450ns, longer for I2C)

// Some 1602IIC modules have reversed data lines (D7-D4 on P4-P7 instead of P7-P4)
// Set to 1 if your display shows garbage, 0 for standard wiring
#define LCD_REVERSE_NIBBLE      0

// LCD row start addresses for common display sizes
// HD44780 DDRAM is organized non-contiguously for rows
static const uint8_t lcd_row_offsets[4] = {
    0x00,   // Row 0 starts at address 0x00
    0x40,   // Row 1 starts at address 0x40
    0x14,   // Row 2 starts at address 0x14 (for 20x4 displays)
    0x54    // Row 3 starts at address 0x54 (for 20x4 displays)
};

// Current backlight state (preserved across operations)
static uint8_t lcd_backlight_state = LCD_BIT_BL;  // Default: backlight ON

/**
 * @brief Initialize I2C master interface for LCD communication
 * 
 * Configures the ESP32-S3 I2C controller as a master device:
 * - Sets up GPIO pins for SDA (data) and SCL (clock)
 * - Enables internal pull-ups (but external 1k pull-ups are still required!)
 * - Sets clock frequency to 100kHz (standard I2C speed)
 * 
 * HARDWARE NOTE: 
 * The internal pull-ups (~45k Ohm) are enabled as a fallback, but they are
 * NOT sufficient for reliable LCD operation. You MUST use external 1k Ohm
 * pull-up resistors on both SDA and SCL lines.
 * 
 * If I2C communication is unreliable (garbled display, no response):
 * 1. Check that 1k Ohm pull-ups are connected from SDA/SCL to 3.3V
 * 2. Verify the LCD module I2C address (use an I2C scanner)
 * 3. Check power supply (LCD backpack needs 5V VCC typically)
 * 
 * @return ESP_OK on success, error code on failure
 */
static esp_err_t i2c_init(void)
{
    ESP_LOGI(TAG, "Initializing I2C master for LCD...");
    ESP_LOGW(TAG, "IMPORTANT: Ensure 1k Ohm pull-up resistors are connected!");
    ESP_LOGI(TAG, "  SDA (GPIO %d) --> 1k Ohm --> 3.3V", I2C_SDA_GPIO);
    ESP_LOGI(TAG, "  SCL (GPIO %d) --> 1k Ohm --> 3.3V", I2C_SCL_GPIO);
    
    /* I2C master configuration structure
     * 
     * mode: I2C_MODE_MASTER - we control the clock and initiate transfers
     * sda_io_num/scl_io_num: GPIO pins for data and clock
     * sda_pullup_en/scl_pullup_en: Enable weak internal pull-ups (backup only)
     * master.clk_speed: Clock frequency in Hz (100kHz is standard)
     */
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,              // ESP32 is the master
        .sda_io_num = I2C_SDA_GPIO,           // GPIO 8 for data
        .scl_io_num = I2C_SCL_GPIO,           // GPIO 9 for clock
        .sda_pullup_en = GPIO_PULLUP_ENABLE,  // Enable internal pull-up (weak)
        .scl_pullup_en = GPIO_PULLUP_ENABLE,  // Enable internal pull-up (weak)
        .master.clk_speed = I2C_FREQ_HZ,      // 100kHz clock speed
    };
    
    // Apply the configuration to the I2C port
    esp_err_t ret = i2c_param_config(I2C_PORT_NUM, &i2c_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C parameter configuration failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Install the I2C driver
    // Parameters: port number, mode, RX buffer size, TX buffer size, interrupt flags
    // For master mode, RX/TX buffers are not used (set to 0)
    ret = i2c_driver_install(I2C_PORT_NUM, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver installation failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "I2C master initialized successfully");
    ESP_LOGI(TAG, "  Port: I2C_NUM_%d", I2C_PORT_NUM);
    ESP_LOGI(TAG, "  SDA: GPIO %d", I2C_SDA_GPIO);
    ESP_LOGI(TAG, "  SCL: GPIO %d", I2C_SCL_GPIO);
    ESP_LOGI(TAG, "  Speed: %d Hz", I2C_FREQ_HZ);
    
    // Scan I2C bus to find connected devices (helps identify LCD address)
    ESP_LOGI(TAG, "Scanning I2C bus for devices...");
    int devices_found = 0;
    for (uint8_t addr = 0x20; addr <= 0x3F; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t result = i2c_master_cmd_begin(I2C_PORT_NUM, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);
        if (result == ESP_OK) {
            ESP_LOGI(TAG, "  Found device at address 0x%02X", addr);
            devices_found++;
        }
    }
    if (devices_found == 0) {
        ESP_LOGW(TAG, "No I2C devices found! Check SDA/SCL wiring.");
    }
    
    return ESP_OK;
}

/**
 * @brief Write a byte to the PCF8574 I/O expander
 * 
 * Sends a single byte over I2C to the PCF8574 chip on the LCD backpack.
 * The PCF8574 latches this byte and outputs it on pins P0-P7.
 * 
 * @param data Byte to write (bits correspond to LCD control lines)
 * @return ESP_OK on success, error code on failure
 */
static esp_err_t lcd_i2c_write_byte(uint8_t data)
{
    // Create I2C command link (transaction builder)
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    // Build the I2C transaction:
    // 1. Generate START condition
    // 2. Send slave address with WRITE bit
    // 3. Send data byte
    // 4. Generate STOP condition
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (LCD_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    
    // Execute the transaction with 100ms timeout
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT_NUM, cmd, pdMS_TO_TICKS(100));
    
    // Free the command link
    i2c_cmd_link_delete(cmd);
    
    return ret;
}

/**
 * @brief Send an enable pulse to the LCD
 * 
 * The HD44780 latches data on the falling edge of the Enable (E) signal.
 * This function:
 * 1. Sets E high (with data already on D4-D7)
 * 2. Waits for >450ns (we use 1us for safety)
 * 3. Sets E low (data is latched on this edge)
 * 4. Waits for command execution
 * 
 * @param data Data byte with control bits (RS, RW, backlight)
 */
static void lcd_pulse_enable(uint8_t data)
{
    // Set Enable high - wait for I2C to complete
    lcd_i2c_write_byte(data | LCD_BIT_E);
    esp_rom_delay_us(500);  // 500us for I2C completion
    
    // Set Enable low (data latched on falling edge)
    lcd_i2c_write_byte(data & ~LCD_BIT_E);
    esp_rom_delay_us(500);  // 500us for command execution
}

/**
 * @brief Send a 4-bit nibble to the LCD
 * 
 * In 4-bit mode, we send data using only D4-D7 pins.
 * This function sends one nibble (4 bits) in the upper 4 bits of the byte.
 * 
 * @param nibble The 4-bit value to send (in upper 4 bits: 0xX0)
 * @param rs_flag Register Select: 0=Command, LCD_BIT_RS=Data
 */
static void lcd_write_nibble(uint8_t nibble, uint8_t rs_flag)
{
    // Apply nibble reversal if configured (for modules with swapped D4-D7 lines)
#if LCD_REVERSE_NIBBLE
    // Reverse bits: swap bit positions 7<->4, 6<->5
    uint8_t reversed = 0;
    if (nibble & 0x80) reversed |= 0x10;  // bit 7 -> bit 4
    if (nibble & 0x40) reversed |= 0x20;  // bit 6 -> bit 5
    if (nibble & 0x20) reversed |= 0x40;  // bit 5 -> bit 6
    if (nibble & 0x10) reversed |= 0x80;  // bit 4 -> bit 7
    nibble = reversed;
#endif
    
    // Combine nibble data with control bits
    // - Upper 4 bits: data (D4-D7)
    // - Bit 3: backlight state
    // - Bit 2: Enable (controlled by pulse_enable)
    // - Bit 1: RW (always 0 for write)
    // - Bit 0: RS (0=command, 1=data)
    uint8_t data = (nibble & 0xF0) | lcd_backlight_state | rs_flag;
    
    // Write data with E low, then pulse E
    lcd_i2c_write_byte(data);
    lcd_pulse_enable(data);
}

/**
 * @brief Send a byte to the LCD in 4-bit mode
 * 
 * In 4-bit mode, each byte is transmitted as two sequential nibbles:
 * 1. High nibble (bits 7-4) first
 * 2. Low nibble (bits 3-0) second
 * 
 * This requires two enable pulses per byte.
 * 
 * @param byte The 8-bit value to send
 * @param rs_flag Register Select: 0=Command, LCD_BIT_RS=Data
 */
static void lcd_write_byte(uint8_t byte, uint8_t rs_flag)
{
    // Send high nibble first (bits 7-4 in positions 7-4)
    lcd_write_nibble(byte & 0xF0, rs_flag);
    
    // Send low nibble second (bits 3-0 shifted to positions 7-4)
    lcd_write_nibble((byte << 4) & 0xF0, rs_flag);
}

/**
 * @brief Send a command to the LCD
 * 
 * Commands control LCD behavior (clear, cursor position, display mode, etc.)
 * The Register Select (RS) pin must be LOW for commands.
 * 
 * @param cmd Command byte to send
 */
static void lcd_send_command(uint8_t cmd)
{
    lcd_write_byte(cmd, 0);  // RS=0 for command
}

/**
 * @brief Send data (character) to the LCD
 * 
 * Data is written to the current DDRAM address and displayed.
 * The cursor automatically advances after each character.
 * The Register Select (RS) pin must be HIGH for data.
 * 
 * @param data Character code to display (ASCII or custom character 0-7)
 */
static void lcd_send_data(uint8_t data)
{
    lcd_write_byte(data, LCD_BIT_RS);  // RS=1 for data
    // Allow LCD time to process the character (HD44780 needs ~37us)
    esp_rom_delay_us(100);
}

/**
 * @brief Initialize the LCD display in 4-bit mode
 * 
 * This function performs the HD44780 initialization sequence as specified
 * in the datasheet. The LCD powers up in 8-bit mode, so we must carefully
 * switch it to 4-bit mode using a specific sequence.
 * 
 * INITIALIZATION SEQUENCE (from HD44780 datasheet):
 * 1. Wait >40ms after power-on
 * 2. Send 0x03 (Function Set 8-bit) - LCD might be in unknown state
 * 3. Wait >4.1ms
 * 4. Send 0x03 again
 * 5. Wait >100us
 * 6. Send 0x03 again
 * 7. Send 0x02 (switch to 4-bit mode)
 * 8. Now in 4-bit mode, send configuration commands
 * 
 * After initialization, the LCD is configured for:
 * - 4-bit interface (D4-D7 only)
 * - 2-line display
 * - 5x8 dot character font
 * - Display ON, cursor OFF, blink OFF
 * - Entry mode: cursor moves right, no display shift
 * 
 * @return ESP_OK on success, error code on failure
 */
static esp_err_t lcd_init(void)
{
    ESP_LOGI(TAG, "Initializing LCD in 4-bit mode...");
    ESP_LOGI(TAG, "LCD I2C address: 0x%02X", LCD_I2C_ADDR);
    ESP_LOGI(TAG, "LCD dimensions: %d columns x %d rows", LCD_COLS, LCD_ROWS);
    
    // Wait for LCD controller to power up (>40ms from power-on)
    vTaskDelay(pdMS_TO_TICKS(LCD_DELAY_INIT_MS));
    
    // Initialize backlight to ON state
    lcd_backlight_state = LCD_BIT_BL;
    lcd_i2c_write_byte(lcd_backlight_state);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    /* ====================================================================
     * HD44780 INITIALIZATION SEQUENCE FOR 4-BIT MODE
     * ====================================================================
     * 
     * The LCD powers up in 8-bit mode and might be in an unknown state.
     * We send the Function Set command three times to reliably reset it,
     * then switch to 4-bit mode.
     * 
     * Note: During initialization, we send only the upper nibble (0x30)
     * because the LCD is still in 8-bit mode and ignores the lower nibble.
     * 
     * IMPORTANT: We use generous delays for I2C-based LCD reliability.
     */
    
    // Step 1: Send 0x30 (Function Set: 8-bit mode) - first attempt
    // This works whether LCD is in 8-bit or 4-bit mode
    lcd_write_nibble(0x30, 0);  // 0011 xxxx = 8-bit mode
    vTaskDelay(pdMS_TO_TICKS(LCD_DELAY_INIT_1_MS));  // Wait >4.1ms
    
    // Step 2: Send 0x30 again - second attempt
    lcd_write_nibble(0x30, 0);
    vTaskDelay(pdMS_TO_TICKS(5));  // Wait 5ms (generous for I2C)
    
    // Step 3: Send 0x30 again - third attempt (LCD is now reliably in 8-bit mode)
    lcd_write_nibble(0x30, 0);
    vTaskDelay(pdMS_TO_TICKS(5));  // Wait 5ms
    
    // Step 4: Switch to 4-bit mode by sending 0x20
    // After this, all subsequent communication is 4-bit (two nibbles per byte)
    lcd_write_nibble(0x20, 0);  // 0010 xxxx = 4-bit mode
    vTaskDelay(pdMS_TO_TICKS(5));  // Wait 5ms for mode switch
    
    /* ====================================================================
     * LCD CONFIGURATION (now in 4-bit mode)
     * ====================================================================
     */
    
    // Function Set: 4-bit mode, 2 lines, 5x8 font
    // 0x28 = 0010 1000 = DL=0 (4-bit), N=1 (2 lines), F=0 (5x8 font)
    lcd_send_command(LCD_CMD_FUNCTION_4B);
    vTaskDelay(pdMS_TO_TICKS(2));
    
    // Display Control: Display ON, cursor OFF, blink OFF
    // 0x0C = 0000 1100 = D=1 (display on), C=0 (cursor off), B=0 (blink off)
    lcd_send_command(LCD_CMD_DISPLAY_ON);
    vTaskDelay(pdMS_TO_TICKS(2));
    
    // Clear Display: Clear all characters and return cursor home
    lcd_send_command(LCD_CMD_CLEAR);
    vTaskDelay(pdMS_TO_TICKS(LCD_DELAY_CLEAR_MS));  // Clear needs longer delay
    
    // Entry Mode Set: Cursor moves right, no display shift
    // 0x06 = 0000 0110 = I/D=1 (increment), S=0 (no shift)
    lcd_send_command(LCD_CMD_ENTRY_MODE);
    vTaskDelay(pdMS_TO_TICKS(2));
    
    ESP_LOGI(TAG, "LCD initialization complete");
    ESP_LOGI(TAG, "LCD ready for display operations");
    
    return ESP_OK;
}

/**
 * @brief Clear the LCD display
 * 
 * Clears all characters from the display and returns the cursor to the
 * home position (row 0, column 0). Also resets any display shift.
 * 
 * Execution time: ~1.52ms (we wait 2ms to be safe)
 */
static void lcd_clear(void)
{
    lcd_send_command(LCD_CMD_CLEAR);
    vTaskDelay(pdMS_TO_TICKS(LCD_DELAY_CLEAR_MS));
}

/**
 * @brief Set the cursor position on the LCD
 * 
 * Moves the cursor to the specified row and column. The next character
 * written will appear at this position.
 * 
 * HD44780 DDRAM addressing:
 * - Row 0: addresses 0x00-0x27 (40 characters)
 * - Row 1: addresses 0x40-0x67 (40 characters)
 * - For 16x2 display, only 0x00-0x0F and 0x40-0x4F are visible
 * 
 * @param row Row number (0 to LCD_ROWS-1)
 * @param col Column number (0 to LCD_COLS-1)
 */
static void lcd_set_cursor(uint8_t row, uint8_t col)
{
    // Clamp row and column to valid range
    if (row >= LCD_ROWS) {
        row = LCD_ROWS - 1;
    }
    if (col >= LCD_COLS) {
        col = LCD_COLS - 1;
    }
    
    // Calculate DDRAM address: base address for row + column offset
    uint8_t address = lcd_row_offsets[row] + col;
    
    // Set DDRAM address command: 0x80 | address
    lcd_send_command(LCD_CMD_SET_DDRAM | address);
    
    // Small delay after cursor movement for I2C LCD reliability
    esp_rom_delay_us(500);
}

/**
 * @brief Print a string on the LCD at the current cursor position
 * 
 * Writes each character of the null-terminated string to the LCD.
 * The cursor automatically advances after each character.
 * 
 * Notes:
 * - Characters beyond the visible columns wrap to the next DDRAM address
 *   (which may not be the next visible row on 16x2 displays)
 * - Use lcd_set_cursor() to control where text appears
 * - To display on multiple lines, use lcd_set_cursor() before each line
 * 
 * @param text Null-terminated string to display
 */
static void lcd_print(const char *text)
{
    if (text == NULL) {
        return;
    }
    
    // Send each character until null terminator
    // Add delay between characters for reliable I2C LCD operation
    while (*text) {
        lcd_send_data((uint8_t)*text);
        text++;
        // Small delay between characters for I2C LCD reliability
        // The PCF8574 I2C expander adds latency that requires this
        esp_rom_delay_us(500);  // 500us per character
    }
}

/**
 * @brief Control the LCD backlight
 * 
 * Turns the LCD backlight LED on or off. The backlight is controlled
 * through bit 3 of the PCF8574 output.
 * 
 * @param on true to turn backlight ON, false to turn OFF
 */
static void lcd_backlight(bool on)
{
    if (on) {
        lcd_backlight_state = LCD_BIT_BL;  // Set bit 3
        ESP_LOGI(TAG, "LCD backlight ON");
    } else {
        lcd_backlight_state = 0;  // Clear bit 3
        ESP_LOGI(TAG, "LCD backlight OFF");
    }
    
    // Update the backlight immediately by writing current state
    lcd_i2c_write_byte(lcd_backlight_state);
}

/* ============================================================================
 * REAL-TIME CLOCK IMPLEMENTATION (Phase 6)
 * ============================================================================
 * 
 * This section implements a software-based Real-Time Clock (RTC) using FreeRTOS.
 * Since the ESP32-S3 does not have a battery-backed RTC, we use a software timer
 * to keep track of time. The time is lost on power cycle unless we save/restore
 * it from the SD card.
 * 
 * DESIGN CHOICES:
 * ---------------
 * 1. FreeRTOS Timer vs Task:
 *    - We use a FreeRTOS software timer for the 1-second tick
 *    - Timers are more efficient than a dedicated task with vTaskDelay
 *    - Timer callback is quick (just increments counters)
 * 
 * 2. Separate Display Task:
 *    - A dedicated task handles LCD updates
 *    - This separates timekeeping from display logic
 *    - Allows LCD updates at different rates if needed
 * 
 * 3. Time Persistence:
 *    - Optionally save time to SD card periodically
 *    - Load last saved time on boot (if available)
 *    - Falls back to compile-time default if no saved time
 * 
 * CALENDAR LOGIC:
 * ---------------
 * The Gregorian calendar has these rules for days per month:
 * - January (1):   31 days
 * - February (2):  28 days, or 29 in leap years
 * - March (3):     31 days
 * - April (4):     30 days
 * - May (5):       31 days
 * - June (6):      30 days
 * - July (7):      31 days
 * - August (8):    31 days
 * - September (9): 30 days
 * - October (10):  31 days
 * - November (11): 30 days
 * - December (12): 31 days
 * 
 * Leap year rules:
 * - Divisible by 4: leap year
 * - EXCEPT divisible by 100: not a leap year
 * - EXCEPT divisible by 400: leap year
 * Examples: 2000 (leap), 1900 (not leap), 2024 (leap), 2025 (not leap)
 */

/**
 * @brief Structure to hold date and time components
 * 
 * This structure stores the complete date and time in individual fields.
 * All values use natural numbering (month 1-12, day 1-31, etc.)
 */
typedef struct {
    uint16_t year;    // Full year (e.g., 2026)
    uint8_t  month;   // Month (1-12)
    uint8_t  day;     // Day of month (1-31)
    uint8_t  hour;    // Hour (0-23, 24-hour format)
    uint8_t  minute;  // Minute (0-59)
    uint8_t  second;  // Second (0-59)
} datetime_t;

// Global current date/time - updated by timer every second
static datetime_t current_datetime = {
    .year = 2026,     // Default: compile year
    .month = 2,       // February
    .day = 18,        // 18th
    .hour = 12,       // Noon
    .minute = 0,
    .second = 0
};

// FreeRTOS timer handle for 1-second ticks
static TimerHandle_t rtc_timer = NULL;

// Task handle for LCD clock display
static TaskHandle_t clock_task_handle = NULL;

// Days per month lookup table (index 0 unused, 1-12 for Jan-Dec)
// February (index 2) handled separately for leap years
static const uint8_t days_in_month[13] = {
    0,   // Index 0 unused
    31,  // January
    28,  // February (non-leap year, leap handled in code)
    31,  // March
    30,  // April
    31,  // May
    30,  // June
    31,  // July
    31,  // August
    30,  // September
    31,  // October
    30,  // November
    31   // December
};

/**
 * @brief Check if a year is a leap year
 * 
 * Implements the Gregorian calendar leap year rules:
 * - Divisible by 4: leap year
 * - Unless divisible by 100: not a leap year
 * - Unless divisible by 400: leap year
 * 
 * @param year The year to check
 * @return true if leap year, false otherwise
 */
static bool is_leap_year(uint16_t year)
{
    // Divisible by 400 -> leap year (e.g., 2000)
    if ((year % 400) == 0) {
        return true;
    }
    // Divisible by 100 but not 400 -> not leap year (e.g., 1900)
    if ((year % 100) == 0) {
        return false;
    }
    // Divisible by 4 but not 100 -> leap year (e.g., 2024)
    if ((year % 4) == 0) {
        return true;
    }
    // Not divisible by 4 -> not leap year
    return false;
}

/**
 * @brief Get the number of days in a specific month
 * 
 * Returns the correct number of days for the given month and year,
 * accounting for leap years in February.
 * 
 * @param month Month (1-12)
 * @param year Year (for leap year calculation)
 * @return Number of days in the month
 */
static uint8_t get_days_in_month(uint8_t month, uint16_t year)
{
    // Validate month
    if (month < 1 || month > 12) {
        return 30;  // Fallback
    }
    
    // February special case: check for leap year
    if (month == 2) {
        return is_leap_year(year) ? 29 : 28;
    }
    
    // All other months: use lookup table
    return days_in_month[month];
}

/**
 * @brief Increment the datetime by one second
 * 
 * This function handles all the cascading increments:
 * second -> minute -> hour -> day -> month -> year
 * 
 * It correctly handles:
 * - Different days per month (28/29/30/31)
 * - Leap years (February 29)
 * - Year rollover
 */
static void datetime_increment_second(void)
{
    // Increment seconds
    current_datetime.second++;
    
    // Check for second overflow (60 seconds = 1 minute)
    if (current_datetime.second >= 60) {
        current_datetime.second = 0;
        current_datetime.minute++;
        
        // Check for minute overflow (60 minutes = 1 hour)
        if (current_datetime.minute >= 60) {
            current_datetime.minute = 0;
            current_datetime.hour++;
            
            // Check for hour overflow (24 hours = 1 day)
            if (current_datetime.hour >= 24) {
                current_datetime.hour = 0;
                current_datetime.day++;
                
                // Check for day overflow (depends on month)
                uint8_t max_days = get_days_in_month(current_datetime.month, 
                                                     current_datetime.year);
                if (current_datetime.day > max_days) {
                    current_datetime.day = 1;
                    current_datetime.month++;
                    
                    // Check for month overflow (12 months = 1 year)
                    if (current_datetime.month > 12) {
                        current_datetime.month = 1;
                        current_datetime.year++;
                        ESP_LOGI(TAG, "Happy New Year! %d", current_datetime.year);
                    }
                }
            }
        }
    }
}

/**
 * @brief Set the current date and time
 * 
 * Allows setting the RTC to a specific date and time.
 * Validates all fields before setting.
 * 
 * @param year Full year (e.g., 2026)
 * @param month Month (1-12)
 * @param day Day of month (1-31)
 * @param hour Hour (0-23)
 * @param minute Minute (0-59)
 * @param second Second (0-59)
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if values are invalid
 */
static esp_err_t datetime_set(uint16_t year, uint8_t month, uint8_t day,
                              uint8_t hour, uint8_t minute, uint8_t second)
{
    // Validate basic ranges
    if (month < 1 || month > 12) {
        ESP_LOGE(TAG, "Invalid month: %d (must be 1-12)", month);
        return ESP_ERR_INVALID_ARG;
    }
    if (hour > 23) {
        ESP_LOGE(TAG, "Invalid hour: %d (must be 0-23)", hour);
        return ESP_ERR_INVALID_ARG;
    }
    if (minute > 59) {
        ESP_LOGE(TAG, "Invalid minute: %d (must be 0-59)", minute);
        return ESP_ERR_INVALID_ARG;
    }
    if (second > 59) {
        ESP_LOGE(TAG, "Invalid second: %d (must be 0-59)", second);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate day based on month and year
    uint8_t max_days = get_days_in_month(month, year);
    if (day < 1 || day > max_days) {
        ESP_LOGE(TAG, "Invalid day: %d (must be 1-%d for month %d)", 
                 day, max_days, month);
        return ESP_ERR_INVALID_ARG;
    }
    
    // All validations passed - set the time
    current_datetime.year = year;
    current_datetime.month = month;
    current_datetime.day = day;
    current_datetime.hour = hour;
    current_datetime.minute = minute;
    current_datetime.second = second;
    
    ESP_LOGI(TAG, "Time set to: %04d-%02d-%02d %02d:%02d:%02d",
             year, month, day, hour, minute, second);
    
    return ESP_OK;
}

/**
 * @brief Timer callback - called every second to update the clock
 * 
 * This function is called by the FreeRTOS timer subsystem every 1000ms.
 * It must execute quickly (no blocking operations) because timer callbacks
 * run in the timer service task context.
 * 
 * @param xTimer Handle to the timer that expired (unused)
 */
static void rtc_timer_callback(TimerHandle_t xTimer)
{
    (void)xTimer;  // Unused parameter
    
    // Increment the time by one second
    datetime_increment_second();
}

/**
 * @brief Initialize the software RTC
 * 
 * Creates and starts the FreeRTOS timer that ticks every second.
 * The timer callback increments the time counters.
 * 
 * @return ESP_OK on success, error code on failure
 */
static esp_err_t rtc_init(void)
{
    ESP_LOGI(TAG, "Initializing software RTC...");
    
    // Create a periodic timer with 1 second (1000ms) period
    // Parameters: name, period in ticks, auto-reload, timer ID, callback
    rtc_timer = xTimerCreate(
        "rtc_timer",                      // Timer name (for debugging)
        pdMS_TO_TICKS(1000),              // Period: 1000ms = 1 second
        pdTRUE,                           // Auto-reload: yes (periodic timer)
        NULL,                             // Timer ID (not used)
        rtc_timer_callback                // Callback function
    );
    
    if (rtc_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create RTC timer");
        return ESP_FAIL;
    }
    
    // Start the timer
    // xTimerStart returns pdPASS on success
    if (xTimerStart(rtc_timer, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start RTC timer");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "RTC timer started - incrementing every second");
    ESP_LOGI(TAG, "Initial time: %04d-%02d-%02d %02d:%02d:%02d",
             current_datetime.year, current_datetime.month, current_datetime.day,
             current_datetime.hour, current_datetime.minute, current_datetime.second);
    
    return ESP_OK;
}

/**
 * @brief Save current time to SD card
 * 
 * Saves the current datetime to a file on the SD card.
 * This allows time to persist across power cycles.
 * 
 * File format: Simple text with each value on a line
 * 
 * @return ESP_OK on success, error code on failure
 */
static esp_err_t rtc_save_to_sd(void)
{
    // Check if USB MSC is active - cannot write if PC has the drive
    if (usb_msc_active) {
        ESP_LOGW(TAG, "Cannot save time - USB MSC is active");
        return ESP_ERR_INVALID_STATE;
    }
    
    const char *filepath = SD_MOUNT_POINT "/time.txt";
    FILE *file = fopen(filepath, "w");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open %s for writing", filepath);
        return ESP_FAIL;
    }
    
    // Write datetime values as text
    fprintf(file, "%d\n", current_datetime.year);
    fprintf(file, "%d\n", current_datetime.month);
    fprintf(file, "%d\n", current_datetime.day);
    fprintf(file, "%d\n", current_datetime.hour);
    fprintf(file, "%d\n", current_datetime.minute);
    fprintf(file, "%d\n", current_datetime.second);
    
    fclose(file);
    ESP_LOGI(TAG, "Time saved to SD card: %s", filepath);
    
    return ESP_OK;
}

/**
 * @brief Load time from SD card
 * 
 * Attempts to load the previously saved datetime from the SD card.
 * If the file doesn't exist or is invalid, keeps the default time.
 * 
 * @return ESP_OK if time loaded, ESP_ERR_NOT_FOUND if file missing
 */
static esp_err_t rtc_load_from_sd(void)
{
    const char *filepath = SD_MOUNT_POINT "/time.txt";
    FILE *file = fopen(filepath, "r");
    if (file == NULL) {
        ESP_LOGW(TAG, "No saved time file found (%s)", filepath);
        ESP_LOGI(TAG, "Using default time: %04d-%02d-%02d %02d:%02d:%02d",
                 current_datetime.year, current_datetime.month, current_datetime.day,
                 current_datetime.hour, current_datetime.minute, current_datetime.second);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Read datetime values
    int year, month, day, hour, minute, second;
    if (fscanf(file, "%d\n%d\n%d\n%d\n%d\n%d\n", 
               &year, &month, &day, &hour, &minute, &second) != 6) {
        ESP_LOGE(TAG, "Invalid time file format");
        fclose(file);
        return ESP_ERR_INVALID_ARG;
    }
    fclose(file);
    
    // Validate and set the loaded time
    esp_err_t ret = datetime_set((uint16_t)year, (uint8_t)month, (uint8_t)day,
                                  (uint8_t)hour, (uint8_t)minute, (uint8_t)second);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Time loaded from SD card");
    }
    
    return ret;
}

/**
 * @brief FreeRTOS task for updating the LCD with current time and status
 * 
 * This task runs continuously and updates the LCD display with:
 * - Row 0: Date (MM/DD) and status indicators (SD/USB)
 * - Row 1: Time in format "HH:MM:SS" (24-hour, centered)
 * 
 * The task updates once per second, synchronized with the RTC timer.
 * Status indicators update immediately when state changes.
 * 
 * LCD LAYOUT (16x2) with Status Indicators:
 * +----------------+
 * |02/18 SD:Y USB:N|  <- Row 0: Date + SD status + USB status
 * |    12:00:00    |  <- Row 1: Time (centered)
 * +----------------+
 * 
 * STATUS INDICATOR MEANINGS:
 * - SD:Y = SD card initialized and mounted successfully
 * - SD:N = SD card failed to initialize
 * - USB:Y = USB host (PC) is connected and accessing storage
 * - USB:N = USB not connected or PC not accessing storage
 * 
 * @param pvParameters Task parameters (unused)
 */
static void clock_task(void *pvParameters)
{
    (void)pvParameters;  // Unused
    
    // Buffers for formatted strings
    // Row 0: "MM/DD SD:Y USB:N" = 16 chars for display
    // Row 1: "    HH:MM:SS    " = 16 chars for display
    // Using larger buffers (24) to prevent compiler truncation warnings
    // when format specifiers could theoretically overflow (e.g., %02d > 2 digits)
    char row0_str[24];  // Date and status
    char row1_str[24];  // Time
    
    // Store previous values to detect changes (reduces unnecessary LCD updates)
    uint8_t prev_second = 255;        // Invalid value forces first update
    bool prev_sd_status = false;      // Previous SD card status
    bool prev_usb_status = false;     // Previous USB status
    bool force_update = true;         // Force full update on first iteration
    
    ESP_LOGI(TAG, "Clock display task started");
    ESP_LOGI(TAG, "LCD Layout: Row 0 = Date + Status, Row 1 = Time");
    
    while (1) {
        // Check if any displayed value has changed
        bool time_changed = (current_datetime.second != prev_second);
        bool sd_changed = (sd_card_initialized != prev_sd_status);
        bool usb_changed = (usb_msc_active != prev_usb_status);
        
        // Update display if anything changed or forced
        if (time_changed || sd_changed || usb_changed || force_update) {
            
            /* ================================================================
             * ROW 0: Date and Status Indicators
             * ================================================================
             * Format: "MM/DD SD:Y USB:N"
             *         01234567890123456
             * 
             * Characters:
             * - 0-4:  Date in MM/DD format (5 chars)
             * - 5:    Space separator
             * - 6-9:  "SD:" + status character (4 chars)
             * - 10:   Space separator
             * - 11-15: "USB:" + status character (5 chars)
             */
            snprintf(row0_str, sizeof(row0_str), "%02d/%02d SD:%c USB:%c",
                     current_datetime.month,
                     current_datetime.day,
                     sd_card_initialized ? 'Y' : 'N',
                     usb_msc_active ? 'Y' : 'N');
            row0_str[16] = '\0';  // Ensure 16 char max for LCD
            
            /* ================================================================
             * ROW 1: Time (centered)
             * ================================================================
             * Format: "    HH:MM:SS    "
             *         0123456789012345
             * 
             * 4 spaces + 8 char time + 4 spaces = 16 chars
             */
            snprintf(row1_str, sizeof(row1_str), "    %02d:%02d:%02d    ",
                     current_datetime.hour,
                     current_datetime.minute,
                     current_datetime.second);
            row1_str[16] = '\0';  // Ensure 16 char max for LCD
            
            // Update LCD - use set_cursor to avoid clearing (reduces flicker)
            // Always update both rows to keep display consistent
            lcd_set_cursor(0, 0);
            lcd_print(row0_str);
            
            lcd_set_cursor(1, 0);
            lcd_print(row1_str);
            
            // Update tracking variables
            prev_second = current_datetime.second;
            prev_sd_status = sd_card_initialized;
            prev_usb_status = usb_msc_active;
            force_update = false;
            
            // Log status changes (helpful for debugging)
            if (sd_changed) {
                ESP_LOGI(TAG, "LCD: SD status changed to %s", 
                         sd_card_initialized ? "OK" : "FAIL");
            }
            if (usb_changed) {
                ESP_LOGI(TAG, "LCD: USB status changed to %s",
                         usb_msc_active ? "CONNECTED" : "DISCONNECTED");
            }
        }
        
        // Poll at 100ms for responsive status updates
        // This is a reasonable tradeoff between responsiveness and CPU usage
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * @brief Start the clock display task
 * 
 * Creates and starts the FreeRTOS task that updates the LCD with current time.
 * The task runs at low priority since it's not time-critical.
 * 
 * @return ESP_OK on success, error code on failure
 */
static esp_err_t clock_task_start(void)
{
    ESP_LOGI(TAG, "Starting clock display task...");
    
    // Create the clock task
    // Stack size of 4096 bytes is sufficient for LCD operations
    // Priority 2 is low (just above idle task at priority 1)
    BaseType_t ret = xTaskCreate(
        clock_task,           // Task function
        "clock_task",         // Task name (for debugging)
        4096,                 // Stack size in bytes
        NULL,                 // Task parameters
        2,                    // Priority (low)
        &clock_task_handle    // Task handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create clock task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Clock display task created successfully");
    return ESP_OK;
}

/* ============================================================================
 * MAIN APPLICATION ENTRY POINT (Phase 7 - Full Integration)
 * ============================================================================ */

/**
 * @brief Application entry point - Integrates all components
 * 
 * STARTUP SEQUENCE (ordered for dependencies):
 * ============================================
 * 
 * 1. MUTEX CREATION (Required by all phases for thread safety)
 *    - Create SD card access mutex for USB/local coordination
 * 
 * 2. SD CARD INITIALIZATION (Phase 2 - Foundation)
 *    - Initialize SPI bus for SD card communication
 *    - Mount FAT file system at /sdcard
 *    - Display SD card info (type, capacity)
 *    - WHY FIRST: USB MSC and file operations depend on this
 * 
 * 3. FILE OPERATIONS DEMO (Phase 3 - Verify SD works)
 *    - Create, read, append files
 *    - List directory contents
 *    - WHY EARLY: Confirms SD card is working before USB takes over
 * 
 * 4. I2C AND LCD INITIALIZATION (Phase 5 - Display setup)
 *    - Initialize I2C master on GPIO 8/9
 *    - Initialize HD44780 LCD in 4-bit mode
 *    - Display startup message
 *    - WHY BEFORE USB: Want to show status during USB setup
 * 
 * 5. SOFTWARE RTC INITIALIZATION (Phase 6 - Timekeeping)
 *    - Try to load saved time from SD card
 *    - Start 1-second FreeRTOS timer
 *    - WHY BEFORE CLOCK TASK: Timer must be running for clock display
 * 
 * 6. USB MASS STORAGE INITIALIZATION (Phase 4 - Takes over SD)
 *    - Unmount local FAT filesystem
 *    - Initialize TinyUSB MSC with SD card
 *    - PC can now access SD card as removable drive
 *    - WHY LATE: Takes exclusive SD access, file ops must be done first
 * 
 * 7. CLOCK DISPLAY TASK (Phase 6 - Continuous operation)
 *    - Start FreeRTOS task to update LCD with time
 *    - Runs continuously at low priority
 *    - WHY LAST: All display infrastructure must be ready
 * 
 * ERROR HANDLING STRATEGY:
 * ========================
 * - Each phase checks for errors and logs details
 * - Critical failures (SD, I2C) halt startup with return
 * - Non-critical failures (time load) log warnings and continue
 * - LCD shows error status if initialization fails
 */
void app_main(void)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  SD Card, USB MSC, and LCD Clock Demo  ");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Starting component initialization...");
    
    /* ========================================================================
     * STEP 1: Create synchronization primitives
     * ========================================================================
     * 
     * The SD card access mutex coordinates between USB MSC operations and
     * local file operations. Although USB MSC has exclusive access while
     * connected, we need the mutex for safe transition between modes.
     */
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "[1/7] Creating synchronization primitives...");
    
    sd_access_mutex = xSemaphoreCreateMutex();
    if (sd_access_mutex == NULL) {
        ESP_LOGE(TAG, "FATAL: Failed to create SD access mutex");
        ESP_LOGE(TAG, "System cannot continue without thread safety");
        return;
    }
    ESP_LOGI(TAG, "       Mutex created successfully");
    
    /* ========================================================================
     * STEP 2: Initialize SD card and mount FAT file system (Phase 2)
     * ========================================================================
     * 
     * The SD card is the foundation for file storage. We must initialize it
     * before any file operations or USB MSC setup can proceed.
     */
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "[2/7] Initializing SD card (Phase 2)...");
    
    ret = sd_card_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FATAL: SD card initialization failed!");
        ESP_LOGE(TAG, "Check SD card slot connections (SDMMC mode):");
        ESP_LOGE(TAG, "  CLK  -> GPIO %d", SD_MMC_CLK_GPIO);
        ESP_LOGE(TAG, "  CMD  -> GPIO %d", SD_MMC_CMD_GPIO);
        ESP_LOGE(TAG, "  D0   -> GPIO %d", SD_MMC_D0_GPIO);
        ESP_LOGE(TAG, "Ensure SD card is inserted and formatted as FAT32");
        return;
    }
    
    // Display SD card information
    sd_card_print_info();
    ESP_LOGI(TAG, "       SD card mounted at %s", SD_MOUNT_POINT);
    
    /* ========================================================================
     * STEP 3: Demonstrate file operations (Phase 3)
     * ========================================================================
     * 
     * Run file operation demos to verify SD card is working properly.
     * This must happen while we have local FAT access (before USB MSC).
     */
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "[3/7] Running file operations demo (Phase 3)...");
    
    file_operations_demo();
    ESP_LOGI(TAG, "       File operations completed");
    
    /* ========================================================================
     * STEP 4: Initialize I2C and LCD display (Phase 5)
     * ========================================================================
     * 
     * Set up the LCD before USB MSC so we can display status during the
     * somewhat slow USB enumeration process.
     * 
     * HARDWARE REMINDER: 1k Ohm pull-up resistors required on SDA/SCL!
     */
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "[4/7] Initializing I2C and LCD (Phase 5)...");
    
    ret = i2c_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FATAL: I2C initialization failed!");
        ESP_LOGE(TAG, "Check hardware connections:");
        ESP_LOGE(TAG, "  SDA -> GPIO %d with 1k pull-up to 3.3V", I2C_SDA_GPIO);
        ESP_LOGE(TAG, "  SCL -> GPIO %d with 1k pull-up to 3.3V", I2C_SCL_GPIO);
        ESP_LOGE(TAG, "  VCC -> 5V (or 3.3V if module supports)");
        ESP_LOGE(TAG, "  GND -> GND");
        // Continue without LCD - not fatal, but display won't work
        ESP_LOGW(TAG, "Continuing without LCD display...");
    } else {
        ret = lcd_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "LCD initialization failed!");
            ESP_LOGE(TAG, "Check LCD I2C address (currently 0x%02X)", LCD_I2C_ADDR);
            ESP_LOGW(TAG, "Continuing without LCD display...");
        } else {
            // Show startup message on LCD
            lcd_clear();
            lcd_set_cursor(0, 0);
            lcd_print("  SD/USB/Clock  ");
            lcd_set_cursor(1, 0);
            lcd_print("  Initializing  ");
            ESP_LOGI(TAG, "       LCD initialized and showing startup message");
        }
    }
    
    /* ========================================================================
     * STEP 5: Initialize software RTC (Phase 6)
     * ========================================================================
     * 
     * Start the real-time clock before the display task so time is ready
     * to be shown. Try to restore saved time from SD card.
     */
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "[5/7] Initializing software RTC (Phase 6)...");
    
    // Try to load saved time from SD card (non-fatal if fails)
    ret = rtc_load_from_sd();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "       Using default time (no saved time found)");
    }
    
    // Initialize and start the RTC timer
    ret = rtc_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RTC initialization failed!");
        ESP_LOGW(TAG, "Clock display will not update");
    } else {
        ESP_LOGI(TAG, "       RTC started, ticking every second");
    }
    
    /* ========================================================================
     * STEP 6: Initialize USB Mass Storage (Phase 4)
     * ========================================================================
     * 
     * USB MSC takes exclusive access to the SD card. We must unmount the
     * local FAT filesystem first. After this, file operations from ESP32
     * require special handling (unmount USB, mount local, do operation,
     * unmount local, remount USB).
     */
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "[6/7] Initializing USB Mass Storage (Phase 4)...");
    
    // Update LCD to show USB setup in progress
    lcd_set_cursor(1, 0);
    lcd_print("  USB Setup...  ");
    
    // NOTE: Do NOT unmount the FAT filesystem!
    // esp_vfs_fat_sdcard_unmount() deinitializes the SDMMC driver, which breaks
    // USB MSC sector access. Instead, we keep FAT mounted - USB MSC can coexist
    // with it because USB MSC uses sector-level access (sdmmc_read_sectors)
    // while FAT uses file-level access. The usb_msc_mount_changed_cb callback
    // sets usb_msc_active=true when PC mounts, blocking local file operations.
    ESP_LOGI(TAG, "       Keeping FAT mounted (USB MSC uses sector-level access)...");
    
    // Initialize USB MSC (sd_card pointer provides sector access)
    ret = usb_msc_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "USB MSC initialization failed!");
        lcd_set_cursor(1, 0);
        lcd_print("  USB ERROR!    ");
        // Continue - SD card still works, just no USB access
        ESP_LOGW(TAG, "Continuing without USB Mass Storage...");
    } else {
        ESP_LOGI(TAG, "       USB MSC ready - connect to PC");
    }
    
    /* ========================================================================
     * STEP 7: Start clock display task (Phase 6)
     * ========================================================================
     * 
     * Now that LCD and RTC are initialized, start the task that continuously
     * updates the display with current date and time.
     */
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "[7/7] Starting clock display task (Phase 6)...");
    
    ret = clock_task_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start clock display task");
        ESP_LOGW(TAG, "LCD will not show time updates");
    } else {
        ESP_LOGI(TAG, "       Clock task running");
    }
    
    /* ========================================================================
     * INITIALIZATION COMPLETE
     * ======================================================================== */
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "    INITIALIZATION COMPLETE            ");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "SYSTEM STATUS:");
    ESP_LOGI(TAG, "  SD Card:  Mounted and accessible via USB");
    ESP_LOGI(TAG, "  USB MSC:  Ready - connect to PC");
    ESP_LOGI(TAG, "  LCD:      Displaying clock");
    ESP_LOGI(TAG, "  RTC:      Running (software-based)");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "USAGE:");
    ESP_LOGI(TAG, "  1. Connect USB to PC - appears as removable drive");
    ESP_LOGI(TAG, "  2. Browse, create, modify files from PC");
    ESP_LOGI(TAG, "  3. Safely eject before disconnecting");
    ESP_LOGI(TAG, "  4. LCD shows current date and time");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "NOTE: Time resets on power cycle unless saved to SD");
    ESP_LOGI(TAG, "========================================");
}
