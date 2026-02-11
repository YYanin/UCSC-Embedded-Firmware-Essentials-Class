/**
 * @file terminal_esp32.h
 * @brief Terminal I/O Abstraction for ESP32 Serial Console
 * 
 * This header provides a unified terminal interface for the ESP32 shell.
 * It abstracts the differences between:
 * - ESP32: UART/USB serial console with limited terminal capabilities
 * - Linux: Full terminal with termios support
 * 
 * Key Features:
 * - Character-by-character input with escape sequence parsing
 * - Line editing with backspace and cursor movement (limited on ESP32)
 * - Command history navigation with up/down arrows
 * - Configurable terminal width (defaults to 80 columns)
 * - Local echo control for serial terminals
 * 
 * Usage:
 *   terminal_init();
 *   
 *   // Read a line with editing support
 *   char line[256];
 *   int len = terminal_read_line(line, sizeof(line));
 *   
 *   // Write output with optional newline conversion
 *   terminal_write("Hello, World!\n");
 *   
 *   terminal_cleanup();
 */

#ifndef TERMINAL_ESP32_H
#define TERMINAL_ESP32_H

#include <stddef.h>

/* Include centralized configuration - provides TERMINAL_* compatibility macros */
#include "shell_config.h"

/* ============================================================================
 * Terminal Configuration Constants
 * ============================================================================ */

/* Default terminal dimensions - can be overridden */
#ifndef TERMINAL_DEFAULT_WIDTH
#define TERMINAL_DEFAULT_WIDTH  80
#endif

#ifndef TERMINAL_DEFAULT_HEIGHT
#define TERMINAL_DEFAULT_HEIGHT 24
#endif

/* Input buffer sizes */
#define TERMINAL_MAX_LINE_LEN  256
#define TERMINAL_HISTORY_SIZE  10

/* Control characters */
#define TERMINAL_CTRL_A   0x01  /* Move to start of line */
#define TERMINAL_CTRL_C   0x03  /* Cancel/interrupt */
#define TERMINAL_CTRL_D   0x04  /* EOF / delete char */
#define TERMINAL_CTRL_E   0x05  /* Move to end of line */
#define TERMINAL_CTRL_K   0x0B  /* Kill to end of line */
#define TERMINAL_CTRL_L   0x0C  /* Clear screen */
#define TERMINAL_CTRL_U   0x15  /* Kill line */
#define TERMINAL_BACKSPACE 0x7F /* Backspace (DEL) */
#define TERMINAL_BS       0x08  /* Backspace (BS) */
#define TERMINAL_TAB      0x09  /* Tab (for completion) */
#define TERMINAL_CR       0x0D  /* Carriage return */
#define TERMINAL_LF       0x0A  /* Line feed */
#define TERMINAL_ESC      0x1B  /* Escape (start of sequence) */

/* Escape sequence parsing states */
typedef enum {
    ESC_STATE_NORMAL,     /* Normal input mode */
    ESC_STATE_ESC,        /* Received ESC (0x1B) */
    ESC_STATE_CSI,        /* Received ESC [ (CSI) */
    ESC_STATE_SS3         /* Received ESC O (SS3) */
} terminal_esc_state_t;

/* Special key codes (returned by terminal_read_key) */
typedef enum {
    KEY_CHAR = 0,         /* Regular character (value in low byte) */
    KEY_UP = 256,         /* Arrow up */
    KEY_DOWN,             /* Arrow down */
    KEY_LEFT,             /* Arrow left */
    KEY_RIGHT,            /* Arrow right */
    KEY_HOME,             /* Home key */
    KEY_END,              /* End key */
    KEY_DELETE,           /* Delete key */
    KEY_PAGEUP,           /* Page up */
    KEY_PAGEDOWN,         /* Page down */
    KEY_CTRL_A,           /* Ctrl+A */
    KEY_CTRL_C,           /* Ctrl+C */
    KEY_CTRL_D,           /* Ctrl+D */
    KEY_CTRL_E,           /* Ctrl+E */
    KEY_CTRL_K,           /* Ctrl+K */
    KEY_CTRL_L,           /* Ctrl+L */
    KEY_CTRL_U,           /* Ctrl+U */
    KEY_TAB,              /* Tab key */
    KEY_BACKSPACE,        /* Backspace */
    KEY_ENTER,            /* Enter/Return */
    KEY_NONE = -1         /* No key available (non-blocking read) */
} terminal_key_t;

/* ============================================================================
 * Terminal State Structure
 * ============================================================================ */

/**
 * @brief Terminal state tracking structure
 * 
 * Keeps track of line editing state, cursor position, and history.
 */
typedef struct {
    /* Current line being edited */
    char line[TERMINAL_MAX_LINE_LEN];
    size_t line_len;        /* Current line length */
    size_t cursor_pos;      /* Cursor position in line */
    
    /* Command history */
    char history[TERMINAL_HISTORY_SIZE][TERMINAL_MAX_LINE_LEN];
    int history_count;      /* Number of history entries */
    int history_index;      /* Current history navigation position */
    
    /* Terminal dimensions */
    int width;              /* Terminal width in columns */
    int height;             /* Terminal height in rows */
    
    /* Escape sequence parsing */
    terminal_esc_state_t esc_state;
    
    /* Echo control */
    int local_echo;         /* 1 = echo characters locally, 0 = no echo */
    
    /* Saved line for history navigation */
    char saved_line[TERMINAL_MAX_LINE_LEN];
    int has_saved_line;     /* 1 if we have a saved line */
    
} terminal_state_t;

