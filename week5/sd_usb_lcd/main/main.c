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

/* Week 5 Wi-Fi and networking headers */
#include "esp_wifi.h"           // Wi-Fi driver: station mode, scan, connect
#include "esp_event.h"          // Event loop: Wi-Fi and IP event dispatching
#include "esp_netif.h"          // Network interface: TCP/IP stack integration
#include "nvs_flash.h"          // NVS flash: required for Wi-Fi credential storage
#include "freertos/event_groups.h"  // FreeRTOS event groups: signal Wi-Fi connected

/* Week 5 NTP time synchronization headers */
#include "esp_sntp.h"           // SNTP client: sync system clock from NTP server
#include <time.h>               // POSIX time functions: time(), localtime(), strftime()
#include <sys/time.h>           // gettimeofday(), settimeofday()

/* Week 5 Telegram Bot and HTTP client headers */
#include "esp_http_client.h"    // HTTP client: make HTTPS requests to Telegram API
#include "esp_tls.h"            // TLS/SSL: secure HTTPS connections to Telegram servers
#include "esp_crt_bundle.h"     // Certificate bundle: verify Telegram's TLS certificate
#include "cJSON.h"              // JSON parser: parse Telegram API responses

/* Week 5 HTTP web server header */
#include "esp_http_server.h"    // HTTP server: serve web pages and photos on port 80
#include "esp_timer.h"          // High-resolution timer: esp_timer_get_time() for uptime

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
 * WI-FI CONFIGURATION (Week 5 - Phase 1)
 * ============================================================================
 * 
 * Wi-Fi station mode configuration for connecting to a hidden access point.
 * 
 * HIDDEN SSID:
 * The target network uses a hidden (non-broadcast) SSID. This means the
 * ESP32 cannot discover it through passive scanning. We must:
 * 1. Explicitly set the SSID in the Wi-Fi config
 * 2. Use WIFI_ALL_CHANNEL_SCAN so the ESP32 sends probe requests on
 *    every channel looking for the hidden AP
 * 
 * SECURITY:
 * The network uses WPA/WPA2 authentication. The credentials are:
 * - SSID: "Embedded" (hidden, not broadcast by the AP)
 * - Password: "class2026-embedded"
 */
#define WIFI_SSID           "Embedded"           // Hidden SSID of the target AP
#define WIFI_PASS           "class2026-embedded"  // WPA/WPA2 password
#define WIFI_MAX_RETRY      10                    // Max reconnection attempts before giving up

// Event group bits for signaling Wi-Fi connection state
// These bits are set/cleared by the Wi-Fi event handler and waited on
// by initialization code that depends on network connectivity
#define WIFI_CONNECTED_BIT  BIT0  // Set when Wi-Fi is connected and IP obtained
#define WIFI_FAIL_BIT       BIT1  // Set when all retry attempts exhausted

/* ============================================================================
 * NTP TIME SYNCHRONIZATION CONFIGURATION (Week 5 - Phase 2)
 * ============================================================================
 * 
 * SNTP (Simple Network Time Protocol) configuration for automatic clock sync.
 * The ESP32 system clock is synchronized from the NTP server, replacing the
 * manual software RTC from Week 4.
 * 
 * NTP SERVER:
 * time.nist.gov is a public NTP server pool operated by the National Institute
 * of Standards and Technology. It provides stratum-1 time accuracy.
 * 
 * TIME ZONE STRING FORMAT (POSIX TZ):
 * "PST8PDT,M3.2.0,M11.1.0" breaks down as:
 *   PST     = Standard time abbreviation (Pacific Standard Time)
 *   8       = Offset from UTC in hours (UTC-8 during standard time)
 *   PDT     = Daylight saving time abbreviation (Pacific Daylight Time)
 *   M3.2.0  = DST starts: Month 3 (March), Week 2 (2nd), Day 0 (Sunday)
 *             i.e., 2nd Sunday of March at 02:00 local time
 *   M11.1.0 = DST ends: Month 11 (November), Week 1 (1st), Day 0 (Sunday)
 *             i.e., 1st Sunday of November at 02:00 local time
 * 
 * During PST: UTC-8 (e.g., UTC 20:00 = PST 12:00)
 * During PDT: UTC-7 (e.g., UTC 20:00 = PDT 13:00)
 */
#define NTP_SERVER          "time.nist.gov"                  // NIST NTP server pool
#define NTP_TZ_STRING       "PST8PDT,M3.2.0,M11.1.0"       // Los Angeles time zone with DST
#define NTP_SYNC_INTERVAL   (15 * 60 * 1000)                // Re-sync every 15 minutes (ms)

/* ============================================================================
 * TELEGRAM BOT CONFIGURATION (Week 5 - Phase 3)
 * ============================================================================
 * 
 * Telegram Bot API configuration for receiving messages and photos.
 * 
 * HOW THE TELEGRAM BOT API WORKS:
 * --------------------------------
 * 1. You create a bot via @BotFather on Telegram, which gives you an API token
 * 2. The bot has a unique token that authenticates API requests
 * 3. Users send messages/photos to the bot via the Telegram app
 * 4. The ESP32 polls the Telegram servers via HTTPS for new messages
 * 5. Messages/photos are downloaded and displayed on the web server
 * 
 * API ENDPOINTS USED:
 * - getUpdates: Long-poll for new messages (text and photos)
 * - getFile: Get the download path for a photo file
 * - file download: Download the actual photo binary data
 * 
 * SECURITY:
 * - All communication uses HTTPS (TLS encrypted)
 * - The API token authenticates the ESP32 as the bot owner
 * - esp_crt_bundle provides the root CA certificates for TLS verification
 * 
 * TOKEN FORMAT:
 * The token looks like "123456789:ABCdefGHIjklMNOpqr" where the first part
 * is the bot's numeric ID and the second part is the authentication hash.
 */
#define TELEGRAM_BOT_TOKEN  "8745173474:AAGRhtDMuCoIJmNWbcv768wsyXLurzzV1Ak"
#define TELEGRAM_API_URL    "https://api.telegram.org/bot" TELEGRAM_BOT_TOKEN
#define TELEGRAM_FILE_URL   "https://api.telegram.org/file/bot" TELEGRAM_BOT_TOKEN

// Polling configuration
#define TELEGRAM_POLL_TIMEOUT    10    // Long polling timeout in seconds
#define TELEGRAM_RETRY_DELAY_MS  5000  // Delay before retrying after an error
#define TELEGRAM_MAX_MSG_LEN     256   // Max length for stored text messages
#define TELEGRAM_PHOTO_PATH      SD_MOUNT_POINT "/telegram_photo.jpg"  // Photo save path

// HTTP response buffer size for Telegram API JSON responses
// getUpdates can return large JSON payloads with multiple messages
#define TELEGRAM_HTTP_BUF_SIZE   4096

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

/* Wi-Fi state variables (Week 5) */

// Event group handle for Wi-Fi connection signaling
// Used to block initialization until Wi-Fi is connected and IP is obtained
static EventGroupHandle_t wifi_event_group = NULL;

// Retry counter for Wi-Fi reconnection attempts
// Incremented on each WIFI_EVENT_STA_DISCONNECTED, reset on successful connection
static int wifi_retry_count = 0;

// Flag to track Wi-Fi connection status (for LCD display)
static volatile bool wifi_connected = false;

// Store the assigned IP address as a string for display on LCD and logging
static char wifi_ip_str[16] = "0.0.0.0";

/* NTP time sync state variables (Week 5 - Phase 2) */

// Flag to track whether the system clock has been synchronized via NTP
// When false, time() returns epoch (1970) and the clock display shows "--:--:--"
// When true, time()/localtime() return accurate Los Angeles local time
static volatile bool ntp_synced = false;

/* Telegram Bot state variables (Week 5 - Phase 3) */

// Offset for Telegram getUpdates polling
// The offset tells the API to only return updates with update_id > offset
// This prevents receiving the same message twice
// Starts at 0 (get all pending updates), then set to last_update_id + 1
static int64_t telegram_update_offset = 0;

// Latest received text message from Telegram
// Protected by telegram_mutex when reading/writing
static char telegram_last_message[TELEGRAM_MAX_MSG_LEN] = "";

// Path to the latest photo received from Telegram and saved to SD card
// Empty string means no photo has been received yet
static char telegram_photo_path[128] = "";

// Flag set when new Telegram content (message or photo) is received
// The web server checks this to know when to update the page
static volatile bool telegram_new_content = false;

// Mutex to protect shared Telegram state variables
// Must be taken before reading or writing telegram_last_message,
// telegram_photo_path, or telegram_new_content
static SemaphoreHandle_t telegram_mutex = NULL;

// Task handle for the Telegram polling task
static TaskHandle_t telegram_task_handle = NULL;

/* HTTP web server state variables (Week 5 - Phase 4) */

// Handle to the running HTTP server instance
// NULL when the server is not running. Used to register URI handlers
// and to stop the server if needed.
static httpd_handle_t http_server = NULL;

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

// Wi-Fi functions (Week 5)
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);
static esp_err_t wifi_init_sta(void);

// NTP time sync functions (Week 5 - Phase 2)
static void ntp_time_sync_notification_cb(struct timeval *tv);
static esp_err_t sntp_init_time(void);

// Telegram Bot functions (Week 5 - Phase 3)
static esp_err_t telegram_get_updates(void);
static esp_err_t telegram_download_photo(const char *file_id);
static void telegram_poll_task(void *pvParameters);
static esp_err_t telegram_poll_task_start(void);

// HTTP web server functions (Week 5 - Phase 4)
static esp_err_t http_root_handler(httpd_req_t *req);
static esp_err_t http_photo_handler(httpd_req_t *req);
static esp_err_t http_status_handler(httpd_req_t *req);
static esp_err_t http_server_start(void);

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
 * WI-FI STATION MODE IMPLEMENTATION (Week 5 - Phase 1)
 * ============================================================================
 * 
 * This section implements Wi-Fi connectivity in station (STA) mode.
 * The ESP32-S3 connects to an existing Wi-Fi access point as a client.
 * 
 * CONNECTION FLOW:
 * ----------------
 * 1. Initialize NVS flash (Wi-Fi driver stores calibration data in NVS)
 * 2. Initialize the TCP/IP network interface (esp_netif)
 * 3. Create default event loop for Wi-Fi and IP events
 * 4. Create default Wi-Fi STA network interface
 * 5. Configure Wi-Fi with SSID, password, and scan method
 * 6. Register event handlers for connection state changes
 * 7. Start Wi-Fi and initiate connection
 * 8. Wait for WIFI_CONNECTED_BIT in event group (blocks until connected)
 * 
 * HIDDEN SSID HANDLING:
 * ---------------------
 * Since the AP hides its SSID, passive scanning will not find it.
 * We set scan_method = WIFI_ALL_CHANNEL_SCAN which causes the ESP32 to
 * send probe requests on all channels. The hidden AP responds to probe
 * requests that contain its SSID, allowing the ESP32 to connect.
 * 
 * RECONNECTION STRATEGY:
 * ----------------------
 * On disconnection, the event handler automatically calls esp_wifi_connect()
 * up to WIFI_MAX_RETRY times. If all retries fail, it sets WIFI_FAIL_BIT
 * in the event group. The main task can check this to take action.
 * ============================================================================ */

