# CMakeLists.txt for the main component
#
# This file registers the main component with the ESP-IDF build system.
# The main component contains the application entry point (app_main) and
# all the application-specific code.
#
# Components used:
# - driver: GPIO, I2C, SPI drivers
# - fatfs: FAT file system support
# - sdmmc: SD/MMC card driver
# - esp_tinyusb: USB device stack for Mass Storage Class (added later)
# - esp_wifi: Wi-Fi driver for station mode (Week 5 - hidden SSID connection)
# - esp_event: Default event loop for Wi-Fi and IP events
# - esp_netif: Network interface abstraction layer (TCP/IP stack binding)
# - nvs_flash: Non-volatile storage for Wi-Fi credential caching
# - esp_http_client: HTTP client for Telegram Bot API HTTPS requests (Week 5 - Phase 3)
# - esp-tls: TLS/SSL support for secure HTTPS connections (Week 5 - Phase 3)
# - json: cJSON library for parsing Telegram API JSON responses (Week 5 - Phase 3)
# - esp_http_server: HTTP server for web UI and photo serving (Week 5 - Phase 4)
# - esp_timer: High-resolution timer for uptime tracking (Week 5 - Phase 4)

idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES driver fatfs sdmmc esp_wifi esp_event esp_netif nvs_flash esp_http_client esp-tls json esp_http_server esp_timer
)
