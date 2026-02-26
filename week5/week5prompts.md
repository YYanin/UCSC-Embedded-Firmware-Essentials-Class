# Prompts for Embedded Firmware Essentials Week 5 Assignment

## Summary

This document contains all prompts necessary to complete the Week 5 assignment for the UCSC Silicon Valley Embedded Firmware Essentials Class. The assignment involves adding networking features to the existing ESP32-S3 clock project from Week 4.

The main features to implement:
- **Wi-Fi Connectivity**: Connect to a hidden Wi-Fi network (SSID: Embedded, Password: class2026-embedded)
- **NTP Time Sync**: Automatically synchronize the clock with time.nist.gov NTP server using the America/Los_Angeles time zone
- **Telegram Bot Integration**: Receive messages or pictures from a Telegram bot (created via BotFather)
- **Web Server**: Run a simple HTTP web server on the ESP32-S3 that displays the message or picture received from the Telegram bot

### Hardware Requirements (Same as Week 4, plus Wi-Fi)

- ESP32-S3 development board (with built-in Wi-Fi)
- USB cable (data capable)
- SD card module (SPI interface)
- SD card (FAT32 formatted)
- I2C LCD display (16x2 or 20x4, HD44780-compatible with I2C backpack)
- 1k Ohm resistors (pull-up for I2C SDA and SCL lines)
- Wi-Fi network access (hidden SSID: Embedded)

### Software Requirements

- ESP-IDF v5.x with Wi-Fi, LWIP, SNTP, and HTTP server components
- Telegram Bot token (obtained from BotFather)
- Telegram Bot API access via HTTPS

---

## Phase 1: Wi-Fi Configuration and Connection

### Prompt 1.1
"Add Wi-Fi support to the existing Week 4 SD/USB/LCD project. Include the necessary headers and ESP-IDF components for Wi-Fi connectivity:
- Add `esp_wifi`, `esp_event`, `esp_netif`, `nvs_flash`, and `lwip` includes
- Update main/CMakeLists.txt to require `esp_wifi`, `esp_event`, `esp_netif`, `nvs_flash`, and `lwip` components
- Add Wi-Fi credential defines for the hidden SSID:
  - SSID: Embedded
  - Password: class2026-embedded
  - Scan method: WIFI_ALL_CHANNEL_SCAN (required for hidden networks)
- Include detailed comments explaining each Wi-Fi dependency"

### Manual Tests to Verify Completion of Prompt 1.1
- [ ] esp_wifi.h, esp_event.h, esp_netif.h, nvs_flash.h headers included
- [ ] CMakeLists.txt updated with Wi-Fi-related REQUIRES
- [ ] WIFI_SSID and WIFI_PASS defines present with correct values
- [ ] Comments note that the SSID is hidden and scan method is set accordingly
- [ ] Project still builds without errors

---

### Prompt 1.2
"Implement Wi-Fi station initialization and connection logic:
- Initialize NVS flash (required for Wi-Fi credential storage)
- Create default Wi-Fi station netif
- Initialize the Wi-Fi event loop
- Configure Wi-Fi in station mode with the hidden SSID credentials
- Set scan_method to WIFI_ALL_CHANNEL_SCAN for hidden SSID support
- Register event handlers for WIFI_EVENT_STA_START, WIFI_EVENT_STA_DISCONNECTED, IP_EVENT_STA_GOT_IP
- Implement an event group or semaphore to signal when Wi-Fi is connected and IP is obtained
- Include detailed comments explaining the Wi-Fi state machine"

### Manual Tests to Verify Completion of Prompt 1.2
- [ ] nvs_flash_init() called before Wi-Fi setup
- [ ] esp_netif_init() and esp_event_loop_create_default() called
- [ ] esp_netif_create_default_wifi_sta() called
- [ ] Wi-Fi configured in WIFI_MODE_STA with correct SSID and password
- [ ] scan_method set to WIFI_ALL_CHANNEL_SCAN
- [ ] Event handlers registered for STA_START, STA_DISCONNECTED, GOT_IP
- [ ] Event group or semaphore blocks until IP is assigned
- [ ] Comments explain each step of the Wi-Fi initialization

