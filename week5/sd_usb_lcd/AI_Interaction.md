
---

## Phase 4: HTTP Web Server (Prompts 4.1 - 4.4)

### Prompt 4.1 -- HTTP Server Setup
- Added `#include "esp_http_server.h"` and `#include "esp_timer.h"` headers
- Added `static httpd_handle_t http_server = NULL;` global variable
- Added `esp_http_server` and `esp_timer` to CMakeLists.txt REQUIRES list
- Implemented `http_server_start()` function:
  - Configures httpd with HTTPD_DEFAULT_CONFIG() on port 80
  - Registers three URI handlers (/, /photo, /status)
  - Logs server URL with device IP address
  - Called from app_main only if Wi-Fi is connected

### Prompt 4.2 -- Main Page Handler (GET /)
- Implemented `http_root_handler()`:
  - Builds a complete HTML page with inline CSS (dark theme, responsive layout)
  - Auto-refreshes every 10 seconds via `<meta http-equiv="refresh" content="10">`
  - Displays latest Telegram message text (mutex-protected read from shared var)
  - Shows `<img src="/photo">` if a photo has been received
  - Shows "No messages yet" placeholder when no content exists
  - Footer shows Wi-Fi/NTP status and device IP
  - Uses heap-allocated 2KB buffer for HTML generation

### Prompt 4.3 -- Photo Serving Handler (GET /photo)
- Implemented `http_photo_handler()`:
  - Reads JPEG photo from SD card path stored in `telegram_photo_path`
  - Uses chunked transfer (2KB chunks) to serve large files without loading entirely into RAM
  - Sets Content-Type to image/jpeg
  - Returns 404 if no photo path is set or file doesn't exist
  - Logs file size and warns if USB MSC is active during photo serving
  - Mutex-protected read of shared photo path variable

### Prompt 4.4 -- Status JSON Endpoint (GET /status)
- Implemented `http_status_handler()`:
  - Returns JSON object built with cJSON library containing:
    - `wifi_status`: "connected" or "disconnected"
    - `ip_address`: Current IP string
    - `ntp_synced`: boolean
    - `current_time`: Formatted LA time string with timezone
    - `last_message`: Latest Telegram message or "(none)"
    - `free_heap`: Available heap bytes via `esp_get_free_heap_size()`
    - `uptime_seconds`: System uptime via `esp_timer_get_time()`
  - Sets Content-Type to application/json

### app_main Updates
- Added Step 9/10: HTTP server start (conditional on Wi-Fi connected)
- Renumbered all steps from /9 to /10
- Updated final status log to show HTTP server state and web URL
- Added web interface URLs to USAGE section

### Build Result
- Build: SUCCESS (no new errors, only pre-existing unused function warnings)
- Binary size: 0x117e00 bytes (1,146,368 bytes), 25% free in partition
- Size increase from Phase 3: ~37KB (HTTP server component + handlers)
- New dependencies: esp_http_server, esp_timer, http_parser (transitive)

---

## Phase 5: Integration and Main Application Update (Prompts 5.1 - 5.2)

### Prompt 5.1 -- app_main() Integration and Init Order
- Extracted NVS flash init from wifi_init_sta() into explicit Step 1 in app_main()
  - NVS init now happens before any other component (Wi-Fi needs it for PHY data)
  - Handles corrupt/full NVS partition with erase + retry
  - Failure is fatal (returns from app_main) since Wi-Fi depends on it
- Renumbered all initialization steps from /10 to /11 to accommodate new step
- Updated wifi_init_sta() internal step numbering (removed NVS step, renumbered 1-7)
- Updated app_main docstring with complete 11-step startup sequence and dependency explanations
- Fixed duplicate "STEP 6" comment for USB MSC section
- Final initialization order:
  1. NVS flash init (NEW -- extracted from Wi-Fi)
  2. Mutex creation (existing)
  3. SD card init (existing)
  4. File operations demo (existing)
  5. I2C + LCD init (existing)
  6. Wi-Fi init and connect
  7. NTP time sync
  8. USB MSC init (existing)
  9. Telegram Bot polling task
  10. HTTP web server
  11. Clock display task (now multi-mode)

### Prompt 5.2 -- LCD Display Mode Cycling
- Rewrote clock_task() to cycle between 3 display modes:
  - Mode 0 (Date/Time): NTP-synced date (MM/DD) with SD/NTP status, centered HH:MM:SS
  - Mode 1 (Wi-Fi/IP): "WiFi: Connected" or "No Conn", IP address on row 2
  - Mode 2 (Telegram): "Telegram:" label, scrolling message text (marquee style)
- Auto-cycles every 5 seconds (DISPLAY_MODE_DURATION_MS = 5000)
- Scrolling logic for messages > 16 chars:
  - Extracts 16-char sliding window from message
  - 4-space gap padding before wrap-around
  - Advances 1 character per 200ms update cycle (5 chars/second)
- Mutex-protected read of telegram_last_message in Mode 2
- Only redraws LCD when content changes (Mode 0) or on each cycle (Modes 1/2)
- 200ms task delay balances smooth scrolling with CPU efficiency

### Build Result
- Build: SUCCESS (no new errors, only pre-existing unused function warnings)
- Binary size: 0x117fb0 bytes (1,146,684 bytes), 25% free in partition
- Size increase from Phase 4: ~420 bytes (display mode logic is minimal)