/**
 * @brief Wi-Fi and IP event handler
 * 
 * This callback function is registered with the ESP event loop and handles
 * all Wi-Fi and IP-related events. It implements the Wi-Fi state machine:
 * 
 * Events handled:
 * - WIFI_EVENT_STA_START: Wi-Fi station started, initiate first connection
 * - WIFI_EVENT_STA_DISCONNECTED: Lost connection, attempt reconnect
 * - IP_EVENT_STA_GOT_IP: Successfully connected and got IP address
 * 
 * @param arg          User argument (unused)
 * @param event_base   Event base (WIFI_EVENT or IP_EVENT)
 * @param event_id     Specific event ID within the base
 * @param event_data   Event-specific data (e.g., IP address info)
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    /* ====================================================================
     * WIFI_EVENT_STA_START
     * ====================================================================
     * Fired once when esp_wifi_start() completes in STA mode.
     * This is where we initiate the first connection attempt.
     */
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wi-Fi STA started, connecting to SSID: %s (hidden)...", WIFI_SSID);
        esp_wifi_connect();
    }
    /* ====================================================================
     * WIFI_EVENT_STA_DISCONNECTED
     * ====================================================================
     * Fired when the ESP32 loses its connection to the AP. This can happen
     * due to: AP going down, signal loss, authentication failure, etc.
     * 
     * We attempt automatic reconnection up to WIFI_MAX_RETRY times.
     * After exhausting retries, we set WIFI_FAIL_BIT so the main task
     * knows the connection failed permanently (until next reboot).
     */
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        
        if (wifi_retry_count < WIFI_MAX_RETRY) {
            wifi_retry_count++;
            ESP_LOGW(TAG, "Wi-Fi disconnected, reconnecting... (attempt %d/%d)",
                     wifi_retry_count, WIFI_MAX_RETRY);
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "Wi-Fi connection failed after %d attempts", WIFI_MAX_RETRY);
            // Signal failure to any task waiting on the event group
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
    }
    /* ====================================================================
     * IP_EVENT_STA_GOT_IP
     * ====================================================================
     * Fired when the DHCP client obtains an IP address from the AP.
     * This means we are fully connected and can use the network.
     * 
     * We store the IP address string for LCD display, reset the retry
     * counter, and signal WIFI_CONNECTED_BIT so initialization can proceed.
     */
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        
        // Convert IP address to string for display on LCD and serial log
        snprintf(wifi_ip_str, sizeof(wifi_ip_str), IPSTR, IP2STR(&event->ip_info.ip));
        
        ESP_LOGI(TAG, "Wi-Fi connected! IP address: %s", wifi_ip_str);
        ESP_LOGI(TAG, "  Gateway: " IPSTR, IP2STR(&event->ip_info.gw));
        ESP_LOGI(TAG, "  Netmask: " IPSTR, IP2STR(&event->ip_info.netmask));
        
        // Reset retry counter on successful connection
        wifi_retry_count = 0;
        wifi_connected = true;
        
        // Signal that Wi-Fi is connected - unblocks any task waiting on this
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/**
 * @brief Initialize Wi-Fi in station mode and connect to the hidden AP
 * 
 * This function performs the complete Wi-Fi initialization sequence:
 * 
 * 1. NVS Flash Init:
 *    - Wi-Fi driver stores PHY calibration data and credentials in NVS
 *    - If NVS partition is corrupted, erases and reinitializes
 * 
 * 2. Network Interface Init:
 *    - Creates the TCP/IP adapter and binds it to the Wi-Fi driver
 *    - esp_netif_create_default_wifi_sta() sets up DHCP client automatically
 * 
 * 3. Event Loop and Handlers:
 *    - Creates the default system event loop
 *    - Registers our wifi_event_handler for WIFI_EVENT and IP_EVENT
 * 
 * 4. Wi-Fi Configuration:
 *    - Sets STA mode with SSID and password
 *    - scan_method = WIFI_ALL_CHANNEL_SCAN for hidden SSID discovery
 *    - threshold.authmode = WIFI_AUTH_WPA2_PSK (minimum security level)
 * 
 * 5. Start and Connect:
 *    - Starts the Wi-Fi driver (triggers WIFI_EVENT_STA_START)
 *    - Event handler calls esp_wifi_connect() on STA_START
 *    - Blocks until WIFI_CONNECTED_BIT or WIFI_FAIL_BIT is set
 * 
 * @return ESP_OK on successful connection, ESP_FAIL if connection failed
 */
static esp_err_t wifi_init_sta(void)
{
    ESP_LOGI(TAG, "Initializing Wi-Fi in station mode...");
    
    /* ====================================================================
     * NOTE: NVS flash is initialized in app_main() before wifi_init_sta()
     * is called. Wi-Fi requires NVS for PHY calibration data storage.
     * By initializing NVS early in app_main, we ensure it's available
     * for any component that needs it, not just Wi-Fi.
     * ====================================================================
     */
    
    /* ====================================================================
     * STEP 1: Create event group for connection signaling
     * ====================================================================
     * The event group allows wifi_init_sta() to block until the Wi-Fi
     * connection is established (or fails permanently).
     */
    wifi_event_group = xEventGroupCreate();
    if (wifi_event_group == NULL) {
        ESP_LOGE(TAG, "  Failed to create Wi-Fi event group");
        return ESP_FAIL;
    }
    
    /* ====================================================================
     * STEP 2: Initialize network interface and event loop
     * ====================================================================
     * esp_netif_init() initializes the underlying TCP/IP stack (LWIP).
     * esp_event_loop_create_default() creates the system event loop that
     * dispatches Wi-Fi and IP events to our handler.
     * esp_netif_create_default_wifi_sta() creates a network interface
     * bound to Wi-Fi STA mode with a DHCP client.
     */
    ESP_LOGI(TAG, "  Initializing network interface...");
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    
    /* ====================================================================
     * STEP 3: Initialize Wi-Fi driver with default configuration
     * ====================================================================
     * WIFI_INIT_CONFIG_DEFAULT() provides sensible defaults for the
     * Wi-Fi driver including buffer sizes, task priorities, etc.
     */
    ESP_LOGI(TAG, "  Initializing Wi-Fi driver...");
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    /* ====================================================================
     * STEP 4: Register event handlers
     * ====================================================================
     * We register our wifi_event_handler for both WIFI_EVENT and IP_EVENT
     * bases. ESP_EVENT_ANY_ID means we get all events in each base.
     * The handler then checks specific event IDs.
     */
    ESP_LOGI(TAG, "  Registering event handlers...");
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,             // Event base: Wi-Fi events
        ESP_EVENT_ANY_ID,       // Any Wi-Fi event (STA_START, DISCONNECTED, etc.)
        &wifi_event_handler,    // Our handler function
        NULL,                   // No user argument
        &instance_any_id        // Handler instance (for unregistering later)
    ));
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,               // Event base: IP events
        IP_EVENT_STA_GOT_IP,    // Only the "got IP" event
        &wifi_event_handler,    // Same handler function
        NULL,                   // No user argument
        &instance_got_ip        // Handler instance
    ));
    
    /* ====================================================================
     * STEP 5: Configure Wi-Fi station parameters
     * ====================================================================
     * Key settings for hidden SSID:
     * - .sta.ssid: Must be set explicitly (AP won't broadcast it)
     * - .sta.scan_method = WIFI_ALL_CHANNEL_SCAN: Sends probe requests
     *   on every channel. This is REQUIRED for hidden networks because
     *   the AP does not include its SSID in beacon frames.
     * - .sta.threshold.authmode: Minimum security level to accept.
     *   Set to WPA2_PSK for security.
     */
    ESP_LOGI(TAG, "  Configuring Wi-Fi for hidden SSID: %s", WIFI_SSID);
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            // WIFI_ALL_CHANNEL_SCAN: Required for hidden SSIDs.
            // The ESP32 sends directed probe requests containing the SSID
            // on each channel. The hidden AP responds to matching probes.
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            // Minimum authentication mode - reject anything weaker than WPA2
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            // SAE (Simultaneous Authentication of Equals) settings for WPA3
            // compatibility. H2E (Hash-to-Element) is the modern approach.
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };
    
    // Set Wi-Fi mode to station (client) and apply configuration
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    /* ====================================================================
     * STEP 6: Start Wi-Fi and wait for connection
     * ====================================================================
     * esp_wifi_start() triggers WIFI_EVENT_STA_START, which causes our
     * event handler to call esp_wifi_connect().
     * 
     * We then block on the event group until either:
     * - WIFI_CONNECTED_BIT: Connected and got IP (success)
     * - WIFI_FAIL_BIT: All retry attempts exhausted (failure)
     */
    ESP_LOGI(TAG, "  Starting Wi-Fi...");
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "  Waiting for Wi-Fi connection...");
    
    // Block here until connected or failed
    // portMAX_DELAY means wait forever (no timeout)
    EventBits_t bits = xEventGroupWaitBits(
        wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,  // Wait for either bit
        pdFALSE,                              // Don't clear bits on exit
        pdFALSE,                              // Wait for ANY bit (not all)
        portMAX_DELAY                         // Wait forever
    );
    
    /* ====================================================================
     * STEP 7: Check connection result
     * ==================================================================== */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Wi-Fi connected successfully to SSID: %s", WIFI_SSID);
        ESP_LOGI(TAG, "  IP Address: %s", wifi_ip_str);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Wi-Fi connection FAILED after %d retries", WIFI_MAX_RETRY);
        ESP_LOGE(TAG, "  Check: 1) SSID correct? 2) Password correct? 3) AP in range?");
        return ESP_FAIL;
    } else {
        // Should not reach here with portMAX_DELAY
        ESP_LOGE(TAG, "Wi-Fi connection unexpected event");
        return ESP_FAIL;
    }
}

/* ============================================================================
 * NTP TIME SYNCHRONIZATION IMPLEMENTATION (Week 5 - Phase 2)
 * ============================================================================
 * 
 * This section implements automatic time synchronization using SNTP.
 * 
 * HOW IT WORKS:
 * -------------
 * 1. After Wi-Fi connects, we initialize the SNTP client
 * 2. SNTP sends a time request to time.nist.gov
 * 3. The server responds with the current UTC time
 * 4. The ESP-IDF SNTP component sets the system clock via settimeofday()
 * 5. We set the POSIX TZ environment variable so localtime() returns
 *    the correct Los Angeles local time (PST/PDT with DST handling)
 * 6. The clock_task uses time()/localtime()/strftime() to display time
 * 
 * WHY NTP REPLACES THE SOFTWARE RTC:
 * -----------------------------------
 * The Week 4 software RTC had several limitations:
 * - Drifted over time (FreeRTOS timer is not crystal-accurate)
 * - Reset to defaults on every power cycle
 * - Required manual time setting or SD card persistence
 * 
 * With NTP:
 * - Time is accurate to within milliseconds of UTC
 * - Automatically set on every boot (no manual intervention)
 * - DST transitions handled by the POSIX time zone rules
 * - Periodic re-sync corrects any drift
 * 
 * SNTP POLL MODE:
 * ---------------
 * We use SNTP_OPMODE_POLL which periodically re-syncs the clock.
 * The default interval is CONFIG_LWIP_SNTP_UPDATE_DELAY (set via menuconfig
 * or we override it). Re-syncing every 15 minutes keeps the clock accurate
 * without excessive network traffic.
 * ============================================================================ */

/**
 * @brief Callback function called when SNTP synchronization completes
 * 
 * This function is registered with esp_sntp_set_time_sync_notification_cb()
 * and is called every time the SNTP client successfully synchronizes the
 * system clock with the NTP server.
 * 
 * On first sync: Sets ntp_synced flag so the clock display switches from
 * "--:--:--" to the actual time.
 * 
 * On subsequent syncs: Logs the sync event for debugging (verifies periodic
 * re-sync is working).
 * 
 * @param tv Pointer to timeval struct with the synchronized time
 */
static void ntp_time_sync_notification_cb(struct timeval *tv)
{
    (void)tv;  // We don't use the raw timeval directly
    
    // Get the synchronized time as a human-readable string for logging
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S %Z", &timeinfo);
    
    if (!ntp_synced) {
        // First successful sync - enable the clock display
        ESP_LOGI(TAG, "NTP: First time sync successful!");
        ESP_LOGI(TAG, "NTP: Current time: %s", time_str);
        ntp_synced = true;
    } else {
        // Periodic re-sync - just log for debugging
        ESP_LOGI(TAG, "NTP: Re-sync completed. Time: %s", time_str);
    }
}

/**
 * @brief Initialize SNTP and synchronize the system clock
 * 
 * This function configures and starts the SNTP client to synchronize
 * the ESP32 system clock from time.nist.gov. It also sets the time zone
 * to America/Los_Angeles (Pacific Time with DST).
 * 
 * INITIALIZATION STEPS:
 * 1. Set the POSIX TZ environment variable for Los Angeles time zone
 * 2. Configure SNTP operating mode (poll mode for periodic re-sync)
 * 3. Set the NTP server address (time.nist.gov)
 * 4. Register the sync notification callback
 * 5. Initialize SNTP (starts background sync)
 * 6. Wait for the first successful sync (blocks until time is set)
 * 
 * IMPORTANT: This function must be called AFTER Wi-Fi is connected,
 * because SNTP needs network access to reach the NTP server.
 * 
 * @return ESP_OK on successful time sync, ESP_FAIL on timeout
 */