---

### Prompt 1.3
"Add Wi-Fi reconnection logic and status reporting:
- On WIFI_EVENT_STA_DISCONNECTED, attempt to reconnect automatically
- Implement a retry counter with a maximum retry limit (e.g., 10 retries)
- Log connection status changes (connecting, connected, disconnected, IP address)
- Display Wi-Fi status on the LCD (e.g., 'WiFi: OK' or 'WiFi: ---')
- Include detailed comments explaining the reconnection strategy"

### Manual Tests to Verify Completion of Prompt 1.3
- [ ] Auto-reconnect triggers on disconnect event
- [ ] Retry counter limits reconnection attempts
- [ ] Connection status logged to serial console
- [ ] IP address logged when obtained
- [ ] Wi-Fi status displayed on LCD
- [ ] Comments explain reconnection behavior

---

## Phase 2: NTP Time Synchronization

### Prompt 2.1
"Configure SNTP (Simple Network Time Protocol) to synchronize the clock with time.nist.gov:
- Include esp_sntp.h (or esp_netif_sntp.h for newer ESP-IDF)
- Configure SNTP to use the server: time.nist.gov
- Set the operating mode to SNTP_OPMODE_POLL
- Set the sync interval (e.g., every 15 minutes or 3600 seconds)
- Set the time zone to America/Los_Angeles (Pacific Time) using setenv() and tzset()
  - TZ string: 'PST8PDT,M3.2.0,M11.1.0' (handles PST/PDT daylight saving transitions)
- Include detailed comments explaining the SNTP configuration and time zone string"

### Manual Tests to Verify Completion of Prompt 2.1
- [ ] esp_sntp.h or esp_netif_sntp.h included
- [ ] SNTP server set to time.nist.gov
- [ ] SNTP operating mode set to SNTP_OPMODE_POLL
- [ ] Sync interval configured
- [ ] Time zone set to PST8PDT with DST rules
- [ ] setenv("TZ", ...) and tzset() called
- [ ] Comments explain the TZ string format and DST rules

---

### Prompt 2.2
"Implement the SNTP initialization function and time sync callback:
- Create a function wifi_and_time_init() or sntp_init_time() that:
  1. Waits for Wi-Fi to be connected (using the event group from Phase 1)
  2. Initializes SNTP
  3. Waits for time to be synchronized (poll until time is set, or use a notification callback)
- Implement a time sync notification callback that logs when time is successfully updated
- Include detailed comments explaining the sync flow"

### Manual Tests to Verify Completion of Prompt 2.2
- [ ] Function waits for Wi-Fi connection before SNTP init
- [ ] SNTP is initialized after Wi-Fi connects
- [ ] Time sync callback logs successful synchronization
- [ ] Function waits until time is actually set before returning
- [ ] Comments explain the synchronization flow

---

### Prompt 2.3
"Replace the software RTC from Week 4 with NTP-synced system time:
- Modify the LCD clock display task to use time() and localtime() instead of the manual RTC struct
- Format the time using strftime() for consistent date/time formatting
- Display the Los Angeles local time on the LCD:
  - Line 1: Date in 'YYYY-MM-DD' or 'MM/DD/YYYY' format
  - Line 2: Time in 'HH:MM:SS' format (24-hour or 12-hour with AM/PM)
- Log the current time to the serial console periodically for debugging
- Include detailed comments explaining the switch from software RTC to NTP"

### Manual Tests to Verify Completion of Prompt 2.3
- [ ] LCD clock task uses time() and localtime() instead of manual RTC
- [ ] strftime() used for consistent formatting
- [ ] Date displayed on LCD line 1
- [ ] Time displayed on LCD line 2 with seconds updating
- [ ] Time shown is correct for Los Angeles time zone
- [ ] DST offset applied correctly if applicable
- [ ] Time logged to serial console
- [ ] Comments explain the NTP-based time display

---

