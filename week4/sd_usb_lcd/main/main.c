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
#include "driver/spi_master.h"
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
 * SD CARD SPI INTERFACE GPIO CONFIGURATION
 * ============================================================================
 * 
 * The SD card is connected via SPI interface. ESP32-S3 has flexible GPIO 
 * matrix allowing SPI signals to be routed to most GPIO pins.
 * 
 * Pin selection rationale:
 * - GPIO 10-13 are commonly used for SPI2 on ESP32-S3
 * - These pins do not conflict with:
 *   - GPIO 0: BOOT button (used in Week 3)
 *   - GPIO 4-7: LEDs (used in Week 3)
 *   - GPIO 8-9: Reserved for I2C (used by LCD)
 * 
 * IMPORTANT: Connect the SD card module as follows:
 *   SD Card Module    ESP32-S3 GPIO
 *   -------------     -------------
 *   MOSI (DI)     --> GPIO 11
 *   MISO (DO)     --> GPIO 13
 *   CLK (SCLK)    --> GPIO 12
 *   CS (SS)       --> GPIO 10
 *   VCC           --> 3.3V
 *   GND           --> GND
 */
#define SD_MOSI_GPIO        GPIO_NUM_11  // Master Out Slave In - data to SD card
#define SD_MISO_GPIO        GPIO_NUM_13  // Master In Slave Out - data from SD card
#define SD_CLK_GPIO         GPIO_NUM_12  // SPI clock signal
#define SD_CS_GPIO          GPIO_NUM_10  // Chip Select - active low

// SPI host to use for SD card (SPI2_HOST is available on ESP32-S3)
// SPI1_HOST is typically used for flash memory
#define SD_SPI_HOST         SPI2_HOST

// SPI clock frequencies for SD card
// Start with low frequency for initialization, then increase for data transfer
#define SD_SPI_FREQ_INIT_KHZ    400     // 400kHz for card initialization
#define SD_SPI_FREQ_DEFAULT_KHZ 20000   // 20MHz for normal operation (can go up to 40MHz)

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
 * @brief Initialize SD card via SPI and mount FAT file system
 * 
 * This function performs the complete SD card initialization sequence:
 * 
 * 1. SPI Bus Initialization:
 *    - Configures the SPI2 peripheral with MOSI, MISO, and CLK pins
 *    - Uses DMA channel for efficient data transfer
 *    - Clock starts at 400kHz for card detection, increases after init
 * 
 * 2. SD Card Slot Configuration:
 *    - Configures the chip select (CS) GPIO pin
 *    - Sets up card detect (CD) if available (not used here)
 * 
 * 3. FAT File System Mount:
 *    - Uses esp_vfs_fat_sdspi_mount() to mount the SD card
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
    
    ESP_LOGI(TAG, "Initializing SD card...");
    
    /* ========================================================================
     * STEP 1: Configure and initialize the SPI bus
     * ========================================================================
     * 
     * The SPI bus must be initialized before we can communicate with the SD card.
     * We configure:
     * - MOSI (Master Out Slave In): Data from ESP32 to SD card
     * - MISO (Master In Slave Out): Data from SD card to ESP32
     * - SCLK (Serial Clock): Clock signal generated by ESP32
     * 
     * Note: We don't set max_transfer_sz here because esp_vfs_fat_sdspi_mount
     * will handle the appropriate transfer size for SD card operations.
     */
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_MOSI_GPIO,      // GPIO 11 - data to SD card
        .miso_io_num = SD_MISO_GPIO,      // GPIO 13 - data from SD card
        .sclk_io_num = SD_CLK_GPIO,       // GPIO 12 - clock signal
        .quadwp_io_num = -1,              // Not used for SD card (quad SPI write protect)
        .quadhd_io_num = -1,              // Not used for SD card (quad SPI hold)
        .max_transfer_sz = 4000,          // Maximum transfer size in bytes
    };
    
    // Initialize the SPI bus
    // SPI_DMA_CH_AUTO lets the driver automatically select a DMA channel
    // DMA (Direct Memory Access) allows data transfer without CPU intervention
    ret = spi_bus_initialize(SD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "SPI bus initialized successfully");
    
    /* ========================================================================
     * STEP 2: Configure the SD card slot (device on SPI bus)
     * ========================================================================
     * 
     * This structure tells the SD card driver which GPIO to use for chip select
     * and optionally for card detect. The chip select (CS) line is used to
     * enable/disable communication with the SD card on the shared SPI bus.
     */
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CS_GPIO;     // GPIO 10 - chip select (active low)
    slot_config.host_id = SD_SPI_HOST;    // Use SPI2_HOST
    
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
     * esp_vfs_fat_sdspi_mount() does several things:
     * 1. Initializes the SD card using SPI protocol
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
    
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    // Optionally adjust the host frequency here if needed
    // host.max_freq_khz = SD_SPI_FREQ_DEFAULT_KHZ;
    
    ret = esp_vfs_fat_sdspi_mount(
        SD_MOUNT_POINT,     // Mount point in VFS (e.g., "/sdcard")
        &host,              // SD host configuration
        &slot_config,       // SPI slot configuration (CS pin, etc.)
        &mount_config,      // FAT mount options
        &sd_card            // Output: pointer to card info structure
    );
    
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set format_if_mount_failed = true.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card is inserted and wiring is correct.", 
                     esp_err_to_name(ret));
        }
        // Clean up SPI bus on failure
        spi_bus_free(SD_SPI_HOST);
        return ret;
    }
    
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
    
    // Unmount the FAT filesystem and release SPI device
    // This flushes any pending writes and frees resources
    esp_err_t ret = esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, sd_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount SD card: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Free the SPI bus
    ret = spi_bus_free(SD_SPI_HOST);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to free SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }
    
    sd_card = NULL;
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