static esp_err_t sntp_init_time(void)
{
    ESP_LOGI(TAG, "Initializing NTP time synchronization...");
    ESP_LOGI(TAG, "  NTP Server: %s", NTP_SERVER);
    ESP_LOGI(TAG, "  Time Zone:  %s", NTP_TZ_STRING);
    
    /* ====================================================================
     * STEP 1: Set the POSIX time zone for Los Angeles
     * ====================================================================
     * setenv("TZ", ...) sets the time zone environment variable used by
     * localtime(), mktime(), strftime(), etc. to convert between UTC and
     * local time.
     * 
     * tzset() applies the TZ variable. Must be called after setenv("TZ").
     * 
     * The TZ string "PST8PDT,M3.2.0,M11.1.0" means:
     * - Standard time: PST (Pacific Standard Time), UTC-8
     * - Daylight time: PDT (Pacific Daylight Time), UTC-7 (implied: 1h ahead)
     * - DST starts: 2nd Sunday of March at 02:00 local
     * - DST ends: 1st Sunday of November at 02:00 local
     */
    setenv("TZ", NTP_TZ_STRING, 1);  // 1 = overwrite if exists
    tzset();
    ESP_LOGI(TAG, "  Time zone set: PST (UTC-8) / PDT (UTC-7) with DST rules");
    
    /* ====================================================================
     * STEP 2: Configure SNTP client
     * ====================================================================
     * SNTP_OPMODE_POLL: The SNTP client will periodically query the NTP
     * server. The interval is set by CONFIG_LWIP_SNTP_UPDATE_DELAY
     * (default: 3600000ms = 1 hour). We log the interval for reference.
     * 
     * esp_sntp_setservername(0, ...): Sets the primary NTP server.
     * Index 0 is the first (primary) server. Up to CONFIG_LWIP_SNTP_MAX_SERVERS
     * can be configured for redundancy.
     */
    ESP_LOGI(TAG, "  Configuring SNTP in poll mode...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, NTP_SERVER);
    
    /* ====================================================================
     * STEP 3: Register notification callback and initialize
     * ====================================================================
     * The callback is called every time SNTP successfully syncs the clock.
     * This lets us set the ntp_synced flag on first sync and log re-syncs.
     */
    esp_sntp_set_time_sync_notification_cb(ntp_time_sync_notification_cb);
    
    ESP_LOGI(TAG, "  Starting SNTP client...");
    esp_sntp_init();
    
    /* ====================================================================
     * STEP 4: Wait for first successful time sync
     * ====================================================================
     * After esp_sntp_init(), the SNTP client runs in the background.
     * We poll the sync status until time is set, with a timeout.
     * 
     * esp_sntp_get_sync_status() returns:
     * - SNTP_SYNC_STATUS_RESET: Not yet synced
     * - SNTP_SYNC_STATUS_COMPLETED: Sync completed successfully
     * - SNTP_SYNC_STATUS_IN_PROGRESS: Smooth sync in progress
     * 
     * We wait up to 30 seconds for the first sync. This is generous
     * because NTP typically responds within 1-2 seconds on a good connection.
     */
    ESP_LOGI(TAG, "  Waiting for NTP time sync...");
    int retry = 0;
    const int max_retry = 30;  // 30 seconds timeout
    
    while (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && retry < max_retry) {
        ESP_LOGI(TAG, "  Waiting for NTP response... (%d/%d)", retry + 1, max_retry);
        vTaskDelay(pdMS_TO_TICKS(1000));
        retry++;
    }
    
    if (esp_sntp_get_sync_status() != SNTP_SYNC_STATUS_RESET) {
        // Time synced successfully - log the current time
        time_t now = time(NULL);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S %Z", &timeinfo);
        
        ESP_LOGI(TAG, "NTP time synchronized successfully!");
        ESP_LOGI(TAG, "  Local time: %s", time_str);
        
        // ntp_synced flag is set by the callback, but set it here too
        // in case the callback fires before we check
        ntp_synced = true;
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "NTP time sync timed out after %d seconds", max_retry);
        ESP_LOGE(TAG, "  Check: 1) Wi-Fi has internet access? 2) DNS resolves?");
        ESP_LOGW(TAG, "  Clock will show '--:--:--' until sync succeeds");
        // SNTP will continue retrying in the background
        return ESP_FAIL;
    }
}

/* ============================================================================
 * TELEGRAM BOT API IMPLEMENTATION (Week 5 - Phase 3)
 * ============================================================================
 * 
 * This section implements communication with the Telegram Bot API to receive
 * text messages and photos from Telegram users.
 * 
 * ARCHITECTURE OVERVIEW:
 * ----------------------
 * The Telegram Bot API uses a simple HTTP-based protocol:
 * 
 * 1. POLLING (getUpdates):
 *    The ESP32 makes periodic HTTPS GET requests to the Telegram API asking
 *    "are there any new messages?" This is called "long polling" because the
 *    request can block for up to TELEGRAM_POLL_TIMEOUT seconds waiting for
 *    new messages before returning an empty result.
 * 
 * 2. MESSAGE HANDLING:
 *    When a message is received, it can be either:
 *    - A text message (message.text field)
 *    - A photo (message.photo array with different sizes)
 *    The code extracts text messages directly and for photos, initiates
 *    a two-step download process.
 * 
 * 3. PHOTO DOWNLOAD (two steps):
 *    Step 1: Call getFile API with the file_id to get the file_path
 *    Step 2: Download the actual file from the file download URL
 *    This two-step process is required because Telegram doesn't include
 *    the actual file data in the getUpdates response.
 * 
 * 4. UPDATE TRACKING:
 *    Each message has a unique update_id. We track the highest update_id
 *    seen and pass offset = highest_id + 1 to getUpdates, so we never
 *    process the same message twice.
 * 
 * API URLS:
 * ---------
 * - Poll:     https://api.telegram.org/bot<TOKEN>/getUpdates?offset=N&timeout=10
 * - Get file: https://api.telegram.org/bot<TOKEN>/getFile?file_id=<ID>
 * - Download: https://api.telegram.org/file/bot<TOKEN>/<FILE_PATH>
 * 
 * ALL REQUESTS USE HTTPS (TLS):
 * The esp_crt_bundle component provides the root CA certificates needed
 * to verify Telegram's TLS certificate chain. Without this, the ESP32
 * would reject the connection as untrusted.
 * ============================================================================ */

/**
 * @brief HTTP event handler for Telegram API requests
 * 
 * This callback is invoked by esp_http_client during the HTTP request lifecycle.
 * We use it to accumulate response data into a buffer as chunks arrive.
 * 
 * HTTP responses can arrive in multiple chunks (especially over TLS), so we
 * append each chunk to a dynamically allocated buffer. The buffer pointer is
 * passed via the user_data field of the HTTP client config.
 * 
 * EVENT LIFECYCLE:
 * 1. HTTP_EVENT_ON_CONNECTED - connection established (we ignore this)
 * 2. HTTP_EVENT_ON_DATA - response data chunk received (we accumulate)
 * 3. HTTP_EVENT_ON_FINISH - response complete (we ignore this)
 * 4. HTTP_EVENT_DISCONNECTED - connection closed (we ignore this)
 * 
 * @param evt HTTP client event data containing event type and chunk info
 * @return ESP_OK always (errors handled by caller checking HTTP status)
 */
static esp_err_t telegram_http_event_handler(esp_http_client_event_t *evt)
{
    // We only care about the ON_DATA event when actual data arrives
    // user_data points to a buffer struct we set up before the request
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA: {
            /*
             * evt->data contains the chunk of response data
             * evt->data_len is the size of this chunk
             * evt->user_data points to our response buffer (char*)
             * 
             * We need to track how much data we've accumulated. Since
             * esp_http_client doesn't give us the total received so far,
             * we use the output_len field of the client handle.
             * 
             * SAFETY: We check that we don't overflow the buffer.
             */
            if (evt->user_data) {
                // Get current length of accumulated data
                char *buf = (char *)evt->user_data;
                int current_len = strlen(buf);
                
                // Check if there's room for this chunk + null terminator
                if (current_len + evt->data_len < TELEGRAM_HTTP_BUF_SIZE) {
                    memcpy(buf + current_len, evt->data, evt->data_len);
                    buf[current_len + evt->data_len] = '\0';
                } else {
                    ESP_LOGW(TAG, "Telegram: HTTP response buffer overflow, truncating");
                }
            }
            break;
        }
        default:
            // Ignore all other events (CONNECTED, HEADERS_SENT, FINISH, etc.)
            break;
    }
    return ESP_OK;
}

/**
 * @brief Poll Telegram Bot API for new messages (getUpdates)
 * 
 * Makes an HTTPS GET request to the Telegram getUpdates endpoint to check
 * for new messages sent to the bot. Uses long polling: the request blocks
 * on the Telegram server for up to TELEGRAM_POLL_TIMEOUT seconds, returning
 * immediately if a new message arrives.
 * 
 * LONG POLLING EXPLAINED:
 * Instead of polling every second (which wastes bandwidth and battery),
 * long polling keeps the HTTP connection open. The Telegram server holds
 * the request until either:
 * - A new message arrives (returns immediately with the message)
 * - The timeout expires (returns with an empty result array)
 * This is much more efficient than short polling.
 * 
 * JSON RESPONSE FORMAT (simplified):
 * {
 *   "ok": true,
 *   "result": [
 *     {
 *       "update_id": 123456789,
 *       "message": {
 *         "text": "Hello!",          // For text messages
 *         "photo": [                  // For photo messages
 *           {"file_id": "...", "width": 90, "height": 90},
 *           {"file_id": "...", "width": 320, "height": 320},
 *           {"file_id": "...", "width": 800, "height": 800}
 *         ]
 *       }
 *     }
 *   ]
 * }
 * 
 * NOTE ON PHOTO ARRAY:
 * Telegram provides each photo in multiple sizes (thumbnails to full resolution).
 * The last element in the array is the largest/highest quality version.
 * We always pick the last element's file_id for the best quality.
 * 
 * @return ESP_OK if polling succeeded (even if no new messages),
 *         ESP_FAIL on HTTP or parsing errors
 */