### Prompt 2.4
"Add NTP sync status indicators:
- Display an indicator on the LCD or serial log when NTP sync is in progress
- Show 'NTP: OK' or 'NTP: ---' on the LCD alongside Wi-Fi status
- If NTP sync fails, fall back to displaying '--:--:--' until sync succeeds
- Re-sync periodically (SNTP handles this automatically, but verify it works)
- Include detailed comments"

### Manual Tests to Verify Completion of Prompt 2.4
- [ ] NTP sync status visible on LCD or serial log
- [ ] Fallback display shown when time not yet synced
- [ ] Periodic re-sync occurs automatically
- [ ] Status updates when sync succeeds or fails
- [ ] Comments explain the status indicators

---

## Phase 3: Telegram Bot Setup

### Prompt 3.1
"Set up a Telegram bot using BotFather:
- Open Telegram and search for @BotFather
- Send /newbot command and follow the instructions:
  1. Provide a name for the bot (e.g., 'ESP32 Display Bot')
  2. Provide a username for the bot (must end in 'bot', e.g., 'esp32_display_bot')
- Save the HTTP API token provided by BotFather
- Note the bot's username for later use
- Document the token storage approach (define in code or store in sdkconfig)

This is a manual step - no code to write."

### Manual Tests to Verify Completion of Prompt 3.1
- [ ] Telegram bot created via BotFather
- [ ] Bot API token obtained and saved securely
- [ ] Bot username noted
- [ ] Token storage approach decided (code define or Kconfig)

---

### Prompt 3.2
"Add Telegram Bot API token and configuration to the project:
- Define the Telegram Bot API token in the code (or use Kconfig for better security)
- Define the Telegram API base URL: https://api.telegram.org/bot<TOKEN>
- Add the esp_http_client and esp_tls components to CMakeLists.txt (needed for HTTPS requests)
- Add the esp_crt_bundle component for TLS certificate verification (Telegram uses HTTPS)
- Include detailed comments about the Telegram Bot API"

### Manual Tests to Verify Completion of Prompt 3.2
- [ ] Telegram Bot token defined in code or Kconfig
- [ ] Telegram API base URL constructed correctly
- [ ] esp_http_client and esp_tls added to CMakeLists.txt REQUIRES
- [ ] esp_crt_bundle included for TLS certificate bundle
- [ ] Project builds without errors
- [ ] Comments explain the Telegram API configuration

---

### Prompt 3.3
"Implement a function to poll for new Telegram messages using the getUpdates API:
- Use esp_http_client to make HTTPS GET requests to:
  https://api.telegram.org/bot<TOKEN>/getUpdates?offset=<OFFSET>&timeout=10
- Parse the JSON response to extract:
  - update_id (to track the offset for the next poll)
  - message.text (text messages)
  - message.photo (photo array, if present - get the largest file_id)
- Use cJSON library (included in ESP-IDF) to parse JSON responses
- Implement long polling with a timeout (e.g., 10 seconds)
- Include detailed comments explaining the Telegram Bot API polling mechanism"

### Manual Tests to Verify Completion of Prompt 3.3
- [ ] HTTPS GET request made to getUpdates endpoint
- [ ] JSON response parsed using cJSON
- [ ] update_id extracted and offset incremented
- [ ] Text messages extracted from message.text
- [ ] Photo file_id extracted from message.photo array
- [ ] Long polling timeout configured
- [ ] Error handling for HTTP and JSON failures
- [ ] Comments explain the polling and parsing logic

---

### Prompt 3.4
"Implement a function to download photos from Telegram:
- First call getFile API with the file_id:
  https://api.telegram.org/bot<TOKEN>/getFile?file_id=<FILE_ID>
- Parse the response to get the file_path
- Download the file from:
  https://api.telegram.org/file/bot<TOKEN>/<FILE_PATH>
- Save the downloaded image to the SD card (e.g., /sdcard/telegram_photo.jpg)
- Implement appropriate buffer management for downloading binary data
- Include detailed comments explaining the two-step download process"

