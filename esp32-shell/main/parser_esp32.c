/**
 * @file parser_esp32.c
 * @brief Simplified Parser Implementation for ESP32 Shell
 * 
 * Implements a lightweight command parser optimized for ESP32 memory constraints.
 * Uses static buffers to avoid dynamic memory allocation.
 */

/* Include executor first to get REDIR_* constants defined */
#include "executor_esp32.h"
#include "parser_esp32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef ESP_PLATFORM
#include "esp_log.h"
static const char *TAG = "parser";
#endif

/* ============================================================================
 * Static Storage for Environment Variables
 * ============================================================================ */

/* Array of environment variables - static allocation */
static parser_var_t g_vars[PARSER_MAX_VARS];

/* Count of variables in use */
static int g_var_count = 0;

/* ============================================================================
 * Error Messages
 * ============================================================================ */

static const char* error_messages[] = {
    "OK",                                   /* PARSER_OK */
    "Empty command",                        /* PARSER_ERR_EMPTY */
    "Command line too long",                /* PARSER_ERR_LINE_TOO_LONG */
    "Too many arguments",                   /* PARSER_ERR_TOO_MANY_ARGS */
    "Argument too long after expansion",    /* PARSER_ERR_ARG_TOO_LONG */
    "Unclosed quote",                       /* PARSER_ERR_UNCLOSED_QUOTE */
    "Syntax error",                         /* PARSER_ERR_SYNTAX */
    "Missing filename after redirection",   /* PARSER_ERR_REDIR_NOFILE */
    "Variable not found",                   /* PARSER_ERR_VAR_NOT_FOUND */
    "Memory allocation failed"              /* PARSER_ERR_MEMORY */
};

/* ============================================================================
 * Parser Initialization
 * ============================================================================ */

/**
 * @brief Initialize the parser module
 */
void parser_init(void) {
    /* Clear all variables */
    memset(g_vars, 0, sizeof(g_vars));
    g_var_count = 0;
    
#ifdef ESP_PLATFORM
    ESP_LOGI(TAG, "Parser initialized (max %d vars, %d args)", 
             PARSER_MAX_VARS, PARSER_MAX_ARGS);
#endif
}

/* ============================================================================
 * Environment Variable Functions
 * ============================================================================ */

/**
 * @brief Find a variable by name
 * 
 * @param name Variable name to find
 * @return Pointer to variable struct, or NULL if not found
 */
static parser_var_t* find_var(const char *name) {
    for (int i = 0; i < PARSER_MAX_VARS; i++) {
        if (g_vars[i].in_use && strcmp(g_vars[i].name, name) == 0) {
            return &g_vars[i];
        }
    }
    return NULL;
}

/**
 * @brief Find a free variable slot
 * 
 * @return Pointer to free slot, or NULL if none available
 */
static parser_var_t* find_free_slot(void) {
    for (int i = 0; i < PARSER_MAX_VARS; i++) {
        if (!g_vars[i].in_use) {
            return &g_vars[i];
        }
    }
    return NULL;
}

/**
 * @brief Set an environment variable
 */
int parser_setvar(const char *name, const char *value) {
    if (name == NULL || name[0] == '\0') {
        return -1;
    }
    
    /* Check name length */
    if (strlen(name) >= PARSER_MAX_VAR_NAME) {
        return -1;
    }
    
    /* Check value length */
    size_t value_len = value ? strlen(value) : 0;
    if (value_len >= PARSER_MAX_VAR_VALUE) {
        return -1;
    }
    
    /* Look for existing variable */
    parser_var_t *var = find_var(name);
    
    if (var != NULL) {
        /* Update existing variable */
        strncpy(var->value, value ? value : "", PARSER_MAX_VAR_VALUE - 1);
        var->value[PARSER_MAX_VAR_VALUE - 1] = '\0';
        return 0;
    }
    
    /* Find free slot for new variable */
    var = find_free_slot();
    if (var == NULL) {
        /* No space available */
        return -1;
    }
    
    /* Create new variable */
    strncpy(var->name, name, PARSER_MAX_VAR_NAME - 1);
    var->name[PARSER_MAX_VAR_NAME - 1] = '\0';
    strncpy(var->value, value ? value : "", PARSER_MAX_VAR_VALUE - 1);
    var->value[PARSER_MAX_VAR_VALUE - 1] = '\0';
    var->in_use = 1;
    g_var_count++;
    
    return 0;
}

/**
 * @brief Get an environment variable value
 */
