# ESP32-S3 Shell
# ============================================================================
# Interactive command-line shell for ESP32-S3 microcontrollers.
# Provides a Unix-like interface over USB serial console.
# ============================================================================

## Hardware Requirements

### Tested Board
- **Board**: ESP32-S3-DevKitC-1
- **Chip**: ESP32-S3 (QFN56, revision v0.2)
- **Features**: WiFi, BLE, Embedded PSRAM 8MB (AP_3v3)
- **Crystal**: 40MHz
- **Flash**: 8MB

### Connections
- USB-C or Micro-USB cable (board dependent)
- The board appears as `/dev/ttyACM0` on Linux

## Software Requirements

- **ESP-IDF**: v5.3.2 or later (native installation)
- **Python**: 3.8+
- **CMake**: 3.16+
- **Ninja**: Build system

## Quick Start

### 1. Activate ESP-IDF Environment
```bash
# Must run in each new terminal before using idf.py
. $HOME/esp/esp-idf/export.sh

# Or use the alias (if configured in ~/.bashrc):
get_idf
```

### 2. Build the Project
```bash
cd esp32-shell

# Set target chip (only needed once per project)
idf.py set-target esp32s3

# Build only
idf.py build

# Build and flash to connected ESP32-S3
idf.py flash

# Build, flash, and open monitor in one command
idf.py flash monitor
```

### 3. Connect to Serial Console
```bash
# Open serial monitor (uses configured baud rate from sdkconfig)
idf.py monitor

# Alternative: Use any serial terminal at 115200 baud
minicom -D /dev/ttyACM0 -b 115200
screen /dev/ttyACM0 115200
picocom -b 115200 /dev/ttyACM0
```

### 4. Exit Serial Monitor
- **idf.py monitor**: Press `Ctrl+]`
- **Monitor help**: Press `Ctrl+T` then `Ctrl+H`
- **minicom**: Press `Ctrl+A` then `X`
- **screen**: Press `Ctrl+A` then `\`
- **picocom**: Press `Ctrl+A` then `Ctrl+X`

## ESP-IDF Command Reference

```bash
# Environment (run in each new terminal)
. $HOME/esp/esp-idf/export.sh    # Activate ESP-IDF environment

# Build commands
idf.py set-target esp32s3        # Set target chip (once per project)
idf.py menuconfig                # Configure project options (interactive)
idf.py build                     # Build project
idf.py flash                     # Flash to device
idf.py monitor                   # Open serial monitor
idf.py flash monitor             # Flash and monitor combined

# Cleaning
idf.py fullclean                 # Remove all build artifacts
rm -rf build/                    # Alternative clean
rm sdkconfig                     # Reset configuration to defaults

# Useful options
idf.py -p /dev/ttyACM0 flash     # Specify port explicitly
idf.py -b 921600 flash           # Faster flash baud rate
idf.py size                      # Show binary size breakdown
idf.py size-components           # Show size by component
```

## Project Structure

```
esp32-shell/
|-- CMakeLists.txt          # ESP-IDF project CMake configuration
|-- partitions.csv          # Custom partition table with SPIFFS
|-- sdkconfig.defaults      # Default ESP-IDF settings
|-- build.sh                # Convenience build helper script
|-- .gitignore              # Git ignore for build artifacts
|-- main/                   # Main component (ESP-IDF convention)
    |-- CMakeLists.txt      # Component build config
    |-- main.c              # Application entry point (app_main)
    |-- esp_shell.c         # Shell implementation
    |-- esp_shell.h         # Shell interface header
    |-- platform.h          # Platform abstraction header
    |-- platform_esp32.c    # ESP32 platform implementation
    |-- platform_linux.c    # Linux platform (for testing)
    |-- parser_esp32.c/h    # Command parser
    |-- executor_esp32.c/h  # Command executor
    |-- terminal_esp32.c/h  # Terminal handling
    |-- vfs_esp32.c         # Virtual filesystem
    |-- shell_config.h      # Configuration options
```

## Available Commands

### Shell Control
- `help` - Show available commands
- `exit` - Reboot the ESP32 (no OS to exit to)
- `reboot` - Reboot the ESP32

### System Information
- `info` - Show chip info, memory, IDF version
- `free` - Show free heap memory
- `uptime` - Show time since boot

### File Operations (SPIFFS)
- `pwd` - Print working directory
- `cd <dir>` - Change directory
- `ls [dir]` - List directory contents
- `cat <file>` - Display file contents
- `cat >file` - Write lines to file (overwrite)
- `cat >>file` - Append lines to file
- `touch <file>` - Create empty file
- `rm <file>` - Remove file
- `echo <text>` - Print text
- `fsinfo` - Show filesystem usage (total/used/free)
- `format --yes` - Format SPIFFS filesystem (WARNING: erases all files)

### Environment Variables
- `set VAR=value` - Set environment variable
- `unset VAR` - Remove environment variable
- `env` - List all environment variables
- `$VAR` or `${VAR}` - Variable expansion in commands

### GPIO Control
- `gpio read <pin>` - Read GPIO pin state (0 or 1)
- `gpio write <pin> <0|1>` - Set GPIO pin output level
- `gpio mode <pin> <in|out>` - Configure GPIO direction

## Troubleshooting

### Wrong chip type error during build/flash
If you see "This chip is ESP32-S3, not ESP32. Wrong --chip argument?":
- Ensure target is set correctly: `idf.py set-target esp32s3`
- Clean build: `idf.py fullclean`
- Rebuild: `idf.py build`

### UART driver errors
If you see "uart_read_bytes: uart driver error":
- The UART driver may not be installed properly
- Check `platform_esp32.c` calls `uart_driver_install()`

### Garbled serial output
- Ensure baud rate matches sdkconfig (default 115200)
- Use: `idf.py monitor` (auto-detects baud rate)
- Press the EN/Reset button on the board to see boot messages

### USB device not detected
```bash
# Check if device is connected
dmesg | tail -20

# Add user to dialout group for serial port access
sudo usermod -a -G dialout $USER
# Log out and back in for group change to take effect
```

### Build fails with missing IDF_PATH
Ensure ESP-IDF environment is activated:
```bash
. $HOME/esp/esp-idf/export.sh
# Verify with:
echo $IDF_PATH
idf.py --version
```

## Memory Considerations

ESP32-S3 has ~320KB usable RAM. The shell is designed to be memory-efficient:
- Command line buffer: 256 bytes
- History: 10 entries max
- File operations use small buffers

Use `free` command to monitor heap usage.

## SPIFFS Filesystem

The partition table allocates 1MB for SPIFFS at `/spiffs`.
On first boot, SPIFFS is automatically formatted if needed.

To upload files to SPIFFS using ESP-IDF:
```bash
# Create a 'spiffs_data' folder with files to upload
mkdir -p spiffs_data
echo "Hello ESP32" > spiffs_data/hello.txt

# Build SPIFFS image and flash it
# (requires spiffsgen.py from ESP-IDF components)
python $IDF_PATH/components/spiffs/spiffsgen.py 0x100000 spiffs_data spiffs.bin
esptool.py --chip esp32s3 write_flash 0x110000 spiffs.bin
```

## Platform Abstraction

The platform abstraction layer (platform.h) allows the same code to compile for:
- ESP32 (using FreeRTOS and ESP-IDF)
- Linux (for desktop testing)

## License

MIT License
