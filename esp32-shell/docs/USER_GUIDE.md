# ESP32 Shell User Guide

A comprehensive guide to using the ESP32 shell on your microcontroller.

## Table of Contents

1. [Getting Started](#getting-started)
2. [Connecting to the Shell](#connecting-to-the-shell)
3. [Basic Commands](#basic-commands)
4. [File Operations](#file-operations)
5. [Environment Variables](#environment-variables)
6. [GPIO Control](#gpio-control)
7. [System Commands](#system-commands)
8. [Keyboard Shortcuts](#keyboard-shortcuts)
9. [Limitations](#limitations)
10. [Tips and Tricks](#tips-and-tricks)

---

## Getting Started

### What is ESP32 Shell?

The ESP32 Shell is a Unix-like command-line interface that runs directly on your ESP32-S3 microcontroller. It provides an interactive way to:

- Explore and manage files on the flash filesystem
- Control GPIO pins
- Monitor system resources
- Set and use environment variables

### Prerequisites

- ESP32-S3 development board (tested on ESP32-S3-DevKitC-1)
- USB cable to connect the board to your computer
- Serial terminal software

### First Boot

When you first power on the ESP32 with the shell firmware, you'll see:

```
=====================================
   ESP32 Shell (ushell port)
=====================================
Free heap:  280000 bytes
Min heap:   275000 bytes

Type 'help' for available commands
Type 'info' for system information

esp32>
```

---

## Connecting to the Shell

### Using ESP-IDF Monitor (Recommended)

```bash
# Activate ESP-IDF environment first
. ~/esp/esp-idf/export.sh

# Open monitor
idf.py monitor
```

Press `Ctrl+]` to exit.

### Using minicom

```bash
minicom -D /dev/ttyACM0 -b 115200
```

Press `Ctrl+A` then `X` to exit.

### Using screen

```bash
screen /dev/ttyACM0 115200
```

Press `Ctrl+A` then `\` to exit.

### Using picocom

```bash
picocom -b 115200 /dev/ttyACM0
```

Press `Ctrl+A` then `Ctrl+X` to exit.

---

## Basic Commands

### Getting Help

```bash
esp32> help
```

Lists all available commands with brief descriptions.

### Echoing Text

```bash
esp32> echo Hello, World!
Hello, World!

esp32> echo "Text with spaces"
Text with spaces
```

### Command History

Use the **Up** and **Down** arrow keys to navigate through previously entered commands.

---

## File Operations

The ESP32 uses SPIFFS (SPI Flash File System) mounted at `/spiffs`.

### Viewing Current Directory

```bash
esp32> pwd
/spiffs
```

### Listing Files

```bash
esp32> ls
file1.txt
file2.txt
config.json
```

### Creating Files

```bash
esp32> touch myfile.txt
```

### Writing to Files

```bash
esp32> echo "Hello" > myfile.txt
esp32> echo "World" >> myfile.txt   # Append
```

### Reading Files

```bash
esp32> cat myfile.txt
Hello
World
```

### Deleting Files

```bash
esp32> rm myfile.txt
```

### Filesystem Information

```bash
esp32> fsinfo
Filesystem: /spiffs
Total:  1048576 bytes (1024 KB)
Used:   12288 bytes (12 KB)
Free:   1036288 bytes (1012 KB)
Usage:  1%
```

### Formatting the Filesystem

**WARNING: This erases ALL files!**

```bash
esp32> format --yes
Formatting SPIFFS partition...
Format complete.
```

---

## Environment Variables

### Setting Variables

```bash
esp32> set NAME=ESP32
esp32> set VERSION=1.0
```

### Using Variables

```bash
esp32> echo Hello, $NAME!
Hello, ESP32!

esp32> echo Version: ${VERSION}
Version: 1.0
```

### Listing All Variables

```bash
esp32> env
NAME=ESP32
VERSION=1.0
```

### Removing Variables

```bash
esp32> unset NAME
```

---

## GPIO Control

Control the ESP32's GPIO pins directly from the shell.

### Setting Pin Mode

Configure a pin as input or output:

```bash
esp32> gpio mode 2 out    # Set GPIO 2 as output
esp32> gpio mode 4 in     # Set GPIO 4 as input
```

### Writing to a Pin

Set a pin HIGH (1) or LOW (0):

```bash
esp32> gpio write 2 1     # Set GPIO 2 HIGH
esp32> gpio write 2 0     # Set GPIO 2 LOW
```

### Reading from a Pin

Read the current state of a pin:

```bash
esp32> gpio read 4
GPIO 4: 1
```

### Blinking an LED (Example)

```bash
esp32> gpio mode 2 out
esp32> gpio write 2 1
esp32> gpio write 2 0
esp32> gpio write 2 1
```

### Common GPIO Pins (ESP32-S3-DevKitC-1)

| Pin | Common Use |
|-----|------------|
| 0   | Boot button (avoid for output) |
| 2   | Often connected to onboard LED |
| 38  | RGB LED (on some boards) |

---

## System Commands

### System Information

```bash
esp32> info
ESP32 System Information
========================
Chip:         ESP32-S3
Revision:     v0.2
Cores:        2
Flash:        8 MB
IDF Version:  v5.5
Free heap:    280000 bytes
```

### Free Memory

```bash
esp32> free
Free heap memory: 280000 bytes
```

### Uptime

```bash
esp32> uptime
Up 0 days, 0:15:32
```

### Reboot

```bash
esp32> reboot
Rebooting...
```

Or use `exit` (which also reboots since there's no OS to exit to).

---

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| Enter | Execute command |
| Backspace | Delete character before cursor |
| Up Arrow | Previous command in history |
| Down Arrow | Next command in history |
| Ctrl+C | Cancel current line |
| Ctrl+D | Exit (reboot) |

---

## Limitations

The ESP32 shell has some limitations compared to the full Linux version:

### Not Available

- **Pipelines** (`cmd1 | cmd2`) - No pipe support
- **Background Jobs** (`&`, `jobs`, `fg`, `bg`) - FreeRTOS doesn't have fork()
- **External Programs** - Only built-in commands available
- **Subdirectories** - SPIFFS is a flat filesystem
- **Tab Completion** - Not implemented
- **Glob Expansion** (`*.txt`) - Not implemented

### Memory Constraints

- Command line limited to 256 characters
- History limited to 20 entries
- Environment variables limited to 32
- Maximum path length: 128 characters

### Filesystem Constraints

- SPIFFS is a flat filesystem (no subdirectories)
- Maximum file name: 64 characters
- Total storage: 1MB

---

## Tips and Tricks

### Storing Configuration

Use files to store persistent configuration:

```bash
esp32> echo "WIFI_SSID=MyNetwork" > config.txt
esp32> echo "WIFI_PASS=secret123" >> config.txt
esp32> cat config.txt
WIFI_SSID=MyNetwork
WIFI_PASS=secret123
```

### Quick System Check

After boot, run these commands to verify the system:

```bash
esp32> info
esp32> free
esp32> fsinfo
esp32> ls
```

### Monitoring Memory Usage

Run `free` periodically to check for memory leaks:

```bash
esp32> free
Free heap memory: 280000 bytes

# ... run some commands ...

esp32> free
Free heap memory: 279000 bytes   # Should be stable
```

### Creating Test Files

```bash
esp32> touch test.txt
esp32> echo "test data" > test.txt
esp32> cat test.txt
test data
esp32> rm test.txt
```

---

## Command Reference

| Command | Description | Example |
|---------|-------------|---------|
| `help` | Show all commands | `help` |
| `echo` | Print text | `echo hello` |
| `pwd` | Print working directory | `pwd` |
| `cd` | Change directory | `cd /spiffs` |
| `ls` | List files | `ls` |
| `cat` | Show file contents | `cat file.txt` |
| `touch` | Create empty file | `touch new.txt` |
| `rm` | Remove file | `rm old.txt` |
| `set` | Set variable | `set VAR=value` |
| `unset` | Remove variable | `unset VAR` |
| `env` | List variables | `env` |
| `gpio` | GPIO control | `gpio read 2` |
| `info` | System info | `info` |
| `free` | Free memory | `free` |
| `uptime` | Time since boot | `uptime` |
| `fsinfo` | Filesystem info | `fsinfo` |
| `format` | Format filesystem | `format --yes` |
| `reboot` | Restart ESP32 | `reboot` |
| `exit` | Exit (reboot) | `exit` |

---

## Getting Help

If you encounter issues:

1. Check the troubleshooting section in the main README
2. Verify your serial connection is at 115200 baud
3. Press the EN/Reset button on the board
4. Run `info` and `free` to check system status