static esp_err_t telegram_get_updates(void)
{
    // Build the getUpdates URL with offset and timeout parameters
    // The offset parameter ensures we only get NEW updates (not already processed)
    // The timeout parameter enables long polling (server waits N seconds for new msgs)
    char url[256];
    snprintf(url, sizeof(url),
             TELEGRAM_API_URL "/getUpdates?offset=%lld&timeout=%d",
             telegram_update_offset, TELEGRAM_POLL_TIMEOUT);
    
    // Allocate response buffer on heap (4KB is too much for stack)
    // calloc zeroes the buffer, so strlen() works correctly in the event handler
    char *response_buf = calloc(1, TELEGRAM_HTTP_BUF_SIZE);
    if (response_buf == NULL) {
        ESP_LOGE(TAG, "Telegram: Failed to allocate response buffer");
        return ESP_FAIL;
    }
    
    /* ====================================================================
     * Configure the HTTP client for the Telegram API request
     * ====================================================================
     * 
     * Key settings:
     * - url: The full getUpdates URL with parameters
     * - method: GET (Telegram Bot API accepts GET for simple requests)
     * - event_handler: Our callback that accumulates response data
     * - user_data: Pointer to our response buffer
     * - crt_bundle_attach: Attaches the ESP-IDF certificate bundle for TLS
     *   This is REQUIRED because Telegram uses HTTPS and we need to verify
     *   the server's certificate chain
     * - timeout_ms: Total request timeout (long poll timeout + extra for network)
     */
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .event_handler = telegram_http_event_handler,
        .user_data = response_buf,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = (TELEGRAM_POLL_TIMEOUT + 5) * 1000,  // Poll timeout + 5s buffer
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Telegram: Failed to init HTTP client");
        free(response_buf);
        return ESP_FAIL;
    }
    
    // Perform the HTTP request (blocks until response received or timeout)
    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    
    // Clean up the HTTP client handle (must be done before processing response)
    esp_http_client_cleanup(client);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Telegram: HTTP request failed: %s", esp_err_to_name(err));
        free(response_buf);
        return ESP_FAIL;
    }
    
    if (status_code != 200) {
        ESP_LOGE(TAG, "Telegram: HTTP status %d (expected 200)", status_code);
        ESP_LOGD(TAG, "Response: %s", response_buf);
        free(response_buf);
        return ESP_FAIL;
    }
    
    /* ====================================================================
     * Parse the JSON response using cJSON
     * ====================================================================
     * 
     * cJSON parses the JSON string into a tree of cJSON objects that we
     * can traverse to extract fields. We must call cJSON_Delete() when
     * done to free the parsed tree.
     * 
     * Response structure:
     * {
     *   "ok": true/false,
     *   "result": [ array of update objects ]
     * }
     */
    cJSON *json = cJSON_Parse(response_buf);
    free(response_buf);  // Raw JSON string no longer needed
    
    if (json == NULL) {
        ESP_LOGE(TAG, "Telegram: Failed to parse JSON response");
        return ESP_FAIL;
    }
    
    // Check the "ok" field - Telegram returns {"ok": false} on API errors
    cJSON *ok = cJSON_GetObjectItem(json, "ok");
    if (!cJSON_IsTrue(ok)) {
        ESP_LOGE(TAG, "Telegram: API returned ok=false");
        cJSON_Delete(json);
        return ESP_FAIL;
    }
    
    // Get the "result" array containing update objects
    cJSON *result = cJSON_GetObjectItem(json, "result");
    if (!cJSON_IsArray(result)) {
        ESP_LOGW(TAG, "Telegram: 'result' is not an array");
        cJSON_Delete(json);
        return ESP_OK;  // Not an error, just unexpected format
    }
    
    // Iterate over each update in the result array
    int update_count = cJSON_GetArraySize(result);
    
    for (int i = 0; i < update_count; i++) {
        cJSON *update = cJSON_GetArrayItem(result, i);
        
        /* ==============================================================
         * Extract update_id - used to track which updates we've processed
         * ==============================================================
         * The offset parameter in getUpdates = last_update_id + 1
         * This tells Telegram "I've processed everything up to this ID,
         * only send me newer updates"
         */
        cJSON *update_id = cJSON_GetObjectItem(update, "update_id");
        if (cJSON_IsNumber(update_id)) {
            // Set offset to update_id + 1 so we don't receive this update again
            int64_t uid = (int64_t)update_id->valuedouble;
            if (uid >= telegram_update_offset) {
                telegram_update_offset = uid + 1;
            }
        }
        
        // Get the "message" object from this update
        cJSON *message = cJSON_GetObjectItem(update, "message");
        if (message == NULL) {
            // Some updates don't have a "message" (e.g., edited messages,
            // channel posts). Skip those.
            continue;
        }
        
        /* ==============================================================
         * Check for TEXT MESSAGES (message.text)
         * ==============================================================
         * If the message contains a "text" field, it's a text message.
         * We store it in the shared variable for the web server to display.
         */
        cJSON *text = cJSON_GetObjectItem(message, "text");
        if (cJSON_IsString(text) && text->valuestring != NULL) {
            ESP_LOGI(TAG, "Telegram: Received text: \"%s\"", text->valuestring);
            
            // Update shared state (protected by mutex)
            if (xSemaphoreTake(telegram_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                strncpy(telegram_last_message, text->valuestring, 
                        TELEGRAM_MAX_MSG_LEN - 1);
                telegram_last_message[TELEGRAM_MAX_MSG_LEN - 1] = '\0';
                telegram_new_content = true;
                xSemaphoreGive(telegram_mutex);
            }
        }
        
        /* ==============================================================
         * Check for PHOTO MESSAGES (message.photo array)
         * ==============================================================
         * Photo messages have a "photo" field containing an array of
         * PhotoSize objects, each with a file_id and dimensions.
         * 
         * The array is sorted by size: smallest first, largest last.
         * We want the LARGEST version for best quality, so we take
         * the LAST element in the array.
         * 
         * Each PhotoSize object:
         * {
         *   "file_id": "AgACAgIAAxk...",
         *   "file_unique_id": "AQADAgAT...",
         *   "file_size": 12345,
         *   "width": 800,
         *   "height": 600
         * }
         */
        cJSON *photo = cJSON_GetObjectItem(message, "photo");
        if (cJSON_IsArray(photo)) {
            int photo_count = cJSON_GetArraySize(photo);
            if (photo_count > 0) {
                // Get the last (largest) photo in the array
                cJSON *largest = cJSON_GetArrayItem(photo, photo_count - 1);
                cJSON *file_id = cJSON_GetObjectItem(largest, "file_id");
                
                if (cJSON_IsString(file_id) && file_id->valuestring != NULL) {
                    ESP_LOGI(TAG, "Telegram: Received photo (file_id: %.20s...)",
                             file_id->valuestring);
                    
                    // Download the photo to SD card
                    // This is a two-step process handled by telegram_download_photo()
                    esp_err_t dl_err = telegram_download_photo(file_id->valuestring);
                    if (dl_err == ESP_OK) {
                        // Update shared state with the photo path
                        if (xSemaphoreTake(telegram_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                            strncpy(telegram_photo_path, TELEGRAM_PHOTO_PATH,
                                    sizeof(telegram_photo_path) - 1);
                            telegram_photo_path[sizeof(telegram_photo_path) - 1] = '\0';
                            telegram_new_content = true;
                            xSemaphoreGive(telegram_mutex);
                        }
                    }
                }
            }
        }
    }
    
    if (update_count > 0) {
        ESP_LOGI(TAG, "Telegram: Processed %d update(s), next offset: %lld",
                 update_count, telegram_update_offset);
    }
    
    cJSON_Delete(json);
    return ESP_OK;
}

/**
 * @brief Download a photo from Telegram and save it to the SD card
 * 
 * This implements the two-step Telegram photo download process:
 * 
 * STEP 1 - Get file path:
 *   Request: GET /bot<TOKEN>/getFile?file_id=<FILE_ID>
 *   Response: { "ok": true, "result": { "file_path": "photos/file_123.jpg" } }
 *   
 *   The file_id from getUpdates is not a URL - it's an opaque identifier.
 *   We must first call getFile to convert it into a downloadable path.
 * 
 * STEP 2 - Download the file:
 *   URL: https://api.telegram.org/file/bot<TOKEN>/<FILE_PATH>
 *   
 *   This downloads the actual binary image data. We save it directly to
 *   the SD card to avoid needing to buffer the entire image in RAM
 *   (photos can be several MB).
 * 
 * CHUNKED DOWNLOAD:
 * The photo is downloaded in chunks and written to the file incrementally.
 * This way, even large photos can be downloaded without running out of RAM.
 * We use a 2KB download buffer which is a good tradeoff between speed and
 * memory usage.
 * 
 * @param file_id The Telegram file_id string from the photo array
 * @return ESP_OK on successful download and save, ESP_FAIL on error
 */
static esp_err_t telegram_download_photo(const char *file_id)
{
    ESP_LOGI(TAG, "Telegram: Starting photo download...");
    
    /* ====================================================================
     * STEP 1: Call getFile to get the file_path
     * ====================================================================
     * The file_id is an opaque token, not a path. We need to ask the
     * Telegram API to convert it into a downloadable file_path.
     */
    char url[512];
    snprintf(url, sizeof(url), TELEGRAM_API_URL "/getFile?file_id=%s", file_id);
    
    // Allocate buffer for the getFile JSON response (small response)
    char *response_buf = calloc(1, 2048);
    if (response_buf == NULL) {
        ESP_LOGE(TAG, "Telegram: Failed to allocate getFile response buffer");
        return ESP_FAIL;
    }
    
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .event_handler = telegram_http_event_handler,
        .user_data = response_buf,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,  // 10 second timeout for getFile
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Telegram: Failed to init HTTP client for getFile");
        free(response_buf);
        return ESP_FAIL;
    }
    
    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    
    if (err != ESP_OK || status_code != 200) {
        ESP_LOGE(TAG, "Telegram: getFile request failed (err=%s, status=%d)",
                 esp_err_to_name(err), status_code);
        free(response_buf);
        return ESP_FAIL;
    }
    
    // Parse the getFile response to extract file_path
    // Response: { "ok": true, "result": { "file_path": "photos/file_123.jpg" } }
    cJSON *json = cJSON_Parse(response_buf);
    free(response_buf);
    
    if (json == NULL) {
        ESP_LOGE(TAG, "Telegram: Failed to parse getFile response");
        return ESP_FAIL;
    }
    
    cJSON *result = cJSON_GetObjectItem(json, "result");
    cJSON *file_path_json = cJSON_GetObjectItem(result, "file_path");
    
    if (!cJSON_IsString(file_path_json) || file_path_json->valuestring == NULL) {
        ESP_LOGE(TAG, "Telegram: No file_path in getFile response");
        cJSON_Delete(json);
        return ESP_FAIL;
    }
    
    // Build the full download URL
    // Format: https://api.telegram.org/file/bot<TOKEN>/<FILE_PATH>
    char download_url[512];
    snprintf(download_url, sizeof(download_url),
             TELEGRAM_FILE_URL "/%s", file_path_json->valuestring);
    
    ESP_LOGI(TAG, "Telegram: File path: %s", file_path_json->valuestring);
    cJSON_Delete(json);
    
    /* ====================================================================
     * STEP 2: Download the actual photo file
     * ====================================================================
     * Download the binary image data and save it directly to the SD card.
     * We use chunked downloading to handle large files without exhausting RAM.
     * 
     * APPROACH:
     * - Open the SD card file for writing
     * - Use esp_http_client_read() in a loop to read chunks
     * - Write each chunk to the file
     * - Close the file when download is complete
     */
    
    // Check if USB MSC is active - cannot write to SD card while PC has it mounted
    if (usb_msc_active) {
        ESP_LOGW(TAG, "Telegram: Cannot save photo - USB MSC is active");
        return ESP_FAIL;
    }
    
    // Open the output file on SD card
    FILE *photo_file = fopen(TELEGRAM_PHOTO_PATH, "wb");
    if (photo_file == NULL) {
        ESP_LOGE(TAG, "Telegram: Failed to open %s for writing", TELEGRAM_PHOTO_PATH);
        return ESP_FAIL;
    }
    
    // Configure HTTP client for binary download (no event handler, we read manually)
    esp_http_client_config_t dl_config = {
        .url = download_url,
        .method = HTTP_METHOD_GET,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 30000,  // 30 second timeout for photo download
    };
    
    esp_http_client_handle_t dl_client = esp_http_client_init(&dl_config);
    if (dl_client == NULL) {
        ESP_LOGE(TAG, "Telegram: Failed to init HTTP client for download");
        fclose(photo_file);
        return ESP_FAIL;
    }
    
    // Open the HTTP connection and send the request
    err = esp_http_client_open(dl_client, 0);  // 0 = no write data (GET request)
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Telegram: Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(dl_client);
        fclose(photo_file);
        return ESP_FAIL;
    }
    
    // Read response headers to get content length
    int content_length = esp_http_client_fetch_headers(dl_client);
    ESP_LOGI(TAG, "Telegram: Downloading photo (%d bytes)...", content_length);
    
    // Download in chunks and write to file
    // 2KB chunks balance between speed (fewer writes) and RAM usage
    char download_buf[2048];
    int total_downloaded = 0;
    int bytes_read;
    
    while ((bytes_read = esp_http_client_read(dl_client, download_buf, sizeof(download_buf))) > 0) {
        size_t written = fwrite(download_buf, 1, bytes_read, photo_file);
        if (written != (size_t)bytes_read) {
            ESP_LOGE(TAG, "Telegram: SD card write error (wrote %d of %d bytes)",
                     (int)written, bytes_read);
            break;
        }
        total_downloaded += bytes_read;
    }
    
    fclose(photo_file);
    esp_http_client_close(dl_client);
    esp_http_client_cleanup(dl_client);
    
    if (total_downloaded > 0) {
        ESP_LOGI(TAG, "Telegram: Photo saved to %s (%d bytes)", 
                 TELEGRAM_PHOTO_PATH, total_downloaded);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Telegram: Failed to download photo (0 bytes received)");
        // Remove the empty/incomplete file
        remove(TELEGRAM_PHOTO_PATH);
        return ESP_FAIL;
    }
}