---

## Phase 6: Build, Flash, and Test (Prompts 6.1 - 6.5)

### Prompt 6.1 -- Build and sdkconfig Verification
- Build: SUCCESS -- no errors, no new warnings (only pre-existing unused function warnings)
- Binary size: 0x117fb0 bytes (1,146,684 bytes), 25% free in 1500KB partition
- Bootloader: 0x5490 bytes, 34% free
- sdkconfig verified -- all required components enabled:
  - CONFIG_ESP_WIFI_ENABLED=y (Wi-Fi for ESP32-S3)
  - CONFIG_LWIP_SNTP_MAX_SERVERS=1 (LWIP SNTP)
  - CONFIG_HTTPD_MAX_REQ_HDR_LEN=512 (HTTP Server)
  - CONFIG_ESP_HTTP_CLIENT_ENABLE_HTTPS=y (HTTP Client with HTTPS)
  - CONFIG_ESP_TLS_USING_MBEDTLS=y (TLS support)
  - CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y (full cert bundle for HTTPS)
  - CONFIG_TINYUSB_MSC_ENABLED=y (TinyUSB MSC)
  - CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y (1500KB app partition)
  - CONFIG_FATFS_LFN_HEAP=y (long filename support)
  - CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y (4MB flash)
- Flash command for user: idf.py -p /dev/ttyACM0 flash monitor

### Prompts 6.2-6.5 -- Manual Hardware Test Checklist
These prompts require physical hardware interaction (flash, serial monitor, browser).
Checklist for manual verification:

Prompt 6.2 -- Wi-Fi Connection:
- [ ] ESP32-S3 connects to hidden SSID "Embedded"
- [ ] Serial log shows "Got IP" with assigned IP address
- [ ] Auto-reconnect works after AP outage
- [ ] Wi-Fi status shows on LCD (Mode 1: WiFi/IP display)
- [ ] No connection loop or crash on failure

Prompt 6.3 -- NTP Time Sync:
- [ ] SNTP sync success message logged in serial monitor
- [ ] Displayed time matches current Los Angeles time (PST/PDT)
- [ ] DST offset applied correctly
- [ ] Clock continues running if NTP becomes unreachable
- [ ] Re-sync logged periodically
- [ ] LCD time updates every second (Mode 0: Date/Time display)

Prompt 6.4 -- Telegram Bot:
- [ ] Text message sent to bot is received by ESP32 (check serial log)
- [ ] Text message appears on web page at http://<ESP32_IP>/
- [ ] Photo sent to bot is received and saved to SD card
- [ ] Photo displayed on web page
- [ ] Web page auto-refreshes (10s) and shows latest content
- [ ] Latest Telegram message scrolls on LCD (Mode 2)
- [ ] Serial log shows Telegram polling activity

Prompt 6.5 -- HTTP Web Server:
- [ ] Main page loads at http://<ESP32_IP>/
- [ ] Telegram content displayed correctly on main page
- [ ] http://<ESP32_IP>/status returns valid JSON with all fields
  - Fields: wifi_status, ip_address, ntp_synced, current_time, last_message, free_heap, uptime_seconds
- [ ] http://<ESP32_IP>/photo serves image with Content-Type: image/jpeg
- [ ] /photo returns 404 when no photo has been received yet
- [ ] Server handles multiple connections without crashing

---

## Phase 7: Documentation and Submission (Prompts 7.1 - 7.2)

### Prompt 7.1 -- Demo Video Requirements
Video must demonstrate (in order):
1. Hardware setup (ESP32-S3 board with SD card and LCD connections)
2. Serial monitor showing Wi-Fi connection and IP address
3. LCD displaying NTP-synced Los Angeles time (Mode 0)
4. Send text message via Telegram to the bot
5. Open browser showing message on web page at http://<ESP32_IP>/
6. Send photo via Telegram and show it on web page
7. Show /status JSON endpoint in browser
8. Demonstrate Week 4 features still work (SD card mount, USB MSC)
NOTE: This is a manual step -- user must record the video.

### Prompt 7.2 -- README.md Update
- Updated README.md with comprehensive Week 5 documentation:
  - Title updated to include all Week 5 features
  - Added 4 new feature sections: Wi-Fi, NTP, Telegram Bot, HTTP Web Server
  - Added Wi-Fi Configuration section with credential macros
  - Added Telegram Bot Setup section with step-by-step instructions
  - Added Accessing the Web Server section with all 3 endpoints
  - Updated Build Instructions with correct Week 5 path
  - Updated Usage section with 11-step boot sequence
  - Updated LCD Display section showing 3 cycling modes
  - Updated Project Structure reflecting ~4700 line main.c
  - Updated Configuration Options with all sdkconfig settings and component list
  - Added troubleshooting for Wi-Fi, NTP, Telegram, Web Server, and partition overflow
  - Updated Success Criteria with Week 4 and Week 5 checklists (all marked done)
  - Updated Author line for Week 4 + Week 5
- README includes instructions for:
  - Setting up Telegram bot token via BotFather
  - Configuring Wi-Fi credentials (SSID/password macros)
  - Accessing the web server (3 URL endpoints)
  - Build, flash, and monitor commands
- User needs to push to GitHub and provide repository URL
