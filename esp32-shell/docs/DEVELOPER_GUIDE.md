# ESP32 Shell Developer Guide

A comprehensive guide for developers who want to understand, modify, or extend the ESP32 shell.

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Source File Structure](#source-file-structure)
3. [Adding New Commands](#adding-new-commands)
4. [Memory Considerations](#memory-considerations)
5. [Build System](#build-system)
6. [Testing](#testing)
7. [Debugging](#debugging)
8. [Porting to Other ESP32 Variants](#porting-to-other-esp32-variants)

---

## Architecture Overview

### High-Level Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Serial Console                        │
│                  (UART/USB @ 115200)                     │
└─────────────────────────┬───────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────┐
│                   Terminal Layer                         │
│          (terminal_esp32.c - line editing)               │
└─────────────────────────┬───────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────┐
│                    Parser Layer                          │
│      (parser_esp32.c - tokenization, variables)          │
└─────────────────────────┬───────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────┐
│                   Executor Layer                         │
│   (esp_shell.c - command dispatch, redirection)          │
└─────────────────────────┬───────────────────────────────┘
                          │
          ┌───────────────┼───────────────┐
          │               │               │
┌─────────▼─────┐ ┌───────▼───────┐ ┌─────▼─────┐
│   Built-in    │ │  VFS Layer    │ │  Platform │
│   Commands    │ │ (vfs_esp32.c) │ │   HAL     │
│ (in esp_shell)│ │   SPIFFS      │ │(platform_)│
└───────────────┘ └───────────────┘ └───────────┘
```

### Key Components

| Component | File | Purpose |
|-----------|------|---------|
| Main | `main.c` | Entry point, initialization |
| Shell | `esp_shell.c` | Command loop, built-in commands |
| Parser | `parser_esp32.c` | Tokenization, variable expansion |
| Terminal | `terminal_esp32.c` | Line editing, key handling |
| Executor | `executor_esp32.c` | Redirection handling |
| VFS | `vfs_esp32.c` | Filesystem abstraction |
| Platform | `platform_esp32.c` | Hardware abstraction |
| Config | `shell_config.h` | Centralized configuration |

### Execution Flow

1. `app_main()` initializes platform and calls `esp_shell_init()`
2. `esp_shell_run()` enters the main loop
3. Loop: read line → parse → execute → repeat
4. Commands are looked up in `builtin_commands[]` table
5. Unknown commands print error message

---

## Source File Structure

```
esp32-shell/
├── src/
│   ├── main.c                 # Entry point (app_main)
│   ├── esp_shell.c            # Main shell and all commands
│   ├── esp_shell.h            # Shell interface
│   ├── shell_config.h         # Configuration constants
│   ├── parser_esp32.c         # Command parser
│   ├── parser_esp32.h         # Parser interface
│   ├── terminal_esp32.c       # Terminal I/O
│   ├── terminal_esp32.h       # Terminal interface
│   ├── executor_esp32.c       # Redirection handling
│   ├── executor_esp32.h       # Executor interface
│   ├── vfs_esp32.c            # VFS/SPIFFS layer
│   ├── shell_vfs.h            # VFS interface
│   ├── platform_esp32.c       # ESP32 platform HAL
│   ├── platform_linux.c       # Linux platform (testing)
│   ├── platform.h             # Platform abstraction
│   └── CMakeLists.txt         # Component build config
├── tests/
│   ├── test_esp32.sh          # Host simulation tests
│   └── test_esp32_device.py   # Serial device tests
├── docs/
│   ├── USER_GUIDE.md          # End-user documentation
│   └── DEVELOPER_GUIDE.md     # This file
├── CMakeLists.txt             # ESP-IDF project config
├── partitions.csv             # Custom partition table
├── sdkconfig.defaults         # SDK configuration
├── build.sh                   # Convenience build script
└── .gitignore                 # Git ignore for build artifacts
```

---

## Adding New Commands

### Step 1: Declare the Command Function

In `esp_shell.c`, add a forward declaration:

```c
/* Forward declarations */
static int cmd_help(int argc, char **argv);
static int cmd_mycommand(int argc, char **argv);  // Add this
```

### Step 2: Register in Command Table

Add an entry to `builtin_commands[]`:

```c
static const builtin_command_t builtin_commands[] = {
    {"help",      "Show available commands",        cmd_help},
    {"mycommand", "Description of my command",      cmd_mycommand},  // Add
    // ...
    {NULL, NULL, NULL}  // Terminator - must be last!
};
```

### Step 3: Implement the Command

```c
/**
 * @brief My custom command
 * 
 * @param argc Argument count (includes command name)
 * @param argv Argument vector (argv[0] is command name)
 * @return 0 on success, non-zero on error
 */
static int cmd_mycommand(int argc, char **argv) {
    /* Check arguments */
    if (argc < 2) {
        printf("Usage: mycommand <arg>\n");
        return 1;
    }
    
    /* Do something */
    printf("You said: %s\n", argv[1]);
    
    return 0;  /* Success */
}
```

### Step 4: Rebuild and Test

```bash
./flash.sh upload
./flash.sh monitor

esp32> mycommand hello
You said: hello
```

### Command Guidelines

1. **Always validate arguments** - Check `argc` before accessing `argv`
2. **Return 0 for success** - Non-zero indicates error
3. **Use printf for output** - Goes to serial console
4. **Keep it small** - Memory is limited
5. **Document with comments** - Especially parameters

### Example: GPIO Command

```c
static int cmd_gpio(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: gpio <read|write|mode> <pin> [value]\n");
        return 1;
    }
    
    int pin = atoi(argv[2]);
    
    if (strcmp(argv[1], "read") == 0) {
        int level = gpio_get_level(pin);
        printf("GPIO %d: %d\n", pin, level);
    } else if (strcmp(argv[1], "write") == 0) {
        if (argc < 4) {
            printf("Usage: gpio write <pin> <0|1>\n");
            return 1;
        }
        int value = atoi(argv[3]);
        gpio_set_level(pin, value);
    } else if (strcmp(argv[1], "mode") == 0) {
        if (argc < 4) {
            printf("Usage: gpio mode <pin> <in|out>\n");
            return 1;
        }
        gpio_config_t cfg = {
            .pin_bit_mask = (1ULL << pin),
            .mode = strcmp(argv[3], "out") == 0 ? 
                    GPIO_MODE_OUTPUT : GPIO_MODE_INPUT,
        };
        gpio_config(&cfg);
    }
    
    return 0;
}
```

---

## Memory Considerations

### ESP32-S3 Memory Map

| Memory | Size | Usage |
|--------|------|-------|
| SRAM | 512KB | FreeRTOS, heap, stacks |
| PSRAM | 8MB | Optional, not used by shell |
| Flash | 8MB | Code, SPIFFS (1MB) |

### Shell Memory Budget

| Component | Allocation | Notes |
|-----------|------------|-------|
| Command buffer | 256 bytes | Static |
| History | 20 × 256 bytes | Static array |
| Variables | 32 × 160 bytes | Static storage |
| Parsed argv | 17 × sizeof(ptr) | On stack |
| Working dir | 128 bytes | Static |

### Configuration Constants (shell_config.h)

```c
/* ESP32 Settings */
#define SHELL_MAX_LINE_LEN      256     /* Command line buffer */
#define SHELL_MAX_ARGS          16      /* Max arguments */
#define SHELL_HISTORY_SIZE      20      /* History entries */
#define SHELL_MAX_ENV_VARS      32      /* Max variables */
#define SHELL_MAX_VAR_NAME      32      /* Variable name length */
#define SHELL_MAX_VAR_VALUE     128     /* Variable value length */
#define SHELL_MAX_PATH          128     /* Max path length */
#define SHELL_LOW_MEMORY_WARN   8192    /* Low memory threshold */
```

### Memory Monitoring

```c
/* Check free heap */
uint32_t free_heap = esp_get_free_heap_size();
uint32_t min_heap = esp_get_minimum_free_heap_size();

if (free_heap < SHELL_LOW_MEMORY_WARN) {
    printf("Warning: Low memory (%lu bytes)\n", free_heap);
}
```

### Best Practices

1. **Use static allocation** - Avoid malloc where possible
2. **Keep buffers small** - Use SHELL_* constants
3. **Check before allocating** - Verify heap space
4. **Free what you allocate** - Avoid memory leaks
5. **Monitor with `free`** - Check heap after operations

---

## Build System

### ESP-IDF Build

```bash
# Activate ESP-IDF environment (run once per terminal session)
. ~/esp/esp-idf/export.sh

# Set target chip (once per project)
idf.py set-target esp32s3

# Build only
idf.py build

# Build and flash
idf.py flash

# Flash and monitor
idf.py flash monitor

# Clean build
idf.py fullclean
```

### Convenience Script

```bash
./build.sh           # Build only
./build.sh flash     # Flash to device
./build.sh monitor   # Serial monitor
./build.sh all       # Flash and monitor
./build.sh clean     # Clean build
./build.sh size      # Show binary size
```

### Key Build Files

| File | Purpose |
|------|---------|
| `CMakeLists.txt` | ESP-IDF project definition |
| `main/CMakeLists.txt` | Component source registration |
| `partitions.csv` | Flash partition layout |
| `sdkconfig.defaults` | ESP-IDF configuration |
| `build.sh` | Convenience build wrapper |

### Adding a New Source File

1. Create the file in `main/`
2. Add to `main/CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS 
        "main.c"
        "esp_shell.c"
        "mynewfile.c"    # Add here
    INCLUDE_DIRS 
        "."
    REQUIRES 
        driver esp_timer spiffs vfs
)
```

---

## Testing

### Host Tests (No Hardware)

```bash
./tests/test_esp32.sh
./tests/test_esp32.sh -v              # Verbose
./tests/test_esp32.sh -t commands     # Specific category
```

Test categories:
- `source` - Source file structure
- `config` - Configuration constants
- `forbidden` - Forbidden features
- `build` - Build files
- `commands` - Command registration
- `docs` - Documentation

### Device Tests (Hardware Required)

```bash
pip install pyserial
python3 tests/test_esp32_device.py /dev/ttyACM0
python3 tests/test_esp32_device.py /dev/ttyACM0 -t basic
```

Test categories:
- `basic` - echo, pwd, ls, help, cd
- `system` - free, uptime, info, fsinfo
- `variable` - set, unset, env
- `file` - touch, rm
- `gpio` - gpio read
- `error` - Invalid commands
- `parsing` - Quoted strings

### Manual Testing Checklist

- [ ] Shell boots with banner
- [ ] `help` lists all commands
- [ ] `pwd` shows `/spiffs`
- [ ] `ls` works (may be empty)
- [ ] `echo hello` prints `hello`
- [ ] `set VAR=test` and `echo $VAR`
- [ ] `touch test.txt` and `rm test.txt`
- [ ] `gpio read 0` shows value
- [ ] `free` shows heap
- [ ] `reboot` restarts

---

## Debugging

### Serial Logging

Use ESP-IDF logging macros:

```c
#include "esp_log.h"

static const char *TAG = "mymodule";

ESP_LOGI(TAG, "Info message: %d", value);
ESP_LOGW(TAG, "Warning message");
ESP_LOGE(TAG, "Error message");
ESP_LOGD(TAG, "Debug message");  // Only if LOG_LEVEL >= DEBUG
```

### Log Level Configuration

In `sdkconfig.defaults`:

```
CONFIG_LOG_DEFAULT_LEVEL_DEBUG=y
CONFIG_LOG_DEFAULT_LEVEL=4
```

### Stack Overflow Detection

FreeRTOS can detect stack overflow:

```c
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    ESP_LOGE("STACK", "Stack overflow in task: %s", pcTaskName);
    abort();
}
```

### Common Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| Kernel panic | Stack overflow | Increase stack size |
| Garbled output | Baud rate mismatch | Use 115200 |
| Watchdog reset | Infinite loop | Add `vTaskDelay()` |
| Out of memory | Memory leak | Check `free` command |

---

## Porting to Other ESP32 Variants

### ESP32 (Original)

Set the target chip:

```bash
idf.py set-target esp32
idf.py build
```

### ESP32-C3

```bash
idf.py set-target esp32c3
idf.py build
```

### Key Considerations

1. **GPIO numbers** - Vary between variants
2. **Flash size** - Adjust partition table
3. **RAM size** - May need smaller buffers
4. **Peripherals** - Check availability

### Platform Abstraction

The `platform.h` interface abstracts hardware differences:

```c
/* Implement these for new platforms */
int platform_init(void);
int platform_getchar(void);
void platform_putchar(int c);
void platform_sleep_ms(int ms);
void platform_reboot(void);
```

---

## Code Style Guidelines

### Naming Conventions

- Functions: `snake_case` (e.g., `cmd_echo`, `parser_init`)
- Macros: `UPPER_CASE` (e.g., `SHELL_MAX_ARGS`)
- Types: `snake_case_t` (e.g., `builtin_command_t`)
- Static globals: Prefixed with module (e.g., `parser_vars`)

### Comments

```c
/**
 * @brief Brief description of function
 * 
 * Detailed description if needed.
 * 
 * @param param1 Description of parameter
 * @return Description of return value
 */
int my_function(int param1);

/* Single-line comment for simple notes */

/*
 * Multi-line comment for longer explanations
 * that span multiple lines.
 */
```

### Error Handling

```c
/* Always check return values */
if (some_function() != 0) {
    ESP_LOGE(TAG, "Operation failed");
    return -1;
}

/* Use errno for system calls */
FILE *f = fopen(path, "r");
if (f == NULL) {
    printf("Error: %s\n", strerror(errno));
    return -1;
}
```

---

## Contributing

1. Run tests before committing: `./tests/test_esp32.sh`
2. Keep code style consistent
3. Document new commands and features
4. Update USER_GUIDE.md for user-facing changes
5. Update this guide for developer-facing changes
