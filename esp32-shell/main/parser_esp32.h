/**
 * @file parser_esp32.h
 * @brief Simplified Parser for ESP32 Shell
 * 
 * This header defines a lightweight command parser optimized for ESP32's
 * memory constraints. It provides:
 * - Simple command tokenization
 * - Variable expansion ($VAR, ${VAR})
 * - Quote handling (single and double quotes)
 * - Comment handling (#)
 * - I/O redirection (>, >>, <)
 * - Memory bounds checking
 * 
 * Features NOT included (to save memory):
 * - Complex pipelines (single pipe only if needed)
 * - Arithmetic evaluation $((expr))
 * - Command substitution $(cmd)
 * - Glob expansion (*.txt, etc.)
 * - Arrays
 * - Here documents (<<EOF)
 * 
 * Usage:
 *   parser_init();
 *   
 *   parser_result_t result;
 *   if (parser_parse_line("echo $NAME > file.txt", &result) == 0) {
 *       // result.argc, result.argv contain parsed command
 *       execute_command(&result);
 *       parser_free_result(&result);
 *   }
 */

#ifndef PARSER_ESP32_H
#define PARSER_ESP32_H

#include <stddef.h>
#include <stdbool.h>

/* Include centralized configuration - provides PARSER_* compatibility macros */
#include "shell_config.h"

/* ============================================================================
 * Parser Configuration - Memory Limits
 * ============================================================================
 * NOTE: Configuration constants are now defined in shell_config.h
 * The following are provided via compatibility macros:
 *   - PARSER_MAX_LINE_LEN, PARSER_MAX_ARGS, PARSER_MAX_ARG_LEN
 *   - PARSER_MAX_VARS, PARSER_MAX_VAR_NAME, PARSER_MAX_VAR_VALUE
 * These can still be overridden by defining them before including this header.
 * ============================================================================ */

/* Maximum length of a single command line */
#ifndef PARSER_MAX_LINE_LEN
#define PARSER_MAX_LINE_LEN     256
#endif

/* Maximum number of arguments in a command */
#ifndef PARSER_MAX_ARGS
#define PARSER_MAX_ARGS         32
#endif

/* Maximum length of a single argument after expansion */
#ifndef PARSER_MAX_ARG_LEN
#define PARSER_MAX_ARG_LEN      128
#endif

/* Maximum number of environment variables */
#ifndef PARSER_MAX_VARS
#define PARSER_MAX_VARS         32
#endif

/* Maximum length of variable name */
#ifndef PARSER_MAX_VAR_NAME
#define PARSER_MAX_VAR_NAME     32
#endif

/* Maximum length of variable value */
#ifndef PARSER_MAX_VAR_VALUE
#define PARSER_MAX_VAR_VALUE    128
#endif

/* ============================================================================
 * Error Codes
 * ============================================================================ */

typedef enum {
    PARSER_OK = 0,              /* Parsing successful */
    PARSER_ERR_EMPTY,           /* Empty command (not an error, just skip) */
    PARSER_ERR_LINE_TOO_LONG,   /* Command line exceeds max length */
    PARSER_ERR_TOO_MANY_ARGS,   /* Too many arguments */
    PARSER_ERR_ARG_TOO_LONG,    /* Single argument too long after expansion */
    PARSER_ERR_UNCLOSED_QUOTE,  /* Missing closing quote */
    PARSER_ERR_SYNTAX,          /* General syntax error */
    PARSER_ERR_REDIR_NOFILE,    /* Redirection without filename */
    PARSER_ERR_VAR_NOT_FOUND,   /* Variable not found (warning, expands to empty) */
    PARSER_ERR_MEMORY           /* Memory allocation failed */
} parser_error_t;

/*
 * NOTE: Redirection types (REDIR_NONE, REDIR_OUTPUT, REDIR_APPEND, REDIR_INPUT)
 * are defined in executor_esp32.h as redir_type_t to avoid redeclaration.
 * Include executor_esp32.h before this header if you need redirection support.
 */

/* ============================================================================
 * Parser Result Structure
 * ============================================================================ */

/* Forward declare redir_type_t from executor_esp32.h to avoid circular includes */
#ifndef EXECUTOR_ESP32_H
typedef enum {
    PARSER_REDIR_NONE = 0,
    PARSER_REDIR_OUTPUT,
    PARSER_REDIR_APPEND,
    PARSER_REDIR_INPUT
} parser_redir_t;
#define PARSER_USES_OWN_REDIR_T
#else
/* executor_esp32.h already included, use redir_type_t */
typedef redir_type_t parser_redir_t;
#endif

