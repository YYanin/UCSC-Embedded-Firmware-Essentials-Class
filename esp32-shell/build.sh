#!/bin/bash
# ESP32 Shell - Build Helper Script
# ============================================================================
# Convenience wrapper for common idf.py commands
# Usage: ./build.sh [build|flash|monitor|all|clean|menuconfig|size]
# ============================================================================

# Activate ESP-IDF if not already active
if [ -z "$IDF_PATH" ]; then
    if [ -f "$HOME/esp/esp-idf/export.sh" ]; then
        echo "Activating ESP-IDF environment..."
        . "$HOME/esp/esp-idf/export.sh"
    else
        echo "Error: ESP-IDF not found. Please install ESP-IDF first."
        echo "See README.md for installation instructions."
        exit 1
    fi
fi

# Default action
ACTION="${1:-build}"

case "$ACTION" in
    build)
        echo "Building project..."
        idf.py build
        ;;
    flash)
        echo "Flashing to device..."
        idf.py flash
        ;;
    monitor)
        echo "Opening serial monitor (Ctrl+] to exit)..."
        idf.py monitor
        ;;
    all|fm)
        echo "Flashing and monitoring..."
        idf.py flash monitor
        ;;
    clean)
        echo "Cleaning build..."
        idf.py fullclean
        ;;
    menuconfig)
        echo "Opening configuration menu..."
        idf.py menuconfig
        ;;
    size)
        echo "Showing size info..."
        idf.py size
        idf.py size-components
        ;;
    help|--help|-h)
        echo "Usage: ./build.sh [command]"
        echo ""
        echo "Commands:"
        echo "  build      - Build the project (default)"
        echo "  flash      - Flash to device"
        echo "  monitor    - Open serial monitor"
        echo "  all, fm    - Flash and monitor"
        echo "  clean      - Clean build directory"
        echo "  menuconfig - Open ESP-IDF configuration"
        echo "  size       - Show binary size breakdown"
        ;;
    *)
        echo "Unknown command: $ACTION"
        echo "Run './build.sh help' for usage"
        exit 1
        ;;
esac