/* ============================================================================
 * Terminal Initialization and Cleanup
 * ============================================================================ */

/**
 * @brief Initialize the terminal subsystem
 * 
 * Sets up UART configuration on ESP32, or raw terminal mode on Linux.
 * Must be called before any other terminal functions.
 */
void terminal_init(void);

/**
 * @brief Clean up terminal resources
 * 
 * Restores terminal settings on Linux. No-op on ESP32.
 */
void terminal_cleanup(void);

/**
 * @brief Set up terminal for raw input mode
 * 
 * Enables character-by-character input without line buffering.
 * On ESP32: Configures UART parameters
 * On Linux: Sets termios to raw mode
 */
void terminal_setup(void);

/**
 * @brief Restore terminal to normal mode
 * 
 * Restores line-buffered input mode.
 * On ESP32: No-op (UART stays in current mode)
 * On Linux: Restores original termios settings
 */
void terminal_restore(void);

/* ============================================================================
 * Terminal Input Functions
 * ============================================================================ */

/**
 * @brief Read a single key from terminal input
 * 
 * Handles escape sequences for arrow keys and other special keys.
 * Non-blocking on ESP32, can be configured on Linux.
 * 
 * @return Key code (KEY_* enum or character value), KEY_NONE if no input
 */
int terminal_read_key(void);

/**
 * @brief Read a line with editing support
 * 
 * Full line editing with:
 * - Backspace and delete
 * - Arrow key navigation (left/right for cursor, up/down for history)
 * - Ctrl+A/E for home/end
 * - Ctrl+U/K for line kill
 * - Ctrl+C for cancel
 * 
 * @param buf Buffer to store the line (null-terminated)
 * @param size Size of buffer
 * @return Number of characters read, 0 on empty line, -1 on cancel/error
 */
int terminal_read_line(char *buf, size_t size);

/**
 * @brief Read a line without editing (simple mode)
 * 
 * Basic line input with only backspace support.
 * Lighter weight than terminal_read_line().
 * 
 * @param buf Buffer to store the line (null-terminated)
 * @param size Size of buffer
 * @return Number of characters read, 0 on empty line, -1 on error
 */
int terminal_read_line_simple(char *buf, size_t size);

/* ============================================================================
 * Terminal Output Functions
 * ============================================================================ */

/**
 * @brief Write a single character to terminal
 * 
 * @param c Character to write
 */
void terminal_write_char(char c);

/**
 * @brief Write a string to terminal
 * 
 * @param str Null-terminated string to write
 */
void terminal_write(const char *str);

/**
 * @brief Write a string with LF -> CRLF conversion
 * 
 * Converts '\n' to '\r\n' for serial terminals that require it.
 * 
 * @param str Null-terminated string to write
 */
void terminal_write_crlf(const char *str);

/**
 * @brief Printf-style formatted output
 * 
 * @param fmt Format string
 * @param ... Arguments
 * @return Number of characters written
 */
int terminal_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/**
 * @brief Flush terminal output buffer
 * 
 * Ensures all pending output is written.
 */
void terminal_flush(void);

/* ============================================================================
 * Terminal Control Functions
 * ============================================================================ */

/**
 * @brief Clear the terminal screen
 * 
 * Sends ANSI clear screen sequence and moves cursor to home.
 */
void terminal_clear_screen(void);

/**
 * @brief Clear from cursor to end of line
 */
void terminal_clear_to_eol(void);

/**
 * @brief Move cursor to beginning of line
 */
void terminal_cursor_home(void);

/**
 * @brief Move cursor left by n positions
 * 
 * @param n Number of positions to move
 */
void terminal_cursor_left(int n);

/**
 * @brief Move cursor right by n positions
 * 
 * @param n Number of positions to move
 */
void terminal_cursor_right(int n);

/**
 * @brief Ring the terminal bell
 */
void terminal_bell(void);

/* ============================================================================
 * Terminal Settings
 * ============================================================================ */

/**
 * @brief Set terminal width
 * 
 * @param width Terminal width in columns
 */
void terminal_set_width(int width);

/**
 * @brief Set terminal height
 * 
 * @param height Terminal height in rows
 */
void terminal_set_height(int height);

/**
 * @brief Get terminal width
 * 
 * @return Terminal width in columns
 */
int terminal_get_width(void);

/**
 * @brief Get terminal height
 * 
 * @return Terminal height in rows
 */
int terminal_get_height(void);

/**
 * @brief Enable or disable local echo
 * 
 * @param enable 1 to enable local echo, 0 to disable
 */
void terminal_set_echo(int enable);

/* ============================================================================
 * Command History
 * ============================================================================ */

/**
 * @brief Add a command to history
 * 
 * @param cmd Command string to add
 */
void terminal_history_add(const char *cmd);

/**
 * @brief Get a command from history
 * 
 * @param index History index (0 = most recent)
 * @return Command string, or NULL if index out of range
 */
const char* terminal_history_get(int index);

/**
 * @brief Get number of history entries
 * 
 * @return Number of commands in history
 */
int terminal_history_count(void);

/**
 * @brief Clear command history
 */
void terminal_history_clear(void);

/* ============================================================================
 * Global Terminal State
 * ============================================================================ */

/**
 * @brief Get pointer to global terminal state
 * 
 * Allows direct access to terminal state for advanced usage.
 * 
 * @return Pointer to terminal state structure
 */
terminal_state_t* terminal_get_state(void);

#endif /* TERMINAL_ESP32_H */