### Manual Tests to Verify Completion of Prompt 3.4
- [ ] getFile API called with correct file_id
- [ ] file_path extracted from JSON response
- [ ] Image downloaded from the file download URL
- [ ] Image saved to SD card successfully
- [ ] Buffer management handles large files (chunked download)
- [ ] Error handling for download failures
- [ ] Comments explain the two-step Telegram file download

---

### Prompt 3.5
"Create a FreeRTOS task for continuous Telegram polling:
- Create a task that runs in a loop polling for Telegram updates
- Store the latest received text message in a global variable (protected by a mutex)
- Store the path to the latest received photo on the SD card
- Set a flag when new content is received (to trigger web server update)
- Handle network errors gracefully (retry after delay)
- Include detailed comments explaining the polling task"

### Manual Tests to Verify Completion of Prompt 3.5
- [ ] FreeRTOS task created for Telegram polling
- [ ] Task polls getUpdates in a loop
- [ ] Latest text message stored in a shared variable
- [ ] Latest photo path stored when photo received
- [ ] Mutex protects shared variables
- [ ] New content flag set on message receipt
- [ ] Network errors handled with retry logic
- [ ] Comments explain the task architecture

---

## Phase 4: HTTP Web Server

### Prompt 4.1
"Set up a simple HTTP web server on the ESP32-S3:
- Include esp_http_server.h
- Add esp_http_server to CMakeLists.txt REQUIRES
- Create a function to start the HTTP server on port 80
- Register a root URI handler for GET requests to '/'
- The handler should return a basic HTML page
- Include detailed comments explaining the HTTP server setup"

### Manual Tests to Verify Completion of Prompt 4.1
- [ ] esp_http_server.h included
- [ ] esp_http_server added to CMakeLists.txt
- [ ] HTTP server starts on port 80
- [ ] Root '/' URI handler registered for GET
- [ ] Basic HTML page returned on GET /
- [ ] Server starts without errors
- [ ] Comments explain the server configuration

---

### Prompt 4.2
"Implement the main web page handler to display Telegram content:
- The GET '/' handler should return an HTML page that shows:
  - The latest text message received from Telegram (if any)
  - An <img> tag pointing to '/photo' if a photo was received
  - A 'No messages yet' placeholder if nothing has been received
  - Auto-refresh the page every 10 seconds using <meta http-equiv='refresh'>
- Use the shared variables (protected by mutex) from the Telegram polling task
- Include basic CSS styling for readability
- Include detailed comments explaining the HTML generation"

### Manual Tests to Verify Completion of Prompt 4.2
- [ ] HTML page displays latest text message
- [ ] HTML page includes <img> tag for photo if available
- [ ] 'No messages yet' shown when no content received
- [ ] Page auto-refreshes every 10 seconds
- [ ] Mutex used when reading shared variables
- [ ] Basic CSS styling applied
- [ ] Comments explain the dynamic HTML generation

---

### Prompt 4.3
"Implement a URI handler to serve photos from the SD card:
- Register a GET handler for '/photo' URI
- Read the image file from SD card (e.g., /sdcard/telegram_photo.jpg)
- Set the correct Content-Type header (image/jpeg)
- Send the image data in the HTTP response
- Handle file-not-found gracefully (return 404)
- Use chunked sending for large files if needed
- Include detailed comments"

### Manual Tests to Verify Completion of Prompt 4.3
- [ ] GET '/photo' URI handler registered
- [ ] Image file read from SD card
- [ ] Content-Type set to image/jpeg
- [ ] Image data sent in response body
- [ ] 404 returned if no photo exists
- [ ] Large files handled with chunked transfer
- [ ] Comments explain the file serving logic

---

### Prompt 4.4
"Add a '/status' endpoint that returns device information as JSON:
- Register a GET handler for '/status'
- Return a JSON response with:
  - Wi-Fi status (connected/disconnected)
  - IP address
  - NTP sync status
  - Current time (Los Angeles)
  - Last Telegram message timestamp
  - Free heap memory
- Use cJSON to build the response
- Include detailed comments"