/**
 * @brief FreeRTOS task for continuous Telegram Bot polling
 * 
 * This task runs in an infinite loop, continuously polling the Telegram Bot API
 * for new messages using long polling. It handles errors gracefully with
 * retry delays.
 * 
 * TASK BEHAVIOR:
 * 1. Wait for Wi-Fi to be connected before starting
 * 2. Call telegram_get_updates() which blocks for up to TELEGRAM_POLL_TIMEOUT seconds
 * 3. On success: immediately poll again (long polling handles the timing)
 * 4. On error: wait TELEGRAM_RETRY_DELAY_MS before retrying
 * 5. If Wi-Fi disconnects: pause polling and wait for reconnection
 * 
 * WHY A SEPARATE TASK:
 * The Telegram polling blocks for up to 10 seconds per request (long polling).
 * Running this in a separate FreeRTOS task prevents it from blocking other
 * functionality like the LCD clock update or the web server.
 * 
 * STACK SIZE:
 * 8192 bytes is needed because:
 * - HTTP client uses stack for TLS buffers
 * - cJSON parsing allocates from heap but uses some stack
 * - URL string formatting uses stack buffers
 * 
 * @param pvParameters Task parameters (unused)
 */
static void telegram_poll_task(void *pvParameters)
{
    (void)pvParameters;
    
    ESP_LOGI(TAG, "Telegram polling task started");
    ESP_LOGI(TAG, "  Bot token: %.10s... (truncated for security)", TELEGRAM_BOT_TOKEN);
    ESP_LOGI(TAG, "  Poll timeout: %d seconds", TELEGRAM_POLL_TIMEOUT);
    ESP_LOGI(TAG, "  Photo save path: %s", TELEGRAM_PHOTO_PATH);
    
    while (1) {
        // Only poll when Wi-Fi is connected (Telegram API requires internet)
        if (!wifi_connected) {
            ESP_LOGW(TAG, "Telegram: Wi-Fi disconnected, pausing polling...");
            vTaskDelay(pdMS_TO_TICKS(TELEGRAM_RETRY_DELAY_MS));
            continue;
        }
        
        // Poll for updates (blocks for up to TELEGRAM_POLL_TIMEOUT seconds)
        esp_err_t ret = telegram_get_updates();
        
        if (ret != ESP_OK) {
            // Error occurred - wait before retrying to avoid hammering the API
            ESP_LOGW(TAG, "Telegram: Poll failed, retrying in %d ms...",
                     TELEGRAM_RETRY_DELAY_MS);
            vTaskDelay(pdMS_TO_TICKS(TELEGRAM_RETRY_DELAY_MS));
        }
        // On success, loop immediately - long polling already provides the delay
        // (the request blocks for TELEGRAM_POLL_TIMEOUT seconds on the server)
    }
}

/**
 * @brief Start the Telegram polling task
 * 
 * Creates the FreeRTOS task that continuously polls the Telegram Bot API.
 * Also creates the mutex used to protect shared Telegram state variables.
 * 
 * PREREQUISITES:
 * - Wi-Fi must be initialized (doesn't need to be connected yet)
 * - The task will wait for Wi-Fi connection before polling
 * 
 * @return ESP_OK on success, ESP_FAIL on task creation failure
 */
