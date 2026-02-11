# =============================================================================
# 7-Segment Display Counter - Main Component CMakeLists.txt
# =============================================================================
# This CMake file registers the main component of the 7-segment counter project.
# It specifies which source files to compile and which ESP-IDF components
# are required (driver for GPIO control).
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
    # - driver: Provides GPIO driver functions for controlling display segments
    REQUIRES driver
)