### Manual Tests to Verify Completion of Prompt 4.4
- [ ] GET '/status' URI handler registered
- [ ] JSON response includes Wi-Fi status
- [ ] JSON response includes IP address
- [ ] JSON response includes NTP sync status
- [ ] JSON response includes current time
- [ ] JSON response includes heap memory info
- [ ] Content-Type set to application/json
- [ ] Comments explain the status endpoint

---

## Phase 5: Integration and Main Application Update

### Prompt 5.1
"Update app_main() to integrate the new Week 5 features with existing Week 4 functionality:
- Initialization order:
  1. NVS flash init
  2. SD card init (existing)
  3. I2C and LCD init (existing)
  4. Wi-Fi init and connect
  5. NTP time sync (after Wi-Fi connected)
  6. USB MSC init (existing)
  7. Start Telegram polling task
  8. Start HTTP web server
  9. Start LCD clock display task (now using NTP time)
- Error handling at each step
- Include detailed comments explaining the startup sequence and dependencies"

### Manual Tests to Verify Completion of Prompt 5.1
- [ ] NVS flash initialized first
- [ ] Wi-Fi initialization added after LCD init
- [ ] NTP sync happens after Wi-Fi connects
- [ ] Telegram polling task started after Wi-Fi
- [ ] HTTP server started after Wi-Fi
- [ ] LCD clock uses NTP time instead of software RTC
- [ ] Existing SD card and USB MSC features still work
- [ ] Error handling present at each initialization step
- [ ] Comments explain initialization order and dependencies

---

### Prompt 5.2
"Update the LCD display to show the new status information:
- Cycle between different display modes or use available LCD lines:
  - Mode 1: Date and Time (NTP-synced, Los Angeles)
  - Mode 2: Wi-Fi status and IP address
  - Mode 3: Last Telegram message (scrolling if too long)
- Cycle modes automatically every few seconds, or use a button to switch
- Include detailed comments explaining the display modes"

### Manual Tests to Verify Completion of Prompt 5.2
- [ ] LCD shows NTP-synced date and time
- [ ] LCD shows Wi-Fi status and IP address
- [ ] LCD shows Telegram message content
- [ ] Display modes cycle or are switchable
- [ ] No display flicker during mode changes
- [ ] Comments explain the display mode logic

---

## Phase 6: Build, Flash, and Test

### Prompt 6.1
"Build and flash the Week 5 project to the ESP32-S3:
```
get_idf
cd ~/esp/projects/week5/sd_usb_lcd
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```
(Replace /dev/ttyACM0 with your actual port if different)

Ensure sdkconfig.defaults or sdkconfig has the following enabled:
- Wi-Fi (should be enabled by default for ESP32-S3)
- LWIP SNTP
- HTTP Server
- HTTP Client
- TLS with certificate bundle
- TinyUSB for MSC"

### Manual Tests to Verify Completion of Prompt 6.1
- [ ] Build completes without errors or warnings
- [ ] Flash completes successfully
- [ ] Serial monitor shows startup messages
- [ ] No crash or panic messages on boot
- [ ] Required components enabled in sdkconfig

---

### Prompt 6.2
"Verify Wi-Fi connection:
1. Confirm ESP32-S3 connects to the hidden 'Embedded' SSID
2. Check serial log for 'Got IP' message with assigned IP address
3. Verify auto-reconnect works by briefly turning off and on the AP
4. Confirm Wi-Fi status shows on LCD"

### Manual Tests to Verify Completion of Prompt 6.2
- [ ] ESP32-S3 connects to hidden SSID 'Embedded'
- [ ] IP address logged in serial monitor
- [ ] Auto-reconnect works after AP outage
- [ ] Wi-Fi status visible on LCD
- [ ] No connection loop or crash on failure

---

### Prompt 6.3
"Verify NTP time synchronization:
1. Check serial log for SNTP sync success message
2. Verify displayed time matches current Los Angeles time
3. Check DST offset is applied correctly (if applicable)
4. Verify time persists after NTP server becomes unreachable (system clock continues)
5. Confirm periodic re-sync occurs"