// LCD timing delays (from HD44780 datasheet)
#define LCD_DELAY_INIT_MS       50    // Power-on delay before initialization
#define LCD_DELAY_INIT_1_MS     5     // First init command delay (>4.1ms)
#define LCD_DELAY_INIT_2_US     200   // Second init command delay (>100us)
#define LCD_DELAY_CMD_US        50    // Standard command execution time
#define LCD_DELAY_CLEAR_MS      2     // Clear command needs longer (>1.52ms)
#define LCD_DELAY_ENABLE_US     1     // Enable pulse width (>450ns)

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
    // Set Enable high
    lcd_i2c_write_byte(data | LCD_BIT_E);
    esp_rom_delay_us(LCD_DELAY_ENABLE_US);
    
    // Set Enable low (data latched on falling edge)
    lcd_i2c_write_byte(data & ~LCD_BIT_E);
    esp_rom_delay_us(LCD_DELAY_CMD_US);
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
     */
    
    // Step 1: Send 0x30 (Function Set: 8-bit mode) - first attempt
    // This works whether LCD is in 8-bit or 4-bit mode
    lcd_write_nibble(0x30, 0);  // 0011 xxxx = 8-bit mode
    vTaskDelay(pdMS_TO_TICKS(LCD_DELAY_INIT_1_MS));  // Wait >4.1ms
    
    // Step 2: Send 0x30 again - second attempt
    lcd_write_nibble(0x30, 0);
    esp_rom_delay_us(LCD_DELAY_INIT_2_US);  // Wait >100us
    
    // Step 3: Send 0x30 again - third attempt (LCD is now reliably in 8-bit mode)
    lcd_write_nibble(0x30, 0);
    esp_rom_delay_us(LCD_DELAY_INIT_2_US);
    
    // Step 4: Switch to 4-bit mode by sending 0x20
    // After this, all subsequent communication is 4-bit (two nibbles per byte)
    lcd_write_nibble(0x20, 0);  // 0010 xxxx = 4-bit mode
    esp_rom_delay_us(LCD_DELAY_INIT_2_US);
    
    /* ====================================================================
     * LCD CONFIGURATION (now in 4-bit mode)
     * ====================================================================
     */
    
    // Function Set: 4-bit mode, 2 lines, 5x8 font
    // 0x28 = 0010 1000 = DL=0 (4-bit), N=1 (2 lines), F=0 (5x8 font)
    lcd_send_command(LCD_CMD_FUNCTION_4B);
    
    // Display Control: Display ON, cursor OFF, blink OFF
    // 0x0C = 0000 1100 = D=1 (display on), C=0 (cursor off), B=0 (blink off)
    lcd_send_command(LCD_CMD_DISPLAY_ON);
    
    // Clear Display: Clear all characters and return cursor home
    lcd_send_command(LCD_CMD_CLEAR);
    vTaskDelay(pdMS_TO_TICKS(LCD_DELAY_CLEAR_MS));  // Clear needs longer delay
    
    // Entry Mode Set: Cursor moves right, no display shift
    // 0x06 = 0000 0110 = I/D=1 (increment), S=0 (no shift)
    lcd_send_command(LCD_CMD_ENTRY_MODE);
    
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
    while (*text) {
        lcd_send_data((uint8_t)*text);
        text++;
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
 * @brief FreeRTOS task for updating the LCD with current time
 * 
 * This task runs continuously and updates the LCD display with:
 * - Line 1: Date in format "YYYY-MM-DD" (ISO 8601)
 * - Line 2: Time in format "HH:MM:SS" (24-hour)
 * 
 * The task updates once per second, synchronized with the RTC timer.
 * We use a simple delay rather than synchronization primitives for simplicity.
 * 
 * LCD LAYOUT (16x2):
 * +----------------+
 * |  2026-02-18    |  <- Row 0: Date (centered)
 * |    12:00:00    |  <- Row 1: Time (centered)
 * +----------------+
 * 
 * @param pvParameters Task parameters (unused)
 */
static void clock_task(void *pvParameters)
{
    (void)pvParameters;  // Unused
    
    // Buffers for formatted strings
    // Date: "YYYY-MM-DD" = 10 chars + null
    // Time: "HH:MM:SS" = 8 chars + null
    char date_str[17];  // Extra space for padding
    char time_str[17];
    
    // Store previous second to detect changes
    uint8_t prev_second = 255;  // Invalid value forces first update
    
    ESP_LOGI(TAG, "Clock display task started");
    
    while (1) {
        // Only update LCD when second changes (reduces I2C traffic)
        if (current_datetime.second != prev_second) {
            prev_second = current_datetime.second;
            
            // Format date string: "  YYYY-MM-DD  " (centered on 16 chars)
            snprintf(date_str, sizeof(date_str), "  %04d-%02d-%02d  ",
                     current_datetime.year,
                     current_datetime.month,
                     current_datetime.day);
            
            // Format time string: "    HH:MM:SS    " (centered on 16 chars)
            snprintf(time_str, sizeof(time_str), "    %02d:%02d:%02d    ",
                     current_datetime.hour,
                     current_datetime.minute,
                     current_datetime.second);
            
            // Update LCD - use set_cursor to avoid clearing (reduces flicker)
            lcd_set_cursor(0, 0);
            lcd_print(date_str);
            
            lcd_set_cursor(1, 0);
            lcd_print(time_str);
        }
        
        // Poll at 100ms for responsive display
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
 * MAIN APPLICATION ENTRY POINT
 * ============================================================================ */

/**
 * @brief Application entry point
 * 
 * Initializes all components and starts tasks:
 * 1. Initialize SD card (SPI, mount FAT)
 * 2. Perform file operation demos
 * 3. Initialize I2C and LCD
 * 4. Initialize USB MSC
 * 5. Start clock display task
 * 6. Start USB task
 */
void app_main(void)
{
    ESP_LOGI(TAG, "=== SD Card, USB MSC, and LCD Clock Demo ===");
    ESP_LOGI(TAG, "Initializing components...");
    
    // Create mutex for SD card access coordination (USB vs local)
    sd_access_mutex = xSemaphoreCreateMutex();
    if (sd_access_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create SD access mutex");
        return;
    }
    
    // Phase 2: Initialize SD card and mount FAT file system
    esp_err_t ret = sd_card_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD card initialization failed!");
        ESP_LOGE(TAG, "Check wiring: MOSI=GPIO%d, MISO=GPIO%d, CLK=GPIO%d, CS=GPIO%d",
                 SD_MOSI_GPIO, SD_MISO_GPIO, SD_CLK_GPIO, SD_CS_GPIO);
        return;
    }
    
    ESP_LOGI(TAG, "Phase 2 complete: SD card initialized and mounted");
    
    // Phase 3: Demonstrate file operations
    file_operations_demo();
    
    ESP_LOGI(TAG, "Phase 3 complete: File operations demonstrated");
    
    // Phase 4: Initialize USB Mass Storage
    // First unmount the FAT filesystem we mounted in Phase 2
    // because the tinyusb_msc component will manage it
    ESP_LOGI(TAG, "Preparing for USB MSC - unmounting local FAT...");
    ret = esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, sd_card);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to unmount FAT: %s (may already be unmounted)", esp_err_to_name(ret));
    }
    
    // Don't free SPI bus - we still need it for USB MSC
    // The sd_card pointer remains valid
    
    ret = usb_msc_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "USB MSC initialization failed!");
        return;
    }
    
    ESP_LOGI(TAG, "Phase 4 complete: USB MSC initialized");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "USB MASS STORAGE READY");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Connect ESP32-S3 USB port to PC");
    ESP_LOGI(TAG, "A removable drive should appear");
    ESP_LOGI(TAG, "You can browse, create, and modify files");
    ESP_LOGI(TAG, "Safely eject before disconnecting");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Next: Implement Phase 5 (LCD) and beyond...");
}
