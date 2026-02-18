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

idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES driver fatfs sdmmc
)
