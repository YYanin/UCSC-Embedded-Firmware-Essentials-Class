/**
 * @file esp_shell.h
 * @brief ESP32 Shell - Shell Interface Header
 * 
 * This header defines the ESP32-specific shell interface functions.
 * It provides the initialization and run functions for the interactive shell.
 */

#ifndef ESP_SHELL_H
#define ESP_SHELL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Include centralized configuration - all constants defined there */
#include "shell_config.h"

/* ============================================================================
 * ESP32 Shell Configuration Constants
 * ============================================================================
 * NOTE: All configuration constants are now defined in shell_config.h
 * The following are provided by shell_config.h via compatibility macros:
 *   - ESP_SHELL_MAX_LINE_LEN  (command line buffer size)
 *   - ESP_SHELL_MAX_ARGS      (max arguments per command)
 *   - ESP_SHELL_HISTORY_SIZE  (history entries to remember)
 *   - ESP_SHELL_PROMPT        (shell prompt string)
 *   - ESP_SHELL_MAX_PATH      (max working directory path)
 * ============================================================================ */


/* ============================================================================
 * Shell Initialization and Control Functions
 * ============================================================================ */

/**
 * @brief Initialize the ESP32 shell
 * 
 * Sets up internal data structures, registers built-in commands,
 * and prepares the shell for operation.
 * 
 * @return 0 on success, negative error code on failure
 */
int esp_shell_init(void);

/**
 * @brief Run the shell main loop
 * 
 * Enters the interactive shell loop. This function reads user input,
 * parses commands, and executes them. It runs forever until the
 * 'exit' command is issued (which reboots the ESP32).
 * 
 * This function should be called from the main task after
 * esp_shell_init() has been called.
 */
void esp_shell_run(void);

/**
 * @brief Execute a single command string
 * 
 * Parses and executes a command string as if it were typed by the user.
 * Useful for running startup scripts or programmatic command execution.
 * 
 * @param cmdline The command string to execute (null-terminated)
 * @return Exit status of the command (0 = success)
 */
int esp_shell_execute(const char *cmdline);

/**
 * @brief Get the current working directory
 * 
 * @param buf Buffer to store the path
 * @param size Size of the buffer
 * @return Pointer to buf on success, NULL on failure
 */
char* esp_shell_getcwd(char *buf, size_t size);

/**
 * @brief Change the current working directory
 * 
 * @param path The new directory path
 * @return 0 on success, -1 on failure
 */
int esp_shell_chdir(const char *path);


/* ============================================================================
 * Built-in Command Registration
 * ============================================================================ */

/**
 * @brief Command handler function type
 * 
 * All shell commands must conform to this signature.
 * 
 * @param argc Number of arguments (including command name)
 * @param argv Array of argument strings
 * @return Exit status (0 = success, non-zero = error)
 */
typedef int (*esp_shell_cmd_func_t)(int argc, char **argv);

/**
 * @brief Command registration structure
 */
typedef struct {
    const char *name;           /* Command name */
    const char *help;           /* Brief help text */
    esp_shell_cmd_func_t func;  /* Handler function */
} esp_shell_cmd_t;

/**
 * @brief Register a new command with the shell
 * 
 * @param cmd Pointer to command structure (must have static lifetime)
 * @return 0 on success, -1 on failure
 */
int esp_shell_register_cmd(const esp_shell_cmd_t *cmd);


#ifdef __cplusplus
}
#endif

#endif /* ESP_SHELL_H */