static esp_err_t telegram_poll_task_start(void)
{
    ESP_LOGI(TAG, "Starting Telegram polling task...");
    
    // Create the mutex for protecting shared Telegram variables
    // This must be created before the task starts
    if (telegram_mutex == NULL) {
        telegram_mutex = xSemaphoreCreateMutex();
        if (telegram_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create Telegram mutex");
            return ESP_FAIL;
        }
    }
    
    // Create the polling task
    // Stack: 8192 bytes (HTTP+TLS needs substantial stack space)
    // Priority: 3 (above clock task at 2, but below time-critical tasks)
    BaseType_t ret = xTaskCreate(
        telegram_poll_task,     // Task function
        "telegram_poll",        // Task name (for debugging)
        8192,                   // Stack size in bytes
        NULL,                   // Task parameters
        3,                      // Priority
        &telegram_task_handle   // Task handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Telegram polling task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Telegram polling task created successfully");
    return ESP_OK;
}

/* ============================================================================
 * HTTP WEB SERVER IMPLEMENTATION (Week 5 - Phase 4)
 * ============================================================================
 * 
 * This section implements a simple HTTP web server running on port 80.
 * The server provides three endpoints:
 * 
 * 1. GET / (Root page):
 *    Returns an HTML page displaying the latest Telegram message and/or photo.
 *    Auto-refreshes every 10 seconds to show new content.
 * 
 * 2. GET /photo:
 *    Serves the latest photo received from Telegram as a JPEG image.
 *    Reads the file from SD card and sends it with Content-Type: image/jpeg.
 * 
 * 3. GET /status:
 *    Returns a JSON object with device status information (Wi-Fi, NTP, heap, etc.)
 * 
 * HOW ESP-IDF HTTP SERVER WORKS:
 * ------------------------------
 * The esp_http_server component creates a lightweight HTTP server that:
 * - Listens on the specified port (80 by default)
 * - Runs in its own FreeRTOS task
 * - Dispatches incoming requests to registered URI handlers
 * - Handles multiple simultaneous connections
 * - Does NOT support HTTPS (plain HTTP only - acceptable for local network)
 * 
 * URI HANDLER REGISTRATION:
 * Each handler is registered with a URI pattern, HTTP method, and callback.
 * When a matching request arrives, the callback is invoked with an httpd_req_t
 * pointer that provides access to request data and response functions.
 * 
 * THREAD SAFETY:
 * The HTTP server runs in its own task context. When accessing shared variables
 * (like telegram_last_message), we must use the telegram_mutex to prevent
 * data races with the Telegram polling task.
 * ============================================================================ */

/**
 * @brief HTTP GET handler for the root page '/'
 * 
 * Returns an HTML page that displays:
 * - The latest text message from Telegram (if any)
 * - The latest photo from Telegram (if any), via an <img> tag to /photo
 * - "No messages yet" if nothing has been received
 * - Auto-refreshes every 10 seconds via <meta http-equiv="refresh">
 * 
 * HTML GENERATION APPROACH:
 * We build the HTML string dynamically using snprintf. The page includes:
 * - A <meta> refresh tag for auto-updating
 * - Inline CSS for basic styling (dark theme, centered layout)
 * - Conditional content based on telegram state
 * 
 * BUFFER SIZE:
 * We use a 2KB buffer for the HTML response. This is sufficient for our
 * simple page. If the message is very long, it will be truncated by snprintf.
 * 
 * @param req HTTP request handle (provided by the server framework)
 * @return ESP_OK on success, ESP_FAIL on error
 */
static esp_err_t http_root_handler(httpd_req_t *req)
{
    // Allocate HTML response buffer on heap (too large for stack)
    // 2KB is sufficient for our simple page layout
    char *html = malloc(2048);
    if (html == NULL) {
        ESP_LOGE(TAG, "HTTP: Failed to allocate HTML buffer");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    /* ====================================================================
     * Build the HTML page
     * ====================================================================
     * We read the shared Telegram state (protected by mutex) and build
     * the HTML content accordingly.
     * 
     * Page structure:
     * - DOCTYPE + head with meta refresh (10 seconds)
     * - Inline CSS for dark background, white text, centered layout
     * - Title header
     * - Message section (text or "No messages yet")
     * - Photo section (img tag or nothing)
     * - Status footer showing Wi-Fi and NTP info
     */
    
    // Temporary buffers for mutex-protected reads
    char msg_copy[TELEGRAM_MAX_MSG_LEN] = "";
    char photo_copy[128] = "";
    
    // Take the mutex to safely read shared Telegram state
    // Use a short timeout since the web server should respond quickly
    if (xSemaphoreTake(telegram_mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        strncpy(msg_copy, telegram_last_message, sizeof(msg_copy) - 1);
        msg_copy[sizeof(msg_copy) - 1] = '\0';
        strncpy(photo_copy, telegram_photo_path, sizeof(photo_copy) - 1);
        photo_copy[sizeof(photo_copy) - 1] = '\0';
        xSemaphoreGive(telegram_mutex);
    }
    
    // Determine what content to show
    bool has_message = (strlen(msg_copy) > 0);
    bool has_photo = (strlen(photo_copy) > 0);
    
    /* ====================================================================
     * HTML TEMPLATE
     * ====================================================================
     * The page uses inline CSS for simplicity (no external stylesheets).
     * 
     * Key CSS choices:
     * - Dark background (#1a1a2e) with light text for readability
     * - Max-width container (600px) centered on page
     * - Responsive image (max-width: 100%) for mobile devices
     * - Card-style sections with rounded corners
     * - Meta refresh every 10 seconds to auto-update content
     */
    int len = snprintf(html, 2048,
        "<!DOCTYPE html>"
        "<html><head>"
        "<meta charset='UTF-8'>"
        "<meta http-equiv='refresh' content='10'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
        "<title>ESP32-S3 Telegram Display</title>"
        "<style>"
        "body{background:#1a1a2e;color:#e0e0e0;font-family:Arial,sans-serif;"
        "margin:0;padding:20px;}"
        ".container{max-width:600px;margin:0 auto;}"
        "h1{color:#00d4ff;text-align:center;border-bottom:2px solid #00d4ff;"
        "padding-bottom:10px;}"
        ".card{background:#16213e;border-radius:8px;padding:15px;margin:15px 0;"
        "border:1px solid #0f3460;}"
        ".card h2{color:#00d4ff;margin-top:0;font-size:1.1em;}"
        ".msg{font-size:1.2em;word-wrap:break-word;}"
        "img{max-width:100%%;border-radius:8px;display:block;margin:10px auto;}"
        ".status{font-size:0.85em;color:#888;text-align:center;margin-top:20px;}"
        ".none{color:#666;font-style:italic;}"
        "</style></head><body>"
        "<div class='container'>"
        "<h1>ESP32-S3 Display</h1>"
        /* Message card */
        "<div class='card'>"
        "<h2>Latest Message</h2>"
        "%s"  /* Message content or placeholder */
        "</div>"
        /* Photo card (only shown if photo exists) */
        "%s"  /* Photo section or empty string */
        /* Status footer */
        "<div class='status'>"
        "WiFi: %s | NTP: %s | IP: %s<br>"
        "Auto-refresh: 10s"
        "</div>"
        "</div></body></html>",
        /* Message content */
        has_message 
            ? msg_copy 
            : "<p class='none'>No messages yet. Send a message to the Telegram bot.</p>",
        /* Photo section */
        has_photo 
            ? "<div class='card'><h2>Latest Photo</h2>"
              "<img src='/photo' alt='Telegram Photo'></div>"
            : "",
        /* Status values */
        wifi_connected ? "Connected" : "Disconnected",
        ntp_synced ? "Synced" : "Waiting",
        wifi_ip_str
    );
    
    // Set Content-Type to text/html and send the response
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, len);
    
    free(html);
    ESP_LOGD(TAG, "HTTP: Served root page (msg=%s, photo=%s)",
             has_message ? "yes" : "no", has_photo ? "yes" : "no");
    
    return ESP_OK;
}

/**
 * @brief HTTP GET handler for '/photo' - serves the Telegram photo from SD card
 * 
 * This handler reads the latest photo file from the SD card and sends it
 * as an HTTP response with Content-Type: image/jpeg.
 * 
 * CHUNKED TRANSFER:
 * Photos can be several hundred KB to a few MB. Instead of loading the
 * entire file into RAM, we use httpd_resp_send_chunk() to send the file
 * in 2KB chunks. This keeps RAM usage bounded regardless of file size.
 * 
 * The chunked transfer approach:
 * 1. Open the file and get its size
 * 2. Set Content-Type to image/jpeg
 * 3. Read 2KB from file, send as a chunk, repeat
 * 4. Send a zero-length chunk to signal end of response
 * 
 * ERROR HANDLING:
 * - If no photo path is stored: return 404
 * - If the file doesn't exist on SD card: return 404
 * - If USB MSC is active (SD card busy): return 503 Service Unavailable
 * 
 * @param req HTTP request handle
 * @return ESP_OK on success, ESP_FAIL on error
 */
static esp_err_t http_photo_handler(httpd_req_t *req)
{
    // Get the photo path (protected by mutex)
    char photo_path[128] = "";
    if (xSemaphoreTake(telegram_mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        strncpy(photo_path, telegram_photo_path, sizeof(photo_path) - 1);
        photo_path[sizeof(photo_path) - 1] = '\0';
        xSemaphoreGive(telegram_mutex);
    }
    
    // Check if we have a photo path at all
    if (strlen(photo_path) == 0) {
        ESP_LOGD(TAG, "HTTP: /photo requested but no photo available");
        httpd_resp_send_404(req);
        return ESP_OK;  // Not an error - just no photo yet
    }
    
    // Check if USB MSC is active (SD card may not be accessible for file reads)
    // Note: In our setup FAT remains mounted even with USB MSC, but concurrent
    // access could cause issues. Log a warning but still try to serve the file.
    if (usb_msc_active) {
        ESP_LOGW(TAG, "HTTP: Serving photo while USB MSC active - may cause issues");
    }
    
    // Open the photo file from SD card
    FILE *photo_file = fopen(photo_path, "rb");
    if (photo_file == NULL) {
        ESP_LOGW(TAG, "HTTP: Photo file not found: %s", photo_path);
        httpd_resp_send_404(req);
        return ESP_OK;
    }
    
    // Get file size for logging (seek to end, get position, seek back)
    fseek(photo_file, 0, SEEK_END);
    long file_size = ftell(photo_file);
    fseek(photo_file, 0, SEEK_SET);
    ESP_LOGI(TAG, "HTTP: Serving photo %s (%ld bytes)", photo_path, file_size);
    
    // Set Content-Type to JPEG
    // This tells the browser to display the response as an image
    httpd_resp_set_type(req, "image/jpeg");
    
    /* ====================================================================
     * Send the file in chunks using httpd_resp_send_chunk()
     * ====================================================================
     * 
     * httpd_resp_send_chunk() sends a portion of the response. The HTTP
     * server automatically uses Transfer-Encoding: chunked when this
     * function is called multiple times.
     * 
     * We use a 2KB buffer which is a good balance between:
     * - Speed (fewer chunks = fewer syscalls)
     * - RAM usage (2KB is modest)
     * 
     * After all data is sent, we must send a zero-length chunk to signal
     * the end of the chunked response.
     */
    char *chunk_buf = malloc(2048);
    if (chunk_buf == NULL) {
        ESP_LOGE(TAG, "HTTP: Failed to allocate chunk buffer");
        fclose(photo_file);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    size_t bytes_read;
    while ((bytes_read = fread(chunk_buf, 1, 2048, photo_file)) > 0) {
        // Send this chunk of the file
        esp_err_t err = httpd_resp_send_chunk(req, chunk_buf, bytes_read);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "HTTP: Failed to send photo chunk");
            free(chunk_buf);
            fclose(photo_file);
            return ESP_FAIL;
        }
    }
    
    // Send zero-length chunk to signal end of response
    httpd_resp_send_chunk(req, NULL, 0);
    
    free(chunk_buf);
    fclose(photo_file);
    
    ESP_LOGD(TAG, "HTTP: Photo served successfully");
    return ESP_OK;
}

/**
 * @brief HTTP GET handler for '/status' - returns device status as JSON
 * 
 * Returns a JSON object containing current device information:
 * - wifi_status: "connected" or "disconnected"
 * - ip_address: Current IP address string
 * - ntp_synced: true or false
 * - current_time: Human-readable local time string (Los Angeles)
 * - last_message: Latest Telegram text message
 * - free_heap: Available heap memory in bytes
 * - uptime_seconds: Seconds since boot
 * 
 * This endpoint is useful for:
 * - Monitoring the device remotely
 * - Debugging connectivity issues
 * - Integration with external monitoring tools
 * 
 * RESPONSE FORMAT:
 * {
 *   "wifi_status": "connected",
 *   "ip_address": "192.168.1.100",
 *   "ntp_synced": true,
 *   "current_time": "2026-02-25 14:30:45 PST",
 *   "last_message": "Hello from Telegram!",
 *   "free_heap": 123456,
 *   "uptime_seconds": 3600
 * }
 * 
 * @param req HTTP request handle
 * @return ESP_OK on success, ESP_FAIL on error
 */
static esp_err_t http_status_handler(httpd_req_t *req)
{
    /* ====================================================================
     * Build the JSON response using cJSON
     * ====================================================================
     * cJSON provides a convenient API for building JSON objects:
     * - cJSON_CreateObject(): Create a JSON object {}
     * - cJSON_AddStringToObject(): Add "key": "string"
     * - cJSON_AddBoolToObject(): Add "key": true/false
     * - cJSON_AddNumberToObject(): Add "key": 12345
     * - cJSON_PrintUnformatted(): Convert to compact JSON string
     */
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "HTTP: Failed to create JSON object");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // Wi-Fi status and IP address
    cJSON_AddStringToObject(root, "wifi_status", 
                            wifi_connected ? "connected" : "disconnected");
    cJSON_AddStringToObject(root, "ip_address", wifi_ip_str);
    
    // NTP sync status
    cJSON_AddBoolToObject(root, "ntp_synced", ntp_synced);
    
    // Current local time (Los Angeles)
    // Use time()/localtime()/strftime() which respects the TZ setting
    char time_str[64] = "not synced";
    if (ntp_synced) {
        time_t now = time(NULL);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S %Z", &timeinfo);
    }
    cJSON_AddStringToObject(root, "current_time", time_str);
    
    // Last Telegram message (mutex-protected read)
    char msg_copy[TELEGRAM_MAX_MSG_LEN] = "";
    if (xSemaphoreTake(telegram_mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        strncpy(msg_copy, telegram_last_message, sizeof(msg_copy) - 1);
        msg_copy[sizeof(msg_copy) - 1] = '\0';
        xSemaphoreGive(telegram_mutex);
    }
    cJSON_AddStringToObject(root, "last_message", 
                            strlen(msg_copy) > 0 ? msg_copy : "(none)");
    
    // Free heap memory - useful for monitoring memory leaks
    // esp_get_free_heap_size() returns the total free heap across all regions
    cJSON_AddNumberToObject(root, "free_heap", (double)esp_get_free_heap_size());
    
    // System uptime in seconds
    // esp_timer_get_time() returns microseconds since boot
    int64_t uptime_us = esp_timer_get_time();
    cJSON_AddNumberToObject(root, "uptime_seconds", (double)(uptime_us / 1000000));
    
    // Convert cJSON tree to a compact JSON string
    // cJSON_PrintUnformatted() allocates a string that we must free
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);  // Free the cJSON tree (no longer needed)
    
    if (json_str == NULL) {
        ESP_LOGE(TAG, "HTTP: Failed to print JSON");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // Set Content-Type to application/json and send
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    
    free(json_str);  // Free the JSON string allocated by cJSON_Print
    
    ESP_LOGD(TAG, "HTTP: Served /status endpoint");
    return ESP_OK;
}

/**
 * @brief Start the HTTP web server on port 80
 * 
 * Creates and starts an HTTP server instance, then registers all URI handlers.
 * The server runs in its own FreeRTOS task (managed by the esp_http_server
 * component) and handles incoming HTTP requests asynchronously.
 * 
 * CONFIGURATION:
 * - Port: 80 (standard HTTP port)
 * - Max URI handlers: 8 (we use 3, leaving room for future endpoints)
 * - Stack size: default (4096 bytes, set by esp_http_server)
 * - Max open sockets: default (7, sufficient for our use case)
 * 
 * URI HANDLERS REGISTERED:
 * - GET /       -> http_root_handler (main page with Telegram content)
 * - GET /photo  -> http_photo_handler (serve JPEG from SD card)
 * - GET /status -> http_status_handler (JSON device status)
 * 
 * PREREQUISITES:
 * - Wi-Fi must be connected (server binds to the Wi-Fi interface IP)
 * - telegram_mutex must be created (handlers read shared state)
 * 
 * ACCESS:
 * After starting, the server is reachable at http://<ESP32_IP>/ from any
 * device on the same network.
 * 
 * @return ESP_OK on success, ESP_FAIL if server fails to start
 */
static esp_err_t http_server_start(void)
{
    ESP_LOGI(TAG, "Starting HTTP web server...");
    
    /* ====================================================================
     * Configure the HTTP server
     * ====================================================================
     * HTTPD_DEFAULT_CONFIG() provides sensible defaults:
     * - Port: 80
     * - Task priority: 5
     * - Stack size: 4096
     * - Max open sockets: 7
     * - Max URI handlers: 8
     * 
     * We use the defaults but could customize if needed. For example,
     * increasing max_uri_handlers if we add more endpoints.
     */
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
    // Start the server
    // httpd_start() creates the server task and starts listening
    esp_err_t ret = httpd_start(&http_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "HTTP server started on port %d", config.server_port);
    
    /* ====================================================================
     * Register URI handlers
     * ====================================================================
     * Each URI handler is defined by an httpd_uri_t struct containing:
     * - .uri:     The URI path to match (e.g., "/")
     * - .method:  The HTTP method to match (e.g., HTTP_GET)
     * - .handler: The callback function to invoke
     * - .user_ctx: Optional user context pointer (we don't use it)
     * 
     * When a request matches both URI and method, the handler is called.
     * If no handler matches, the server returns 404 automatically.
     */
    
    // Root page handler: GET /
    // Displays the latest Telegram message and photo
    const httpd_uri_t root_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = http_root_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(http_server, &root_uri);
    ESP_LOGI(TAG, "  Registered: GET / (main page)");
    
    // Photo handler: GET /photo
    // Serves the latest Telegram photo from the SD card as JPEG
    const httpd_uri_t photo_uri = {
        .uri       = "/photo",
        .method    = HTTP_GET,
        .handler   = http_photo_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(http_server, &photo_uri);
    ESP_LOGI(TAG, "  Registered: GET /photo (JPEG image)");
    
    // Status handler: GET /status
    // Returns JSON with device status information
    const httpd_uri_t status_uri = {
        .uri       = "/status",
        .method    = HTTP_GET,
        .handler   = http_status_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(http_server, &status_uri);
    ESP_LOGI(TAG, "  Registered: GET /status (JSON status)");
    
    ESP_LOGI(TAG, "HTTP server ready - access at http://%s/", wifi_ip_str);
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
 * @brief FreeRTOS task for updating the LCD with cycling display modes
 * 
 * This task runs continuously and cycles the LCD between three display modes:
 * 
 * MODE 1 - DATE AND TIME (NTP-synced, Los Angeles):
 * +----------------+
 * |MM/DD SD:Y NT:Y |  <- Row 0: Date + SD/NTP status indicators
 * |    HH:MM:SS    |  <- Row 1: Time (24-hour, centered)
 * +----------------+
 * Before NTP sync: shows "--/--" and "--:--:--" as placeholders.
 * 
 * MODE 2 - WI-FI STATUS AND IP ADDRESS:
 * +----------------+
 * |WiFi: Connected |  <- Row 0: Connection state
 * |192.168.1.100   |  <- Row 1: IP address (left-aligned)
 * +----------------+
 * If not connected: "WiFi: No Conn" and "No IP" on row 1.
 * 
 * MODE 3 - LAST TELEGRAM MESSAGE:
 * +----------------+
 * |Telegram:       |  <- Row 0: Label
 * |Hello from bot! |  <- Row 1: Message text (scrolls if > 16 chars)
 * +----------------+
 * If no message: shows "(no messages)" on row 1.
 * Scrolling: If the message is longer than 16 characters, it scrolls
 * horizontally by advancing a scroll offset each update cycle.
 * 
 * MODE CYCLING:
 * Each mode is shown for DISPLAY_MODE_DURATION_MS (5000ms = 5 seconds).
 * Within each mode, the display updates at 200ms intervals for smooth
 * scrolling (Mode 3) and responsive time updates (Mode 1).
 * When switching modes, the LCD is redrawn completely.
 * 
 * WHY CYCLING INSTEAD OF BUTTONS:
 * The ESP32-S3 Freenove board doesn't have dedicated user buttons easily
 * accessible. Auto-cycling is simpler and shows all information without
 * user interaction. The 5-second duration per mode gives enough time to
 * read each screen.
 * 
 * TIME SOURCE: POSIX time()/localtime() backed by SNTP sync.
 * THREAD SAFETY: Telegram message read is mutex-protected.
 * 
 * @param pvParameters Task parameters (unused)
 */

// Duration in milliseconds to show each display mode before cycling
#define DISPLAY_MODE_DURATION_MS  5000

// Number of display modes
#define DISPLAY_MODE_COUNT        3

static void clock_task(void *pvParameters)
{
    (void)pvParameters;  // Unused
    
    // Buffers for LCD row content (16 chars + null terminator)
    // Using 32-byte buffers to prevent snprintf truncation warnings
    char row0_str[32];
    char row1_str[32];
    
    /* ================================================================
     * Display mode state variables
     * ================================================================
     * display_mode: Current mode (0=time, 1=wifi, 2=telegram)
     * mode_start_tick: Tick count when current mode started
     * scroll_offset: For Mode 3 message scrolling (character offset)
     * prev_second: Track time changes to reduce LCD writes in Mode 1
     * mode_just_changed: Force full redraw when switching modes
     */
    int display_mode = 0;            // Start with date/time mode
    TickType_t mode_start_tick = xTaskGetTickCount();
    int scroll_offset = 0;           // Telegram message scroll position
    int prev_second = -1;            // Force first update in time mode
    bool mode_just_changed = true;   // Force full draw on first iteration
    
    ESP_LOGI(TAG, "Clock display task started (multi-mode)");
    ESP_LOGI(TAG, "Display modes: 0=Date/Time, 1=WiFi/IP, 2=Telegram");
    ESP_LOGI(TAG, "Mode cycle interval: %d ms", DISPLAY_MODE_DURATION_MS);
    
    while (1) {
        /* ================================================================
         * CHECK IF IT'S TIME TO SWITCH DISPLAY MODES
         * ================================================================
         * Compare elapsed ticks since mode started against the duration.
         * pdMS_TO_TICKS() converts milliseconds to FreeRTOS ticks.
         * When cycling, reset scroll offset and force a full redraw.
         */
        TickType_t elapsed = xTaskGetTickCount() - mode_start_tick;
        if (elapsed >= pdMS_TO_TICKS(DISPLAY_MODE_DURATION_MS)) {
            // Advance to next mode (wrap around using modulo)
            display_mode = (display_mode + 1) % DISPLAY_MODE_COUNT;
            mode_start_tick = xTaskGetTickCount();
            scroll_offset = 0;       // Reset scroll for new mode cycle
            prev_second = -1;         // Force time redraw when returning to mode 0
            mode_just_changed = true; // Force full LCD redraw
            
            ESP_LOGD(TAG, "LCD: Switched to display mode %d", display_mode);
        }
        
        /* ================================================================
         * RENDER CURRENT DISPLAY MODE
         * ================================================================
         * Each mode fills row0_str and row1_str (16 chars each),
         * then the common code at the bottom writes them to the LCD.
         */
        bool should_update = mode_just_changed;  // Always update on mode change
        
        switch (display_mode) {
            
            /* ============================================================
             * MODE 0: DATE AND TIME (NTP-synced Los Angeles time)
             * ============================================================
             * Row 0: "MM/DD SD:Y NT:Y "  (date + status indicators)
             * Row 1: "    HH:MM:SS    "  (centered time)
             * 
             * Updates once per second (when tm_sec changes).
             * Before NTP sync: "--/--" and "--:--:--" placeholders.
             * 
             * Status indicators:
             *   SD:Y/N = SD card mounted/failed
             *   NT:Y/N = NTP synchronized/waiting
             */
            case 0: {
                time_t now = time(NULL);
                struct tm timeinfo;
                localtime_r(&now, &timeinfo);
                
                // Only redraw if the second changed (avoid LCD flicker)
                if (timeinfo.tm_sec != prev_second || mode_just_changed) {
                    should_update = true;
                    prev_second = timeinfo.tm_sec;
                    
                    // Row 0: Date and status
                    if (ntp_synced) {
                        snprintf(row0_str, sizeof(row0_str), "%02d/%02d SD:%c NT:Y ",
                                 timeinfo.tm_mon + 1, timeinfo.tm_mday,
                                 sd_card_initialized ? 'Y' : 'N');
                    } else {
                        snprintf(row0_str, sizeof(row0_str), "--/-- SD:%c NT:N ",
                                 sd_card_initialized ? 'Y' : 'N');
                    }
                    row0_str[16] = '\0';
                    
                    // Row 1: Centered time
                    if (ntp_synced) {
                        char time_part[12];
                        strftime(time_part, sizeof(time_part), "%H:%M:%S", &timeinfo);
                        snprintf(row1_str, sizeof(row1_str), "    %s    ", time_part);
                    } else {
                        snprintf(row1_str, sizeof(row1_str), "    --:--:--    ");
                    }
                    row1_str[16] = '\0';
                }
                break;
            }
            
            /* ============================================================
             * MODE 1: WI-FI STATUS AND IP ADDRESS
             * ============================================================
             * Row 0: "WiFi: Connected " or "WiFi: No Conn   "
             * Row 1: "192.168.1.100   " (IP, left-aligned, padded)
             * 
             * This mode only needs to update when it first appears
             * (mode_just_changed) since WiFi status rarely changes.
             * We still refresh it each cycle for simplicity.
             */
            case 1: {
                should_update = true;  // Always redraw in WiFi mode (simple)
                
                // Row 0: Connection status
                if (wifi_connected) {
                    snprintf(row0_str, sizeof(row0_str), "WiFi: Connected ");
                } else {
                    snprintf(row0_str, sizeof(row0_str), "WiFi: No Conn   ");
                }
                row0_str[16] = '\0';
                
                // Row 1: IP address (left-aligned, padded to 16 chars)
                // Use %.16s to limit output to 16 chars max, preventing
                // format-truncation warning with longer IP strings
                if (wifi_connected && strlen(wifi_ip_str) > 0) {
                    snprintf(row1_str, sizeof(row1_str), "%.16s", wifi_ip_str);
                    // Pad remaining characters with spaces up to 16 chars
                    int ip_len = strlen(row1_str);
                    for (int i = ip_len; i < 16; i++) {
                        row1_str[i] = ' ';
                    }
                    row1_str[16] = '\0';
                } else {
                    snprintf(row1_str, sizeof(row1_str), "No IP           ");
                }
                row1_str[16] = '\0';
                break;
            }
            
            /* ============================================================
             * MODE 2: LAST TELEGRAM MESSAGE (with scrolling)
             * ============================================================
             * Row 0: "Telegram:       " (static label)
             * Row 1: Message text (scrolls if longer than 16 chars)
             * 
             * SCROLLING LOGIC:
             * If the message fits in 16 characters, display it static.
             * If longer, we use a "marquee" style scroll:
             * - Extract a 16-character window starting at scroll_offset
             * - Advance scroll_offset by 1 each update cycle (200ms)
             * - When the window reaches the end, wrap back to start
             * 
             * Example for "Hello World from Telegram Bot!":
             *   offset 0:  "Hello World from"
             *   offset 1:  "ello World from "
             *   offset 2:  "llo World from T"
             *   ...until end, then wrap to 0
             * 
             * MUTEX PROTECTION:
             * telegram_last_message is written by the Telegram polling
             * task and read here. We use telegram_mutex to prevent
             * reading a partially-written string.
             */
            case 2: {
                should_update = true;  // Always redraw (scrolling needs it)
                
                // Row 0: Static label
                snprintf(row0_str, sizeof(row0_str), "Telegram:       ");
                row0_str[16] = '\0';
                
                // Read the message (mutex-protected)
                char msg_copy[TELEGRAM_MAX_MSG_LEN] = "";
                if (xSemaphoreTake(telegram_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    strncpy(msg_copy, telegram_last_message, sizeof(msg_copy) - 1);
                    msg_copy[sizeof(msg_copy) - 1] = '\0';
                    xSemaphoreGive(telegram_mutex);
                }
                
                int msg_len = strlen(msg_copy);
                
                if (msg_len == 0) {
                    // No message received yet
                    snprintf(row1_str, sizeof(row1_str), "(no messages)   ");
                    scroll_offset = 0;
                } else if (msg_len <= 16) {
                    // Message fits on screen - display static, padded to 16 chars
                    // Use manual padding instead of %-16s to avoid truncation warning
                    memcpy(row1_str, msg_copy, msg_len);
                    memset(row1_str + msg_len, ' ', 16 - msg_len);
                    row1_str[16] = '\0';
                    scroll_offset = 0;
                } else {
                    /* ====================================================
                     * SCROLLING: Extract a 16-char window from the message
                     * ====================================================
                     * We pad the message with 4 trailing spaces so there's
                     * a visual gap before the message wraps around.
                     * 
                     * Total scroll length = msg_len + 4 (for padding)
                     * When scroll_offset reaches msg_len + 4, reset to 0.
                     */
                    int padded_len = msg_len + 4;  // 4 spaces of padding
                    
                    // Build 16-char window starting at scroll_offset
                    for (int i = 0; i < 16; i++) {
                        int src_idx = (scroll_offset + i) % padded_len;
                        if (src_idx < msg_len) {
                            row1_str[i] = msg_copy[src_idx];
                        } else {
                            row1_str[i] = ' ';  // Padding space
                        }
                    }
                    row1_str[16] = '\0';
                    
                    // Advance scroll position for next update cycle
                    scroll_offset = (scroll_offset + 1) % padded_len;
                }
                break;
            }
            
            default:
                // Should never happen, but reset to mode 0 if it does
                display_mode = 0;
                mode_just_changed = true;
                continue;
        }
        
        /* ================================================================
         * WRITE TO LCD (common for all modes)
         * ================================================================
         * Only write if content changed to reduce I2C bus traffic and
         * prevent LCD flicker. Use set_cursor (not clear) for smooth
         * transitions - overwriting existing characters is flicker-free.
         */
        if (should_update) {
            lcd_set_cursor(0, 0);
            lcd_print(row0_str);
            lcd_set_cursor(1, 0);
            lcd_print(row1_str);
        }
        
        mode_just_changed = false;
        
        /* ================================================================
         * TASK DELAY
         * ================================================================
         * 200ms update interval provides:
         * - Smooth scrolling in Mode 2 (5 chars/second is readable)
         * - Responsive time updates in Mode 0 (< 1 second latency)
         * - Reasonable CPU usage (5 updates/second)
         * 
         * Note: Mode 0 internally checks tm_sec to skip redundant writes,
         * so the 200ms polling doesn't cause excessive LCD updates there.
         */
        vTaskDelay(pdMS_TO_TICKS(200));
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
 * 1. NVS FLASH INITIALIZATION (Required by Wi-Fi and other components)
 *    - Initialize non-volatile storage partition
 *    - Handle corrupt/full partitions with erase + retry
 *    - WHY FIRST: Wi-Fi needs NVS for PHY calibration data.
 *      Other ESP-IDF components also use NVS for persistent settings.
 *      Must be initialized before any component that depends on it.
 * 
 * 2. SYNCHRONIZATION PRIMITIVES (Required by all phases for thread safety)
 *    - Create SD card access mutex for USB/local coordination
 * 
 * 3. SD CARD INITIALIZATION (Phase 2 - Foundation)
 *    - Initialize SDMMC bus for SD card communication
 *    - Mount FAT file system at /sdcard
 *    - Display SD card info (type, capacity)
 *    - WHY EARLY: USB MSC and file operations depend on this
 * 
 * 4. FILE OPERATIONS DEMO (Phase 3 - Verify SD works)
 *    - Create, read, append files on SD card
 *    - List directory contents
 *    - WHY BEFORE USB: Confirms SD card is working while we have exclusive access
 * 
 * 5. I2C AND LCD INITIALIZATION (Phase 5 - Display setup)
 *    - Initialize I2C master on GPIO 8/9
 *    - Initialize HD44780 LCD in 4-bit mode via PCF8574 I/O expander
 *    - Display startup message
 *    - WHY BEFORE Wi-Fi: Want to show connection progress on LCD
 * 
 * 6. WI-FI INITIALIZATION (Week 5 - Network connectivity)
 *    - Configure station mode for hidden SSID "Embedded"
 *    - Connect and obtain IP address via DHCP
 *    - WHY HERE: NVS ready, LCD ready to show status. Required before
 *      NTP, Telegram, and HTTP server which all need network access.
 * 
 * 7. NTP TIME SYNCHRONIZATION (Week 5 - Phase 2)
 *    - Configure SNTP client with time.nist.gov
 *    - Set POSIX TZ for Los Angeles (PST/PDT with DST)
 *    - Wait for first successful sync (up to 30 seconds)
 *    - WHY AFTER Wi-Fi: Requires network access to reach NTP server
 * 
 * 8. USB MASS STORAGE INITIALIZATION (Phase 4 - Takes over SD)
 *    - Initialize TinyUSB MSC with SD card
 *    - PC can now access SD card as removable drive
 *    - WHY AFTER NTP: SD file ops demo done, NTP doesn't need SD
 * 
 * 9. TELEGRAM BOT POLLING TASK (Week 5 - Phase 3)
 *    - Create mutex for shared Telegram state variables
 *    - Start FreeRTOS task that polls getUpdates in a loop
 *    - Receives text messages and photos from Telegram users
 *    - WHY AFTER USB: SD card writes for photos need to be coordinated
 * 
 * 10. HTTP WEB SERVER (Week 5 - Phase 4)
 *     - Start HTTP server on port 80 with 3 URI handlers
 *     - Serves Telegram content, photos, and JSON status
 *     - WHY AFTER TELEGRAM: Serves content from Telegram module
 * 
 * 11. LCD CLOCK DISPLAY TASK (Phase 6 - Continuous operation)
 *     - Start FreeRTOS task to cycle LCD between display modes
 *     - Mode 1: NTP-synced date/time (Los Angeles)
 *     - Mode 2: Wi-Fi status and IP address
 *     - Mode 3: Last Telegram message (scrolling)
 *     - WHY LAST: All display data sources must be ready
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
     * STEP 1: Initialize NVS Flash (Non-Volatile Storage)
     * ========================================================================
     * 
     * NVS provides a key-value storage mechanism in flash memory. It is
     * required by several ESP-IDF components:
     * - Wi-Fi: stores PHY calibration data and connection parameters
     * - Bluetooth (if used): stores pairing information
     * - Other components: persistent configuration settings
     * 
     * WHY FIRST: Must be ready before any component that uses NVS.
     * Wi-Fi will fail to initialize without NVS.
     * 
     * If the NVS partition is full or has a version mismatch (e.g., after
     * firmware update), we erase it and re-initialize. This is safe because
     * NVS data is just a cache (Wi-Fi recalibrates, settings have defaults).
     */
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "[1/11] Initializing NVS flash...");
    
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition is full or version mismatch - erase and retry
        ESP_LOGW(TAG, "       NVS flash needs erase (error: %s)", esp_err_to_name(ret));
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FATAL: NVS flash init failed: %s", esp_err_to_name(ret));
        ESP_LOGE(TAG, "Wi-Fi and other components will not work without NVS");
        return;
    }
    ESP_LOGI(TAG, "       NVS flash initialized successfully");
    
    /* ========================================================================
     * STEP 2: Create synchronization primitives
     * ========================================================================
     * 
     * The SD card access mutex coordinates between USB MSC operations and
     * local file operations. Although USB MSC has exclusive access while
     * connected, we need the mutex for safe transition between modes.
     */
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "[2/11] Creating synchronization primitives...");
    
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
    ESP_LOGI(TAG, "[3/11] Initializing SD card (Phase 2)...");
    
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
    ESP_LOGI(TAG, "[4/11] Running file operations demo (Phase 3)...");
    
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
    ESP_LOGI(TAG, "[5/11] Initializing I2C and LCD (Phase 5)...");
    
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
     * STEP 5: Initialize Wi-Fi (Week 5 - Network connectivity)
     * ========================================================================
     * 
     * Connect to the hidden Wi-Fi network. This must happen after LCD init
     * so we can show connection status, and before NTP sync (future phase)
     * which requires network access.
     * 
     * The Wi-Fi initialization:
     * 1. Initializes NVS flash (Wi-Fi stores calibration data there)
     * 2. Creates the network interface and event loop
     * 3. Configures station mode with hidden SSID scan
     * 4. Blocks until connected or all retries exhausted
     */
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "[6/11] Initializing Wi-Fi (Week 5)...");
    
    // Update LCD to show Wi-Fi connection in progress
    lcd_set_cursor(1, 0);
    lcd_print(" WiFi Connect.. ");
    
    ret = wifi_init_sta();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi initialization failed!");
        ESP_LOGE(TAG, "  SSID: %s (hidden)", WIFI_SSID);
        ESP_LOGE(TAG, "  Check: AP is powered on, in range, credentials correct");
        lcd_set_cursor(1, 0);
        lcd_print("  WiFi FAILED!  ");
        // Continue without Wi-Fi - NTP and web server will not work
        ESP_LOGW(TAG, "Continuing without Wi-Fi...");
    } else {
        ESP_LOGI(TAG, "       Wi-Fi connected, IP: %s", wifi_ip_str);
        // Briefly show IP on LCD before clock takes over
        lcd_set_cursor(1, 0);
        char ip_display[17];
        snprintf(ip_display, sizeof(ip_display), "%-16s", wifi_ip_str);
        lcd_print(ip_display);
        // Short delay so user can see the IP address
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    
    /* ========================================================================
     * STEP 6: Initialize NTP time synchronization (Week 5 - Phase 2)
     * ========================================================================
     * 
     * Now that Wi-Fi is connected, synchronize the system clock with
     * time.nist.gov via SNTP. This replaces the Week 4 software RTC:
     * - No more manual timekeeping with FreeRTOS timer callbacks
     * - System clock is set accurately via NTP
     * - time()/localtime() provide correct LA time with DST
     * - SNTP periodically re-syncs to correct oscillator drift
     * 
     * If Wi-Fi failed, NTP won't work but the SNTP client will keep
     * retrying in the background once connectivity is restored.
     */
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "[7/11] Initializing NTP time sync (Week 5)...");
    
    if (wifi_connected) {
        // Update LCD to show NTP sync in progress
        lcd_set_cursor(1, 0);
        lcd_print("  NTP Syncing.. ");
        
        ret = sntp_init_time();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "       NTP sync timed out - will retry in background");
            lcd_set_cursor(1, 0);
            lcd_print(" NTP Timeout... ");
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            ESP_LOGI(TAG, "       NTP synchronized - system clock set");
            // Briefly show the synced time on LCD
            time_t now = time(NULL);
            struct tm timeinfo;
            localtime_r(&now, &timeinfo);
            char time_display[17];
            strftime(time_display, sizeof(time_display), "  NTP %H:%M:%S  ", &timeinfo);
            lcd_set_cursor(1, 0);
            lcd_print(time_display);
            vTaskDelay(pdMS_TO_TICKS(1500));
        }
    } else {
        ESP_LOGW(TAG, "       Skipping NTP - Wi-Fi not connected");
        ESP_LOGW(TAG, "       Clock will show '--:--:--' until Wi-Fi connects and NTP syncs");
    }
    
    /* ========================================================================
     * STEP 8: Initialize USB Mass Storage (Phase 4)
     * ========================================================================
     * 
     * USB MSC takes exclusive access to the SD card. We must unmount the
     * local FAT filesystem first. After this, file operations from ESP32
     * require special handling (unmount USB, mount local, do operation,
     * unmount local, remount USB).
     */
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "[8/11] Initializing USB Mass Storage (Phase 4)...");
    
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
     * STEP 8: Start Telegram Bot polling task (Week 5 - Phase 3)
     * ========================================================================
     * 
     * Start the background task that polls the Telegram Bot API for new
     * messages and photos. The task handles:
     * - Long polling the getUpdates endpoint
     * - Storing received text messages in a shared variable
     * - Downloading and saving photos to the SD card
     * - Retry logic for network errors
     * 
     * Requires Wi-Fi to be initialized (the task will wait for connection
     * before starting to poll).
     */
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "[9/11] Starting Telegram Bot polling (Week 5)...");
    
    ret = telegram_poll_task_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Telegram polling task failed to start!");
        ESP_LOGW(TAG, "Continuing without Telegram bot...");
    } else {
        ESP_LOGI(TAG, "       Telegram polling task running");
    }
    
    /* ========================================================================
     * STEP 9: Start HTTP Web Server (Week 5 - Phase 4)
     * ========================================================================
     * 
     * Start the HTTP web server on port 80. This provides:
     * - GET /       - Main page showing latest Telegram message and photo
     * - GET /photo  - Serves the latest Telegram photo as JPEG
     * - GET /status - JSON endpoint with device status
     * 
     * Requires Wi-Fi to be connected so clients can reach the server.
     * The server runs in its own FreeRTOS task.
     */
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "[10/11] Starting HTTP web server (Week 5)...");
    
    if (wifi_connected) {
        ret = http_server_start();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "HTTP server failed to start!");
            ESP_LOGW(TAG, "Web interface will not be available");
        } else {
            ESP_LOGI(TAG, "       HTTP server running at http://%s/", wifi_ip_str);
        }
    } else {
        ESP_LOGW(TAG, "       Skipping HTTP server - Wi-Fi not connected");
    }
    
    /* ========================================================================
     * STEP 11: Start clock display task (Phase 6)
     * ========================================================================
     * 
     * Now that LCD and all data sources are initialized, start the task
     * that continuously updates the display, cycling between modes:
     * - Mode 1: NTP-synced date and time (Los Angeles)
     * - Mode 2: Wi-Fi status and IP address
     * - Mode 3: Last Telegram message (scrolling if longer than 16 chars)
     */
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "[11/11] Starting clock display task (Phase 6)...");
    
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
    ESP_LOGI(TAG, "  Wi-Fi:    %s", wifi_connected ? "Connected" : "Not connected");
    if (wifi_connected) {
        ESP_LOGI(TAG, "  IP Addr:  %s", wifi_ip_str);
    }
    ESP_LOGI(TAG, "  NTP:      %s", ntp_synced ? "Synchronized" : "Waiting for sync");
    ESP_LOGI(TAG, "  Telegram: Polling for messages");
    ESP_LOGI(TAG, "  HTTP:     %s", http_server != NULL ? "Running" : "Not started");
    if (http_server != NULL) {
        ESP_LOGI(TAG, "  Web URL:  http://%s/", wifi_ip_str);
    }
    ESP_LOGI(TAG, "  LCD:      Displaying clock");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "USAGE:");
    ESP_LOGI(TAG, "  1. Connect USB to PC - appears as removable drive");
    ESP_LOGI(TAG, "  2. Browse, create, modify files from PC");
    ESP_LOGI(TAG, "  3. Safely eject before disconnecting");
    ESP_LOGI(TAG, "  4. LCD shows NTP-synced Los Angeles time");
    ESP_LOGI(TAG, "  5. Send messages/photos to Telegram bot");
    ESP_LOGI(TAG, "  6. Visit http://%s/ for web interface", wifi_ip_str);
    ESP_LOGI(TAG, "  7. Visit http://%s/status for JSON status", wifi_ip_str);
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "NOTE: Time is synced via NTP (time.nist.gov)");
    ESP_LOGI(TAG, "      Timezone: America/Los_Angeles (PST/PDT)");
    ESP_LOGI(TAG, "      Telegram bot ready for messages and photos");
    ESP_LOGI(TAG, "========================================");
}