/**
 * @brief Result of parsing a command line
 * 
 * Contains the tokenized command with arguments and redirection info.
 * The argv array points into the internal buffer - do not modify.
 */
typedef struct {
    /* Parsed command and arguments */
    int argc;                           /* Number of arguments */
    char *argv[PARSER_MAX_ARGS + 1];    /* Argument pointers (NULL terminated) */
    
    /* Internal buffer for expanded arguments */
    char buffer[PARSER_MAX_LINE_LEN * 2];  /* Expanded command buffer */
    size_t buffer_used;                    /* Bytes used in buffer */
    
    /* Output redirection - uses redir_type_t values (REDIR_NONE, etc) */
    int stdout_redir;                   /* Type of stdout redirection */
    const char *stdout_file;            /* Filename for stdout */
    
    /* Input redirection */
    int stdin_redir;                    /* Type of stdin redirection */
    const char *stdin_file;             /* Filename for stdin */
    
    /* Error information */
    parser_error_t error;               /* Error code if parsing failed */
    const char *error_msg;              /* Human-readable error message */
    
} parser_result_t;

/* ============================================================================
 * Environment Variable Storage
 * ============================================================================ */

/**
 * @brief Environment variable structure
 */
typedef struct {
    char name[PARSER_MAX_VAR_NAME];
    char value[PARSER_MAX_VAR_VALUE];
    int in_use;  /* 1 if slot is in use, 0 if free */
} parser_var_t;

/* ============================================================================
 * Parser Functions
 * ============================================================================ */

/**
 * @brief Initialize the parser module
 * 
 * Clears all environment variables and resets state.
 */
void parser_init(void);

/**
 * @brief Parse a command line into tokens
 * 
 * Performs:
 * 1. Comment removal (everything after #)
 * 2. Variable expansion ($VAR, ${VAR})
 * 3. Quote processing ("..." and '...')
 * 4. Whitespace tokenization
 * 5. Redirection extraction (>, >>, <)
 * 
 * @param line Input command line (not modified)
 * @param result Output structure for parsed result
 * @return PARSER_OK on success, error code on failure
 */
parser_error_t parser_parse_line(const char *line, parser_result_t *result);

/**
 * @brief Free resources associated with a parse result
 * 
 * Currently a no-op since we use static buffers, but call it
 * for forward compatibility.
 * 
 * @param result Parse result to free
 */
void parser_free_result(parser_result_t *result);

/**
 * @brief Get error message for a parser error code
 * 
 * @param error Error code
 * @return Human-readable error message
 */
const char* parser_error_string(parser_error_t error);

/* ============================================================================
 * Environment Variable Functions
 * ============================================================================ */

/**
 * @brief Set an environment variable
 * 
 * @param name Variable name
 * @param value Variable value
 * @return 0 on success, -1 if no space available
 */
int parser_setvar(const char *name, const char *value);

/**
 * @brief Get an environment variable value
 * 
 * @param name Variable name
 * @return Variable value, or NULL if not found
 */
const char* parser_getvar(const char *name);

/**
 * @brief Unset (remove) an environment variable
 * 
 * @param name Variable name
 * @return 0 on success, -1 if not found
 */
int parser_unsetvar(const char *name);

/**
 * @brief List all environment variables
 * 
 * Calls the callback for each defined variable.
 * 
 * @param callback Function to call for each variable
 * @param user_data User data passed to callback
 */
void parser_list_vars(void (*callback)(const char *name, const char *value, void *user_data), 
                      void *user_data);

/**
 * @brief Get count of defined variables
 * 
 * @return Number of variables currently defined
 */
int parser_var_count(void);

/**
 * @brief Clear all environment variables
 */
void parser_clear_vars(void);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Check if a line is a variable assignment
 * 
 * Checks if the line is in the form "NAME=value".
 * 
 * @param line Input line
 * @param name Output: variable name (must be PARSER_MAX_VAR_NAME bytes)
 * @param value Output: variable value (must be PARSER_MAX_VAR_VALUE bytes)
 * @return 1 if it's an assignment, 0 if not
 */
int parser_is_assignment(const char *line, char *name, char *value);

/**
 * @brief Expand variables in a string
 * 
 * Replaces $VAR and ${VAR} with their values.
 * 
 * @param input Input string
 * @param output Output buffer
 * @param output_size Size of output buffer
 * @return Number of characters written, or -1 on error
 */
int parser_expand_vars(const char *input, char *output, size_t output_size);

#endif /* PARSER_ESP32_H */