### Manual Tests to Verify Completion of Prompt 6.3
- [ ] SNTP sync success logged
- [ ] Displayed time matches Los Angeles current time
- [ ] DST handled correctly
- [ ] Clock continues running if NTP becomes unreachable
- [ ] Re-sync logged periodically
- [ ] LCD time updates every second

---

### Prompt 6.4
"Verify Telegram bot integration:
1. Open Telegram and send a text message to the bot
2. Check serial log for received message
3. Open web browser and navigate to ESP32 IP address
4. Confirm the text message is displayed on the web page
5. Send a photo to the bot via Telegram
6. Refresh the web page and confirm the photo is displayed
7. Send another message and confirm the web page updates"

### Manual Tests to Verify Completion of Prompt 6.4
- [ ] Text message sent to bot is received by ESP32
- [ ] Text message appears on the web page at ESP32 IP
- [ ] Photo sent to bot is received and saved to SD card
- [ ] Photo displayed on the web page
- [ ] Web page auto-refreshes and shows latest content
- [ ] Multiple messages handled correctly (latest shown)
- [ ] Serial log shows Telegram polling activity

---

### Prompt 6.5
"Verify the HTTP web server:
1. Open a web browser on a device connected to the same network
2. Navigate to http://<ESP32_IP_ADDRESS>/
3. Confirm the main page loads with Telegram content
4. Navigate to http://<ESP32_IP_ADDRESS>/status
5. Confirm JSON status response with all fields
6. Navigate to http://<ESP32_IP_ADDRESS>/photo
7. Confirm photo is served (or 404 if no photo received yet)"

### Manual Tests to Verify Completion of Prompt 6.5
- [ ] Main page loads at root URL
- [ ] Telegram content displayed correctly
- [ ] /status returns valid JSON with all expected fields
- [ ] /photo serves the image with correct Content-Type
- [ ] 404 returned for /photo when no photo exists
- [ ] Server handles multiple simultaneous connections
- [ ] No memory leaks during extended operation

---

## Phase 7: Documentation and Submission

### Prompt 7.1
"Record a demo video showing all Week 5 features:
1. Show the ESP32-S3 board with connections
2. Show serial monitor with Wi-Fi connection and IP address
3. Show LCD displaying NTP-synced Los Angeles time
4. Open Telegram and send a text message to the bot
5. Open browser and show the message on the web page
6. Send a photo via Telegram and show it on the web page
7. Show the /status JSON endpoint in the browser
8. Demonstrate that existing Week 4 features still work (SD card, USB MSC)
Save the video file for submission."

### Manual Tests to Verify Completion of Prompt 7.1
- [ ] Video shows hardware setup
- [ ] Wi-Fi connection demonstrated
- [ ] NTP-synced clock visible on LCD
- [ ] Telegram text message sent and received
- [ ] Web page displays Telegram text message
- [ ] Telegram photo sent and displayed on web page
- [ ] /status JSON endpoint shown
- [ ] Week 4 features still functional
- [ ] Video quality sufficient for grading

---

### Prompt 7.2
"Update GitHub repository with Week 5 code:
1. Add all new/modified files to the repository:
   - Updated main/main.c with Wi-Fi, NTP, Telegram, and web server code
   - Updated main/CMakeLists.txt with new component dependencies
   - Updated sdkconfig.defaults if modified
   - README.md updated with Week 5 features and setup instructions
2. Include instructions for:
   - Setting up the Telegram bot token
   - Configuring Wi-Fi credentials
   - Accessing the web server
3. Push to GitHub and provide the repository URL"

### Manual Tests to Verify Completion of Prompt 7.2
- [ ] Repository contains all updated source files
- [ ] All code has detailed comments
- [ ] README.md updated with Week 5 instructions
- [ ] Telegram bot setup instructions included
- [ ] Wi-Fi configuration documented
- [ ] Web server access instructions included
- [ ] Build instructions are included
- [ ] Repository URL documented

---

## Success Criteria