const char* parser_getvar(const char *name) {
    if (name == NULL) {
        return NULL;
    }
    
    parser_var_t *var = find_var(name);
    if (var != NULL) {
        return var->value;
    }
    
    return NULL;
}

/**
 * @brief Unset (remove) an environment variable
 */
int parser_unsetvar(const char *name) {
    if (name == NULL) {
        return -1;
    }
    
    parser_var_t *var = find_var(name);
    if (var != NULL) {
        var->in_use = 0;
        var->name[0] = '\0';
        var->value[0] = '\0';
        g_var_count--;
        return 0;
    }
    
    return -1;
}

/**
 * @brief List all environment variables
 */
void parser_list_vars(void (*callback)(const char *name, const char *value, void *user_data), 
                      void *user_data) {
    if (callback == NULL) {
        return;
    }
    
    for (int i = 0; i < PARSER_MAX_VARS; i++) {
        if (g_vars[i].in_use) {
            callback(g_vars[i].name, g_vars[i].value, user_data);
        }
    }
}

/**
 * @brief Get count of defined variables
 */
int parser_var_count(void) {
    return g_var_count;
}

/**
 * @brief Clear all environment variables
 */
void parser_clear_vars(void) {
    memset(g_vars, 0, sizeof(g_vars));
    g_var_count = 0;
}

/* ============================================================================
 * Variable Expansion
 * ============================================================================ */

/**
 * @brief Check if character is valid in a variable name
 */
static int is_var_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

/**
 * @brief Expand variables in a string
 * 
 * Handles both $VAR and ${VAR} syntax.
 * Variables inside single quotes are not expanded.
 * Variables inside double quotes are expanded.
 */
int parser_expand_vars(const char *input, char *output, size_t output_size) {
    if (input == NULL || output == NULL || output_size == 0) {
        return -1;
    }
    
    const char *src = input;
    char *dst = output;
    char *dst_end = output + output_size - 1;  /* Leave room for null terminator */
    
    int in_single_quote = 0;
    
    while (*src && dst < dst_end) {
        /* Handle single quotes - no expansion inside */
        if (*src == '\'' && !in_single_quote) {
            in_single_quote = 1;
            *dst++ = *src++;
            continue;
        }
        
        if (*src == '\'' && in_single_quote) {
            in_single_quote = 0;
            *dst++ = *src++;
            continue;
        }
        
        /* Skip variable expansion inside single quotes */
        if (in_single_quote) {
            *dst++ = *src++;
            continue;
        }
        
        /* Check for variable reference */
        if (*src == '$') {
            src++;  /* Skip the $ */
            
            if (*src == '\0') {
                /* Lone $ at end of string - keep it */
                if (dst < dst_end) *dst++ = '$';
                break;
            }
            
            /* Check for ${VAR} syntax */
            int braced = (*src == '{');
            if (braced) {
                src++;  /* Skip the { */
            }
            
            /* Extract variable name */
            char var_name[PARSER_MAX_VAR_NAME];
            int name_len = 0;
            
            while (*src && name_len < PARSER_MAX_VAR_NAME - 1) {
                if (braced) {
                    /* ${VAR} - stop at } */
                    if (*src == '}') {
                        src++;  /* Skip the } */
                        break;
                    }
                } else {
                    /* $VAR - stop at non-var character */
                    if (!is_var_char(*src)) {
                        break;
                    }
                }
                var_name[name_len++] = *src++;
            }
            var_name[name_len] = '\0';
            
            /* Look up variable */
            if (name_len > 0) {
                const char *value = parser_getvar(var_name);
                if (value != NULL) {
                    /* Copy value to output */
                    while (*value && dst < dst_end) {
                        *dst++ = *value++;
                    }
                }
                /* If variable not found, expand to empty string */
            } else {
                /* Empty variable name like ${ } - just skip */
            }
            
            continue;
        }
        
        /* Regular character - copy it */
        *dst++ = *src++;
    }
    
    *dst = '\0';
    return (int)(dst - output);
}

/* ============================================================================
 * Assignment Detection
 * ============================================================================ */

/**
 * @brief Check if a line is a variable assignment
 */
