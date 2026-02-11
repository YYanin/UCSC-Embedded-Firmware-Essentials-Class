/**
 * @file executor_esp32.c
 * @brief ESP32 Shell Executor - Direct Execution with I/O Redirection
 * 
 * Implements command execution with simple I/O redirection support.
 * Uses VFS layer for file operations to maintain portability.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "executor_esp32.h"
#include "shell_vfs.h"
#include "esp_log.h"

static const char *TAG = "executor";

/* ============================================================================
 * I/O Redirection State
 * ============================================================================ */

/* Current redirection output file */
static vfs_file_t g_redir_file = NULL;

/* ============================================================================
 * Redirection Parsing
 * ============================================================================ */

/**
 * @brief Parse command line and extract redirection operators
 */
int parse_redirections(int argc, char **argv, parsed_cmd_t *cmd) {
    if (cmd == NULL || argv == NULL) {
        return -1;
    }
    
    /* Initialize output structure */
    cmd->argc = 0;
    cmd->argv = argv;
    cmd->stdout_redir = REDIR_NONE;
    cmd->stdout_file = NULL;
    cmd->stdin_redir = REDIR_NONE;
    cmd->stdin_file = NULL;
    
    int new_argc = 0;
    
    for (int i = 0; i < argc; i++) {
        /* Check for output redirection (append) */
        if (strcmp(argv[i], ">>") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "syntax error: expected filename after '>>'\n");
                return -1;
            }
            cmd->stdout_redir = REDIR_APPEND;
            cmd->stdout_file = argv[i + 1];
            i++;  /* Skip the filename */
            continue;
        }
        
        /* Check for output redirection (truncate) */
        if (strcmp(argv[i], ">") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "syntax error: expected filename after '>'\n");
                return -1;
            }
            cmd->stdout_redir = REDIR_OUTPUT;
            cmd->stdout_file = argv[i + 1];
            i++;  /* Skip the filename */
            continue;
        }
        
        /* Check for input redirection */
        if (strcmp(argv[i], "<") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "syntax error: expected filename after '<'\n");
                return -1;
            }
            cmd->stdin_redir = REDIR_INPUT;
            cmd->stdin_file = argv[i + 1];
            i++;  /* Skip the filename */
            continue;
        }
        
        /* Check for inline redirection like ">file" or ">>file" */
        if (argv[i][0] == '>' && argv[i][1] == '>') {
            if (argv[i][2] == '\0') {
                /* ">>" with space before filename - handled above */
                fprintf(stderr, "syntax error: expected filename after '>>'\n");
                return -1;
            }
            cmd->stdout_redir = REDIR_APPEND;
            cmd->stdout_file = &argv[i][2];
            continue;
        }
        
        if (argv[i][0] == '>' && argv[i][1] != '\0') {
            cmd->stdout_redir = REDIR_OUTPUT;
            cmd->stdout_file = &argv[i][1];
            continue;
        }
        
        if (argv[i][0] == '<' && argv[i][1] != '\0') {
            cmd->stdin_redir = REDIR_INPUT;
            cmd->stdin_file = &argv[i][1];
            continue;
        }
        
        /* Regular argument - keep it */
        argv[new_argc++] = argv[i];
    }
    
    cmd->argc = new_argc;
    
    /* Null-terminate the argument list */
    if (new_argc < argc) {
        argv[new_argc] = NULL;
    }
    
    return 0;
}

/**
 * @brief Check if pipeline operator exists in command
 */
int has_pipeline(int argc, char **argv) {
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "|") == 0) {
            return 1;
        }
    }
    return 0;
}

/**
 * @brief Check if background operator (&) exists in command
 * 
 * Detects trailing & or & as separate argument.
 * ESP32 cannot support background processes.
 */
int has_background(int argc, char **argv) {
    if (argc == 0) return 0;
    
    /* Check if last argument is & */
    if (strcmp(argv[argc - 1], "&") == 0) {
        return 1;
    }
    
    /* Check if last argument ends with & */
    const char *last = argv[argc - 1];
    size_t len = strlen(last);
    if (len > 0 && last[len - 1] == '&') {
        return 1;
    }
    
    return 0;
}

/* ============================================================================
 * Output Capture for Redirection
 * ============================================================================ */

/* ============================================================================
 * Command Execution with Redirection
 * ============================================================================ */

/**
 * @brief Set up output redirection
 * 
 * @param filepath Path to output file
 * @param append True to append, false to truncate
 * @return 0 on success, -1 on error
 */
static int setup_output_redirection(const char *filepath, int append) {
    const char *mode = append ? "a" : "w";
    
    g_redir_file = vfs_open(filepath, mode);
    if (g_redir_file == NULL) {
        fprintf(stderr, "cannot open '%s' for writing\n", filepath);
        return -1;
    }
    
    ESP_LOGD(TAG, "Output redirected to: %s (mode=%s)", filepath, mode);
    
    return 0;
}

/**
 * @brief Restore original stdout
 */
static void restore_output(void) {
    if (g_redir_file != NULL) {
        vfs_close(g_redir_file);
        g_redir_file = NULL;
    }
}

/**
 * @brief Execute command with output captured to buffer
 * 
 * This version uses a simple approach: execute the command and
 * capture its output by replacing printf temporarily.
 * 
 * Note: This is a simplified implementation. Full stdout redirection
 * would require modifying the VFS layer or using ESP-IDF's
 * esp_vfs_register for custom stdout handling.
 */
int execute_with_redirection(parsed_cmd_t *cmd, 
                             int (*func)(int argc, char **argv)) {
    int result;
    
    if (cmd == NULL || func == NULL) {
        return -1;
    }
    
    /* Set up output redirection if needed */
    if (cmd->stdout_redir != REDIR_NONE && cmd->stdout_file != NULL) {
        if (setup_output_redirection(cmd->stdout_file, 
                                      cmd->stdout_redir == REDIR_APPEND) != 0) {
            return 1;
        }
    }
    
    /* TODO: Set up input redirection if needed */
    /* Input redirection would require a custom stdin implementation */
    if (cmd->stdin_redir != REDIR_NONE) {
        fprintf(stderr, "input redirection not yet supported\n");
        restore_output();
        return 1;
    }
    
    /* Execute the command */
    result = func(cmd->argc, cmd->argv);
    
    /* Restore original I/O */
    restore_output();
    
    return result;
}

/* ============================================================================
 * Redirection-Aware Print Functions
 * ============================================================================ */

/**
 * @brief Get the current redirection output file
 */
void* get_redir_output_file(void) {
    return g_redir_file;
}

/**
 * @brief Check if output is being redirected
 */
int is_output_redirected(void) {
    return g_redir_file != NULL;
}

/**
 * @brief Write string to redirected output or stdout
 */
void redir_puts(const char *str) {
    if (g_redir_file != NULL) {
        size_t len = strlen(str);
        vfs_write(str, len, g_redir_file);
    } else {
        fputs(str, stdout);
    }
}

/**
 * @brief Printf replacement that respects redirection
 * 
 * Commands should use this instead of printf when output
 * redirection needs to be supported.
 */
int redir_printf(const char *format, ...) {
    va_list args;
    int len;
    char buf[512];
    
    va_start(args, format);
    len = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    
    if (len < 0) {
        return len;
    }
    
    /* Handle truncation */
    if (len >= (int)sizeof(buf)) {
        len = sizeof(buf) - 1;
    }
    
    if (g_redir_file != NULL) {
        /* Write to redirection file */
        vfs_write(buf, len, g_redir_file);
    } else {
        /* Write to stdout */
        fputs(buf, stdout);
    }
    
    return len;
}