### Wi-Fi Connectivity
- [ ] ESP32-S3 connects to hidden SSID 'Embedded' with password 'class2026-embedded'
- [ ] Auto-reconnect on disconnection
- [ ] IP address obtained and logged
- [ ] Wi-Fi status displayed on LCD

### NTP Time Synchronization
- [ ] Time synced from time.nist.gov
- [ ] Time zone set to America/Los_Angeles (PST/PDT)
- [ ] DST transitions handled correctly
- [ ] LCD displays accurate local time
- [ ] Periodic re-sync occurs

### Telegram Bot Integration
- [ ] Bot created via BotFather with valid API token
- [ ] ESP32 polls for messages using getUpdates
- [ ] Text messages received and stored
- [ ] Photos downloaded and saved to SD card
- [ ] Polling runs continuously in a FreeRTOS task

### HTTP Web Server
- [ ] Web server runs on port 80
- [ ] Main page displays latest Telegram text message
- [ ] Main page displays latest Telegram photo
- [ ] /photo endpoint serves image from SD card
- [ ] /status endpoint returns JSON with device info
- [ ] Page auto-refreshes

### Integration with Week 4
- [ ] SD card still mounts and operates correctly
- [ ] USB MSC still works
- [ ] LCD clock now uses NTP time instead of software RTC
- [ ] All tasks run concurrently without crashes or memory issues

### Demo Video and GitHub
- [ ] Video demonstrates all Week 5 features
- [ ] Code pushed to GitHub with detailed comments
- [ ] README documents setup and usage

---

## Additional Notes

1. **Hidden SSID**: The Wi-Fi network uses a hidden SSID. Set `scan_method` to `WIFI_ALL_CHANNEL_SCAN` and explicitly set the SSID in the Wi-Fi config. The ESP32 will probe for the hidden network.

2. **NTP Time Zone String**: The TZ string `PST8PDT,M3.2.0,M11.1.0` means:
   - PST = Pacific Standard Time, 8 hours behind UTC
   - PDT = Pacific Daylight Time
   - M3.2.0 = DST starts 2nd Sunday of March
   - M11.1.0 = DST ends 1st Sunday of November

3. **Telegram Bot API**: The Bot API uses HTTPS. The ESP32 needs the TLS certificate bundle (`esp_crt_bundle`) to verify Telegram's SSL certificate. Use long polling with a timeout to reduce API calls.

4. **Telegram getUpdates Offset**: After processing an update, increment the offset to `update_id + 1` to acknowledge it. This prevents receiving the same update again.

5. **Telegram Photo Sizes**: The `message.photo` field is an array of PhotoSize objects sorted by size. Use the last element (largest) for best quality, or the first element (smallest) to save bandwidth and storage.

6. **HTTP Server Memory**: The ESP-IDF HTTP server uses heap memory. Monitor free heap with `esp_get_free_heap_size()` to detect leaks during extended operation.

7. **Concurrent SD Card Access**: Be mindful that Telegram photo downloads (writing to SD), USB MSC, and the web server (reading from SD) may all access the SD card. Use the existing mutex from Week 4 to coordinate access.

8. **Web Server and Wi-Fi**: The web server is only accessible from devices on the same Wi-Fi network. Log the ESP32's IP address to the serial console and LCD so users know where to connect.

9. **HTTPS Client for Telegram**: Use `esp_http_client` with TLS enabled. Set `crt_bundle_attach` to `esp_crt_bundle_attach` for automatic certificate verification.

10. **FreeRTOS Task Stack Sizes**: The Telegram polling task and HTTP server tasks may need larger stack sizes (e.g., 8192 or 16384 bytes) due to TLS and JSON parsing memory requirements.

11. **cJSON Library**: ESP-IDF includes cJSON. Use `cJSON_Parse()`, `cJSON_GetObjectItem()`, etc. to parse Telegram API responses. Always call `cJSON_Delete()` to free memory after parsing.

12. **Exit Serial Monitor**: Use Ctrl+] to exit the idf.py monitor.
