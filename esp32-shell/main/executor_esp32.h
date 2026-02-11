/**
 * @file executor_esp32.h
 * @brief ESP32 Shell Executor - Direct Execution Model
 * 
 * This header defines the execution model for ESP32 which uses direct
 * function calls instead of fork/exec. This is required because:
 * - ESP32 runs on FreeRTOS, not Linux
 * - No process creation/fork support
 * - All commands execute in the same context
 * 
 * Features:
 * - Direct function dispatch (no subprocess)
 * - Simple I/O redirection using VFS
 * - No background jobs (single-threaded)
 * - No pipelines to external commands
 */

#ifndef EXECUTOR_ESP32_H
#define EXECUTOR_ESP32_H

#include <stddef.h>

/* ============================================================================
 * Redirection Types
 * ============================================================================ */

/**
 * @brief I/O Redirection types
 */
typedef enum {
    REDIR_NONE = 0,      /**< No redirection */
    REDIR_OUTPUT,        /**< > : Write to file (truncate) */
    REDIR_APPEND,        /**< >> : Append to file */
    REDIR_INPUT          /**< < : Read from file */
} redir_type_t;

/**
 * @brief Parsed command with redirection info
 */
typedef struct {
    int argc;                      /**< Argument count */
    char **argv;                   /**< Argument array */
    redir_type_t stdout_redir;     /**< Stdout redirection type */
    char *stdout_file;             /**< Stdout redirection filename */
    redir_type_t stdin_redir;      /**< Stdin redirection type */
    char *stdin_file;              /**< Stdin redirection filename */
} parsed_cmd_t;

/* ============================================================================
 * Function Prototypes
 * ============================================================================ */

/**
 * @brief Parse command line and extract redirection operators
 * 
 * Scans the argument list for >, >>, and < operators and separates
 * them from the command arguments.
 * 
 * @param argc Original argument count
 * @param argv Original argument array (will be modified)
 * @param cmd Output structure with parsed command and redirections
 * @return 0 on success, -1 on error
 */
int parse_redirections(int argc, char **argv, parsed_cmd_t *cmd);

/**
 * @brief Execute command with I/O redirection
 * 
 * Sets up file-based I/O redirection before executing the command
 * and restores original I/O after completion.
 * 
 * @param cmd Parsed command structure with redirection info
 * @param func Command function to execute
 * @return Command exit status
 */
int execute_with_redirection(parsed_cmd_t *cmd, 
                             int (*func)(int argc, char **argv));

/**
 * @brief Check if pipeline operator exists in command
 * 
 * @param argc Argument count
 * @param argv Argument array
 * @return 1 if pipeline found, 0 otherwise
 */
int has_pipeline(int argc, char **argv);

/**
 * @brief Check if background operator (&) exists in command
 * 
 * ESP32 does not support background processes because there's no
 * fork() system call. This function detects & to give a helpful error.
 * 
 * @param argc Argument count
 * @param argv Argument array
 * @return 1 if background operator found, 0 otherwise
 */
int has_background(int argc, char **argv);

/**
 * @brief Get the current redirection output file
 * 
 * Commands can use this to write directly to the redirection
 * target instead of stdout when output is being redirected.
 * 
 * @return Pointer to vfs_file_t if redirected, NULL otherwise
 */
void* get_redir_output_file(void);

/**
 * @brief Check if output is currently being redirected
 * 
 * @return 1 if output is redirected, 0 otherwise
 */
int is_output_redirected(void);

/**
 * @brief Write string to redirected output or stdout
 * 
 * This function should be used by commands that support redirection.
 * It writes to the redirection file if active, otherwise to stdout.
 * 
 * @param str String to output
 */
void redir_puts(const char *str);

/**
 * @brief Printf-style output with redirection support
 * 
 * @param format Printf format string
 * @param ... Arguments
 * @return Number of characters written
 */
int redir_printf(const char *format, ...);

#endif /* EXECUTOR_ESP32_H */
