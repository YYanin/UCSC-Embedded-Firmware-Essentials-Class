/**
 * @file shell_config.h
 * @brief Centralized Shell Configuration for ESP32 and Linux
 * 
 * This header provides unified configuration constants for the shell.
 * All buffer sizes, limits, and memory-related settings are defined here
 * to ensure consistent behavior and easy tuning for different platforms.
 * 
 * ESP32 uses smaller values to fit within RAM constraints:
 * - ESP32-S3: 512KB SRAM (327KB usable after FreeRTOS/IDF overhead)
 * - Target heap usage: < 32KB for shell operations
 * 
 * Linux uses larger values for desktop-class memory:
 * - Generous buffer sizes for longer commands
 * - Larger history and environment capacity
 */

#ifndef SHELL_CONFIG_H
#define SHELL_CONFIG_H

/* ============================================================================
 * Platform Detection
 * ============================================================================ */

#ifdef ESP_PLATFORM
/* --------------------------------------------------------------------------
 * ESP32 Platform Configuration
 * Memory-constrained embedded environment
 * -------------------------------------------------------------------------- */

/* Command line and parsing limits */
#define SHELL_MAX_LINE_LEN      256     /* Max command line length */
#define SHELL_MAX_ARGS          16      /* Max arguments per command */
#define SHELL_MAX_ARG_LEN       128     /* Max length of single argument */

/* History configuration */
#define SHELL_HISTORY_SIZE      20      /* Number of history entries */

/* Environment variable limits */
#define SHELL_MAX_ENV_VARS      32      /* Max number of variables */
#define SHELL_MAX_VAR_NAME      32      /* Max variable name length */
#define SHELL_MAX_VAR_VALUE     128     /* Max variable value length */

/* Filesystem path limits */
#define SHELL_MAX_PATH          128     /* Max path length */
#define SHELL_MAX_FILENAME      64      /* Max filename length */

/* Terminal configuration */
#define SHELL_TERMINAL_WIDTH    80      /* Default terminal width */
#define SHELL_TERMINAL_HEIGHT   24      /* Default terminal height */

/* FreeRTOS task configuration */
#define SHELL_STACK_SIZE        8192    /* Stack size for shell task */
#define SHELL_TASK_PRIORITY     5       /* Task priority (0-configMAX_PRIORITIES-1) */

/* Memory monitoring thresholds */
#define SHELL_LOW_MEMORY_WARN   8192    /* Warn when free heap below this (bytes) */
#define SHELL_CRITICAL_MEMORY   4096    /* Critical low memory threshold (bytes) */

#else
/* --------------------------------------------------------------------------
 * Linux/Desktop Platform Configuration  
 * Memory-rich environment with generous limits
 * -------------------------------------------------------------------------- */

/* Command line and parsing limits */
#define SHELL_MAX_LINE_LEN      4096    /* Max command line length */
#define SHELL_MAX_ARGS          256     /* Max arguments per command */
#define SHELL_MAX_ARG_LEN       4096    /* Max length of single argument */

/* History configuration */
#define SHELL_HISTORY_SIZE      500     /* Number of history entries */

/* Environment variable limits */
#define SHELL_MAX_ENV_VARS      256     /* Max number of variables */
#define SHELL_MAX_VAR_NAME      256     /* Max variable name length */
#define SHELL_MAX_VAR_VALUE     4096    /* Max variable value length */

/* Filesystem path limits */
#define SHELL_MAX_PATH          4096    /* Max path length */
#define SHELL_MAX_FILENAME      256     /* Max filename length */

/* Terminal configuration */
#define SHELL_TERMINAL_WIDTH    120     /* Default terminal width */
#define SHELL_TERMINAL_HEIGHT   40      /* Default terminal height */

/* Process/threading configuration */
#define SHELL_STACK_SIZE        65536   /* Stack size for threads */

/* No memory monitoring on Linux - not constrained */
#define SHELL_LOW_MEMORY_WARN   0
#define SHELL_CRITICAL_MEMORY   0

#endif /* ESP_PLATFORM */

/* ============================================================================
 * Derived Constants (platform-independent)
 * ============================================================================ */

/* Size of argv array (needs room for NULL terminator) */
#define SHELL_ARGV_SIZE         (SHELL_MAX_ARGS + 1)

/* Prompt string - can be customized */
#ifndef SHELL_PROMPT
#ifdef ESP_PLATFORM
#define SHELL_PROMPT            "esp32> "
#else
#define SHELL_PROMPT            "ushell$ "
#endif
#endif

/* ============================================================================
 * Compatibility Macros
 * ============================================================================
 * These map old names to new centralized names for backward compatibility.
 * New code should use SHELL_* names directly.
 */

/* esp_shell.h compatibility */
#ifndef ESP_SHELL_MAX_LINE_LEN
#define ESP_SHELL_MAX_LINE_LEN  SHELL_MAX_LINE_LEN
#endif

#ifndef ESP_SHELL_MAX_ARGS
#define ESP_SHELL_MAX_ARGS      SHELL_MAX_ARGS
#endif

#ifndef ESP_SHELL_HISTORY_SIZE
#define ESP_SHELL_HISTORY_SIZE  SHELL_HISTORY_SIZE
#endif

#ifndef ESP_SHELL_MAX_PATH
#define ESP_SHELL_MAX_PATH      SHELL_MAX_PATH
#endif

#ifndef ESP_SHELL_PROMPT
#define ESP_SHELL_PROMPT        SHELL_PROMPT
#endif

/* parser_esp32.h compatibility */
#ifndef PARSER_MAX_LINE_LEN
#define PARSER_MAX_LINE_LEN     SHELL_MAX_LINE_LEN
#endif

#ifndef PARSER_MAX_ARGS
#define PARSER_MAX_ARGS         SHELL_MAX_ARGS
#endif

#ifndef PARSER_MAX_ARG_LEN
#define PARSER_MAX_ARG_LEN      SHELL_MAX_ARG_LEN
#endif

#ifndef PARSER_MAX_VARS
#define PARSER_MAX_VARS         SHELL_MAX_ENV_VARS
#endif

#ifndef PARSER_MAX_VAR_NAME
#define PARSER_MAX_VAR_NAME     SHELL_MAX_VAR_NAME
#endif

#ifndef PARSER_MAX_VAR_VALUE
#define PARSER_MAX_VAR_VALUE    SHELL_MAX_VAR_VALUE
#endif

/* terminal_esp32.h compatibility */
#ifndef TERMINAL_MAX_LINE_LEN
#define TERMINAL_MAX_LINE_LEN   SHELL_MAX_LINE_LEN
#endif

#ifndef TERMINAL_HISTORY_SIZE
#define TERMINAL_HISTORY_SIZE   SHELL_HISTORY_SIZE
#endif

#ifndef TERMINAL_DEFAULT_WIDTH
#define TERMINAL_DEFAULT_WIDTH  SHELL_TERMINAL_WIDTH
#endif

#ifndef TERMINAL_DEFAULT_HEIGHT
#define TERMINAL_DEFAULT_HEIGHT SHELL_TERMINAL_HEIGHT
#endif

#endif /* SHELL_CONFIG_H */
