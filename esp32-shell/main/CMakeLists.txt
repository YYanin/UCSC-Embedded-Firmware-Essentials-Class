# ESP32 Shell - Main Component CMakeLists.txt
# ============================================================================
# This CMakeLists.txt registers the main component with ESP-IDF.
# All ESP32-specific source files are compiled as part of this component.
# Note: platform_linux.c and vfs_linux.c are excluded (Linux-only testing)
# ============================================================================

# Register this as an ESP-IDF component
idf_component_register(
    SRCS 
        "main.c"
        "esp_shell.c"
        "platform_esp32.c"
        "vfs_esp32.c"
        "executor_esp32.c"
        "terminal_esp32.c"
        "parser_esp32.c"
    INCLUDE_DIRS 
        "."
    REQUIRES 
        driver          # For UART driver and GPIO
        esp_timer       # For timing functions
        spiffs          # For SPIFFS filesystem
        vfs             # For VFS abstraction
        spi_flash       # For esp_flash.h (flash info)
        esp_system      # For system info, chip info
)