int parser_is_assignment(const char *line, char *name, char *value) {
    if (line == NULL || name == NULL || value == NULL) {
        return 0;
    }
    
    /* Skip leading whitespace */
    while (*line && isspace((unsigned char)*line)) {
        line++;
    }
    
    /* Check for valid variable name start */
    if (!isalpha((unsigned char)*line) && *line != '_') {
        return 0;
    }
    
    /* Extract variable name */
    const char *start = line;
    while (*line && is_var_char(*line)) {
        line++;
    }
    
    /* Must have = immediately after name */
    if (*line != '=') {
        return 0;
    }
    
    /* Copy name */
    size_t name_len = line - start;
    if (name_len >= PARSER_MAX_VAR_NAME) {
        return 0;
    }
    memcpy(name, start, name_len);
    name[name_len] = '\0';
    
    /* Skip the = */
    line++;
    
    /* Copy value (rest of line, trimmed) */
    /* Handle quoted values */
    if (*line == '"' || *line == '\'') {
        char quote = *line++;
        start = line;
        while (*line && *line != quote) {
            line++;
        }
        size_t value_len = line - start;
        if (value_len >= PARSER_MAX_VAR_VALUE) {
            value_len = PARSER_MAX_VAR_VALUE - 1;
        }
        memcpy(value, start, value_len);
        value[value_len] = '\0';
    } else {
        /* Unquoted value - take until whitespace or end */
        start = line;
        while (*line && !isspace((unsigned char)*line)) {
            line++;
        }
        size_t value_len = line - start;
        if (value_len >= PARSER_MAX_VAR_VALUE) {
            value_len = PARSER_MAX_VAR_VALUE - 1;
        }
        memcpy(value, start, value_len);
        value[value_len] = '\0';
    }
    
    return 1;
}

/* ============================================================================
 * Main Parser
 * ============================================================================ */

/**
 * @brief Remove comments from line
 * 
 * Removes everything from # to end of line, unless # is quoted.
 * 
 * @param line Input/output line (modified in place)
 */
static void remove_comments(char *line) {
    int in_single = 0;
    int in_double = 0;
    
    for (char *p = line; *p; p++) {
        if (*p == '\'' && !in_double) {
            in_single = !in_single;
        } else if (*p == '"' && !in_single) {
            in_double = !in_double;
        } else if (*p == '#' && !in_single && !in_double) {
            *p = '\0';
            return;
        }
    }
}

/**
 * @brief Parse a command line into tokens
 */
