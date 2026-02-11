# =============================================================================
# LED Sequence Project - Main Component CMakeLists.txt
# =============================================================================
# This CMake file registers the main component of the LED sequence project.
# It specifies which source files to compile and which ESP-IDF components
# are required (driver for GPIO, esp_timer for timing functions).
#
# Author: UCSC Embedded Firmware Essentials Class
# Date: January 2026
# Target: ESP32-S3
# =============================================================================

# Register the main component with ESP-IDF build system
idf_component_register(
    # Source files to compile
    SRCS "main.c"
    
    # Include directories for header files
    INCLUDE_DIRS "."
    
    # Required ESP-IDF components:
    # - driver: Provides GPIO driver functions
    # - esp_timer: Provides timing utilities (though we use FreeRTOS delays)
    REQUIRES driver esp_timer
)