parser_error_t parser_parse_line(const char *line, parser_result_t *result) {
    if (result == NULL) {
        return PARSER_ERR_SYNTAX;
    }
    
    /* Initialize result structure */
    memset(result, 0, sizeof(parser_result_t));
    result->error = PARSER_OK;
    result->stdout_redir = REDIR_NONE;
    result->stdin_redir = REDIR_NONE;
    
    if (line == NULL || *line == '\0') {
        result->error = PARSER_ERR_EMPTY;
        return PARSER_ERR_EMPTY;
    }
    
    /* Check line length */
    size_t line_len = strlen(line);
    if (line_len >= PARSER_MAX_LINE_LEN) {
        result->error = PARSER_ERR_LINE_TOO_LONG;
        result->error_msg = error_messages[PARSER_ERR_LINE_TOO_LONG];
        return PARSER_ERR_LINE_TOO_LONG;
    }
    
    /* Copy line to buffer for modification */
    char work_buf[PARSER_MAX_LINE_LEN];
    strncpy(work_buf, line, sizeof(work_buf) - 1);
    work_buf[sizeof(work_buf) - 1] = '\0';
    
    /* Remove comments */
    remove_comments(work_buf);
    
    /* Expand variables */
    char expanded[PARSER_MAX_LINE_LEN * 2];
    int expand_result = parser_expand_vars(work_buf, expanded, sizeof(expanded));
    if (expand_result < 0) {
        result->error = PARSER_ERR_ARG_TOO_LONG;
        result->error_msg = error_messages[PARSER_ERR_ARG_TOO_LONG];
        return PARSER_ERR_ARG_TOO_LONG;
    }
    
    /* Copy expanded result to result buffer */
    strncpy(result->buffer, expanded, sizeof(result->buffer) - 1);
    result->buffer[sizeof(result->buffer) - 1] = '\0';
    
    /* Tokenize the line */
    char *p = result->buffer;
    int argc = 0;
    
    while (*p && argc < PARSER_MAX_ARGS) {
        /* Skip leading whitespace */
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }
        
        if (!*p) break;
        
        /* Check for output redirection operators */
        if (*p == '>' && *(p+1) == '>') {
            /* Append redirection >> */
            p += 2;
            while (*p && isspace((unsigned char)*p)) p++;
            
            if (!*p) {
                result->error = PARSER_ERR_REDIR_NOFILE;
                result->error_msg = error_messages[PARSER_ERR_REDIR_NOFILE];
                return PARSER_ERR_REDIR_NOFILE;
            }
            
            result->stdout_redir = REDIR_APPEND;
            
            /* Get filename */
            if (*p == '"' || *p == '\'') {
                char quote = *p++;
                result->stdout_file = p;
                while (*p && *p != quote) p++;
                if (*p) *p++ = '\0';
            } else {
                result->stdout_file = p;
                while (*p && !isspace((unsigned char)*p)) p++;
                if (*p) *p++ = '\0';
            }
            continue;
        }
        
        if (*p == '>') {
            /* Output redirection > */
            p++;
            while (*p && isspace((unsigned char)*p)) p++;
            
            if (!*p) {
                result->error = PARSER_ERR_REDIR_NOFILE;
                result->error_msg = error_messages[PARSER_ERR_REDIR_NOFILE];
                return PARSER_ERR_REDIR_NOFILE;
            }
            
            result->stdout_redir = REDIR_OUTPUT;
            
            /* Get filename */
            if (*p == '"' || *p == '\'') {
                char quote = *p++;
                result->stdout_file = p;
                while (*p && *p != quote) p++;
                if (*p) *p++ = '\0';
            } else {
                result->stdout_file = p;
                while (*p && !isspace((unsigned char)*p)) p++;
                if (*p) *p++ = '\0';
            }
            continue;
        }
        
        if (*p == '<') {
            /* Input redirection < */
            p++;
            while (*p && isspace((unsigned char)*p)) p++;
            
            if (!*p) {
                result->error = PARSER_ERR_REDIR_NOFILE;
                result->error_msg = error_messages[PARSER_ERR_REDIR_NOFILE];
                return PARSER_ERR_REDIR_NOFILE;
            }
            
            result->stdin_redir = REDIR_INPUT;
            
            /* Get filename */
            if (*p == '"' || *p == '\'') {
                char quote = *p++;
                result->stdin_file = p;
                while (*p && *p != quote) p++;
                if (*p) *p++ = '\0';
            } else {
                result->stdin_file = p;
                while (*p && !isspace((unsigned char)*p)) p++;
                if (*p) *p++ = '\0';
            }
            continue;
        }
        
        /* Regular argument */
        if (*p == '"') {
            /* Double-quoted string */
            p++;  /* Skip opening quote */
            result->argv[argc++] = p;
            
            while (*p && *p != '"') {
                p++;
            }
            
            if (*p == '"') {
                *p++ = '\0';
            } else {
                result->error = PARSER_ERR_UNCLOSED_QUOTE;
                result->error_msg = error_messages[PARSER_ERR_UNCLOSED_QUOTE];
                return PARSER_ERR_UNCLOSED_QUOTE;
            }
        } else if (*p == '\'') {
            /* Single-quoted string */
            p++;  /* Skip opening quote */
            result->argv[argc++] = p;
            
            while (*p && *p != '\'') {
                p++;
            }
            
            if (*p == '\'') {
                *p++ = '\0';
            } else {
                result->error = PARSER_ERR_UNCLOSED_QUOTE;
                result->error_msg = error_messages[PARSER_ERR_UNCLOSED_QUOTE];
                return PARSER_ERR_UNCLOSED_QUOTE;
            }
        } else {
            /* Unquoted argument */
            result->argv[argc++] = p;
            
            while (*p && !isspace((unsigned char)*p) && *p != '>' && *p != '<') {
                p++;
            }
            
            if (*p && isspace((unsigned char)*p)) {
                *p++ = '\0';
            }
            /* Don't null-terminate if we stopped at > or < - let next iteration handle it */
        }
    }
    
    /* Check for too many arguments */
    if (argc >= PARSER_MAX_ARGS && *p) {
        result->error = PARSER_ERR_TOO_MANY_ARGS;
        result->error_msg = error_messages[PARSER_ERR_TOO_MANY_ARGS];
        return PARSER_ERR_TOO_MANY_ARGS;
    }
    
    /* Null-terminate argv */
    result->argc = argc;
    result->argv[argc] = NULL;
    
    /* Check for empty command after parsing */
    if (argc == 0) {
        result->error = PARSER_ERR_EMPTY;
        return PARSER_ERR_EMPTY;
    }
    
    return PARSER_OK;
}

/**
 * @brief Free resources associated with a parse result
 */
void parser_free_result(parser_result_t *result) {
    /* Currently a no-op since we use static buffers */
    /* But call it for forward compatibility */
    (void)result;
}

/**
 * @brief Get error message for a parser error code
 */
const char* parser_error_string(parser_error_t error) {
    if (error < 0 || error > PARSER_ERR_MEMORY) {
        return "Unknown error";
    }
    return error_messages[error];
}
