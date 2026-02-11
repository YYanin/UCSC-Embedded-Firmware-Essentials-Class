/**
 * @file terminal_esp32.c
 * @brief Terminal I/O Implementation for ESP32 Serial Console
 * 
 * This file implements terminal handling for the ESP32 shell, providing:
 * - Character-by-character input via UART
 * - Escape sequence parsing for arrow keys and special keys
 * - Line editing with cursor movement and history navigation
 * - ANSI escape code output for cursor control
 * 
 * On ESP32, terminal capabilities are limited compared to a full terminal:
 * - No automatic terminal size detection (uses default 80x24)
 * - Some terminal emulators may not support all escape sequences
 * - Local echo must be handled by the shell (not the terminal)
 */

#include "terminal_esp32.h"
#include "platform.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"

static const char *TAG = "terminal";
#endif

/* ============================================================================
 * Global Terminal State
 * ============================================================================ */

/* Global terminal state - initialized to defaults */
static terminal_state_t g_terminal = {
    .line = {0},
    .line_len = 0,
    .cursor_pos = 0,
    .history = {{0}},
    .history_count = 0,
    .history_index = -1,
    .width = TERMINAL_DEFAULT_WIDTH,
    .height = TERMINAL_DEFAULT_HEIGHT,
    .esc_state = ESC_STATE_NORMAL,
    .local_echo = 1,
    .saved_line = {0},
    .has_saved_line = 0
};

/* ============================================================================
 * Terminal Initialization and Cleanup
 * ============================================================================ */

/**
 * @brief Initialize the terminal subsystem
 * 
 * Initializes terminal state and sets up platform-specific I/O.
 */
void terminal_init(void) {
    /* Reset terminal state */
    memset(&g_terminal, 0, sizeof(g_terminal));
    g_terminal.width = TERMINAL_DEFAULT_WIDTH;
    g_terminal.height = TERMINAL_DEFAULT_HEIGHT;
    g_terminal.local_echo = 1;
    g_terminal.history_index = -1;
    g_terminal.esc_state = ESC_STATE_NORMAL;
    
#ifdef ESP_PLATFORM
    ESP_LOGI(TAG, "Terminal initialized (ESP32 serial console)");
    ESP_LOGI(TAG, "Terminal size: %dx%d", g_terminal.width, g_terminal.height);
#endif
    
    /* Set up terminal for raw input */
    terminal_setup();
}

/**
 * @brief Clean up terminal resources
 */
void terminal_cleanup(void) {
    terminal_restore();
    
#ifdef ESP_PLATFORM
    ESP_LOGI(TAG, "Terminal cleanup complete");
#endif
}

/**
 * @brief Set up terminal for raw input mode
 * 
 * ESP32: UART is already in raw mode, nothing special needed
 * Linux: Would set termios to raw mode (not implemented here)
 */
void terminal_setup(void) {
#ifdef ESP_PLATFORM
    /* ESP32 UART is already configured by platform_init() */
    /* Nothing special needed here */
#else
    /* Linux: Would save and modify termios settings */
    /* This is handled by platform layer or unified-shell terminal.c */
#endif
}

/**
 * @brief Restore terminal to normal mode
 */
void terminal_restore(void) {
#ifdef ESP_PLATFORM
    /* ESP32: Nothing to restore */
#else
    /* Linux: Would restore termios settings */
#endif
}

/* ============================================================================
 * Low-Level I/O Helpers
 * ============================================================================ */

/**
 * @brief Read a single raw character from input
 * 
 * Non-blocking read that returns -1 if no character available.
 * 
 * @return Character (0-255) or -1 if none available
 */
static int read_raw_char(void) {
    return platform_read_char();
}

/**
 * @brief Write a single character to output
 */
static void write_char(char c) {
    platform_write_char(c);
}

/**
 * @brief Write a string to output
 */
static void write_string(const char *s) {
    platform_write_string(s);
}

/* ============================================================================
 * Escape Sequence Parsing
 * ============================================================================ */

/**
 * @brief Process an escape sequence and return the corresponding key code
 * 
 * Handles ANSI/VT100 escape sequences:
 * - ESC [ A = Up arrow
 * - ESC [ B = Down arrow
 * - ESC [ C = Right arrow
 * - ESC [ D = Left arrow
 * - ESC [ H = Home
 * - ESC [ F = End
 * - ESC [ 3 ~ = Delete
 * - ESC O A-D = Arrow keys (alternative)
 * 
 * @param c Current character in the escape sequence
 * @return Key code if sequence complete, KEY_NONE if more chars needed
 */
static int process_escape_sequence(int c) {
    switch (g_terminal.esc_state) {
        case ESC_STATE_NORMAL:
            /* Should not get here */
            return KEY_NONE;
            
        case ESC_STATE_ESC:
            /* We received ESC, now check next char */
            if (c == '[') {
                g_terminal.esc_state = ESC_STATE_CSI;
                return KEY_NONE;
            } else if (c == 'O') {
                g_terminal.esc_state = ESC_STATE_SS3;
                return KEY_NONE;
            } else {
                /* Unknown sequence, return ESC and put char back */
                g_terminal.esc_state = ESC_STATE_NORMAL;
                /* Just ignore unknown sequences on ESP32 */
                return KEY_NONE;
            }
            
        case ESC_STATE_CSI:
            /* CSI sequence: ESC [ followed by command char */
            g_terminal.esc_state = ESC_STATE_NORMAL;
            switch (c) {
                case 'A': return KEY_UP;
                case 'B': return KEY_DOWN;
                case 'C': return KEY_RIGHT;
                case 'D': return KEY_LEFT;
                case 'H': return KEY_HOME;
                case 'F': return KEY_END;
                case '3':
                    /* Could be Delete key (ESC [ 3 ~) - need one more char */
                    /* Simplified: just return delete */
                    return KEY_DELETE;
                case '5': return KEY_PAGEUP;
                case '6': return KEY_PAGEDOWN;
                default:
                    /* Unknown CSI sequence */
                    return KEY_NONE;
            }
            
        case ESC_STATE_SS3:
            /* SS3 sequence: ESC O followed by command char */
            /* Used by some terminals for arrow keys and function keys */
            g_terminal.esc_state = ESC_STATE_NORMAL;
            switch (c) {
                case 'A': return KEY_UP;
                case 'B': return KEY_DOWN;
                case 'C': return KEY_RIGHT;
                case 'D': return KEY_LEFT;
                case 'H': return KEY_HOME;
                case 'F': return KEY_END;
                default:
                    return KEY_NONE;
            }
    }
    
    return KEY_NONE;
}

/* ============================================================================
 * Terminal Input Functions
 * ============================================================================ */

/**
 * @brief Read a single key from terminal input
 * 
 * Handles escape sequences for special keys.
 * 
 * @return Key code (KEY_* enum), KEY_NONE if no input available
 */
int terminal_read_key(void) {
    int c = read_raw_char();
    
    if (c < 0) {
        return KEY_NONE;
    }
    
    /* Check if we're in the middle of an escape sequence */
    if (g_terminal.esc_state != ESC_STATE_NORMAL) {
        return process_escape_sequence(c);
    }
    
    /* Process normal characters */
    switch (c) {
        case TERMINAL_ESC:
            g_terminal.esc_state = ESC_STATE_ESC;
            return KEY_NONE;  /* Need more chars */
            
        case TERMINAL_CTRL_A:
            return KEY_CTRL_A;
            
        case TERMINAL_CTRL_C:
            return KEY_CTRL_C;
            
        case TERMINAL_CTRL_D:
            return KEY_CTRL_D;
            
        case TERMINAL_CTRL_E:
            return KEY_CTRL_E;
            
        case TERMINAL_CTRL_K:
            return KEY_CTRL_K;
            
        case TERMINAL_CTRL_L:
            return KEY_CTRL_L;
            
        case TERMINAL_CTRL_U:
            return KEY_CTRL_U;
            
        case TERMINAL_TAB:
            return KEY_TAB;
            
        case TERMINAL_BACKSPACE:
        case TERMINAL_BS:
            return KEY_BACKSPACE;
            
        case TERMINAL_CR:
        case TERMINAL_LF:
            return KEY_ENTER;
            
        default:
            /* Regular printable character */
            if (c >= 32 && c < 127) {
                return c;  /* Return character value directly */
            }
            /* Ignore other control characters */
            return KEY_NONE;
    }
}

/**
 * @brief Refresh the display after editing
 * 
 * Redraws the current line from the cursor position.
 */
static void refresh_line(void) {
    /* Clear from cursor to end of line */
    write_string("\x1b[K");
    
    /* Write remaining characters */
    for (size_t i = g_terminal.cursor_pos; i < g_terminal.line_len; i++) {
        write_char(g_terminal.line[i]);
    }
    
    /* Move cursor back to correct position */
    int chars_to_left = g_terminal.line_len - g_terminal.cursor_pos;
    if (chars_to_left > 0) {
        char buf[16];
        snprintf(buf, sizeof(buf), "\x1b[%dD", chars_to_left);
        write_string(buf);
    }
    
    platform_flush();
}

/**
 * @brief Redraw entire line (for history navigation)
 * 
 * Clears current line and draws new content.
 */
static void redraw_line(void) {
    /* Move to start of line (carriage return) */
    write_char('\r');
    
    /* Clear entire line */
    write_string("\x1b[K");
    
    /* Write prompt - use the same prompt as esp_shell */
    write_string("esp32> ");
    
    /* Write line content */
    write_string(g_terminal.line);
    
    /* Move cursor to correct position */
    int chars_to_left = g_terminal.line_len - g_terminal.cursor_pos;
    if (chars_to_left > 0) {
        char buf[16];
        snprintf(buf, sizeof(buf), "\x1b[%dD", chars_to_left);
        write_string(buf);
    }
    
    platform_flush();
}

/**
 * @brief Navigate up in history
 */
static void history_up(void) {
    if (g_terminal.history_count == 0) {
        terminal_bell();
        return;
    }
    
    /* Save current line if this is the first navigation */
    if (!g_terminal.has_saved_line) {
        strncpy(g_terminal.saved_line, g_terminal.line, TERMINAL_MAX_LINE_LEN - 1);
        g_terminal.saved_line[TERMINAL_MAX_LINE_LEN - 1] = '\0';
        g_terminal.has_saved_line = 1;
        g_terminal.history_index = -1;
    }
    
    /* Move up in history */
    if (g_terminal.history_index < g_terminal.history_count - 1) {
        g_terminal.history_index++;
        
        /* Copy history entry to current line */
        const char *hist = g_terminal.history[g_terminal.history_index];
        strncpy(g_terminal.line, hist, TERMINAL_MAX_LINE_LEN - 1);
        g_terminal.line[TERMINAL_MAX_LINE_LEN - 1] = '\0';
        g_terminal.line_len = strlen(g_terminal.line);
        g_terminal.cursor_pos = g_terminal.line_len;
        
        redraw_line();
    } else {
        terminal_bell();
    }
}

/**
 * @brief Navigate down in history
 */
static void history_down(void) {
    if (!g_terminal.has_saved_line) {
        terminal_bell();
        return;
    }
    
    if (g_terminal.history_index > 0) {
        g_terminal.history_index--;
        
        /* Copy history entry to current line */
        const char *hist = g_terminal.history[g_terminal.history_index];
        strncpy(g_terminal.line, hist, TERMINAL_MAX_LINE_LEN - 1);
        g_terminal.line[TERMINAL_MAX_LINE_LEN - 1] = '\0';
        g_terminal.line_len = strlen(g_terminal.line);
        g_terminal.cursor_pos = g_terminal.line_len;
        
        redraw_line();
    } else if (g_terminal.history_index == 0) {
        /* Return to saved line */
        g_terminal.history_index = -1;
        strncpy(g_terminal.line, g_terminal.saved_line, TERMINAL_MAX_LINE_LEN - 1);
        g_terminal.line[TERMINAL_MAX_LINE_LEN - 1] = '\0';
        g_terminal.line_len = strlen(g_terminal.line);
        g_terminal.cursor_pos = g_terminal.line_len;
        g_terminal.has_saved_line = 0;
        
        redraw_line();
    } else {
        terminal_bell();
    }
}

/**
 * @brief Read a line with full editing support
 * 
 * @param buf Buffer to store the line
 * @param size Size of buffer
 * @return Number of characters, 0 on empty, -1 on cancel
 */
int terminal_read_line(char *buf, size_t size) {
    /* Reset line state */
    g_terminal.line[0] = '\0';
    g_terminal.line_len = 0;
    g_terminal.cursor_pos = 0;
    g_terminal.history_index = -1;
    g_terminal.has_saved_line = 0;
    g_terminal.esc_state = ESC_STATE_NORMAL;
    
    while (1) {
        int key = terminal_read_key();
        
        if (key == KEY_NONE) {
            /* No input available, yield to other tasks */
#ifdef ESP_PLATFORM
            vTaskDelay(pdMS_TO_TICKS(10));
#endif
            continue;
        }
        
        switch (key) {
            case KEY_ENTER:
                /* End of line - write CRLF for proper terminal display */
                write_string("\r\n");
                platform_flush();
                
                /* Consume any trailing CR or LF from CRLF sequence
                 * Some terminals send CR+LF, we need to skip the second char */
                {
                    int next = platform_read_char();
                    if (next >= 0 && next != '\r' && next != '\n') {
                        /* Got a real character, can't push back on ESP32
                         * This shouldn't happen in practice */
                    }
                    /* If we got CR or LF, just discard it */
                }
                
                /* Copy line to output buffer */
                strncpy(buf, g_terminal.line, size - 1);
                buf[size - 1] = '\0';
                return g_terminal.line_len;
                
            case KEY_CTRL_C:
                /* Cancel - return empty line with indicator */
                write_string("^C\r\n");
                platform_flush();
                buf[0] = '\0';
                return -1;
                
            case KEY_BACKSPACE:
                /* Delete character before cursor */
                if (g_terminal.cursor_pos > 0) {
                    g_terminal.cursor_pos--;
                    
                    /* Shift characters left */
                    memmove(&g_terminal.line[g_terminal.cursor_pos],
                            &g_terminal.line[g_terminal.cursor_pos + 1],
                            g_terminal.line_len - g_terminal.cursor_pos);
                    g_terminal.line_len--;
                    g_terminal.line[g_terminal.line_len] = '\0';
                    
                    /* Update display */
                    write_string("\b");  /* Move cursor left */
                    refresh_line();
                } else {
                    terminal_bell();
                }
                break;
                
            case KEY_DELETE:
            case KEY_CTRL_D:
                /* Delete character at cursor */
                if (g_terminal.cursor_pos < g_terminal.line_len) {
                    /* Shift characters left */
                    memmove(&g_terminal.line[g_terminal.cursor_pos],
                            &g_terminal.line[g_terminal.cursor_pos + 1],
                            g_terminal.line_len - g_terminal.cursor_pos);
                    g_terminal.line_len--;
                    g_terminal.line[g_terminal.line_len] = '\0';
                    
                    refresh_line();
                } else if (key == KEY_CTRL_D && g_terminal.line_len == 0) {
                    /* Ctrl+D on empty line = EOF */
                    write_string("^D\r\n");
                    platform_flush();
                    buf[0] = '\0';
                    return -1;
                } else {
                    terminal_bell();
                }
                break;
                
            case KEY_LEFT:
                /* Move cursor left */
                if (g_terminal.cursor_pos > 0) {
                    g_terminal.cursor_pos--;
                    write_string("\x1b[D");  /* ANSI cursor left */
                } else {
                    terminal_bell();
                }
                break;
                
            case KEY_RIGHT:
                /* Move cursor right */
                if (g_terminal.cursor_pos < g_terminal.line_len) {
                    g_terminal.cursor_pos++;
                    write_string("\x1b[C");  /* ANSI cursor right */
                } else {
                    terminal_bell();
                }
                break;
                
            case KEY_UP:
                /* History up */
                history_up();
                break;
                
            case KEY_DOWN:
                /* History down */
                history_down();
                break;
                
            case KEY_HOME:
            case KEY_CTRL_A:
                /* Move to start of line */
                if (g_terminal.cursor_pos > 0) {
                    terminal_cursor_left(g_terminal.cursor_pos);
                    g_terminal.cursor_pos = 0;
                }
                break;
                
            case KEY_END:
            case KEY_CTRL_E:
                /* Move to end of line */
                if (g_terminal.cursor_pos < g_terminal.line_len) {
                    int move = g_terminal.line_len - g_terminal.cursor_pos;
                    terminal_cursor_right(move);
                    g_terminal.cursor_pos = g_terminal.line_len;
                }
                break;
                
            case KEY_CTRL_U:
                /* Kill entire line */
                if (g_terminal.line_len > 0) {
                    /* Move to start */
                    if (g_terminal.cursor_pos > 0) {
                        terminal_cursor_left(g_terminal.cursor_pos);
                    }
                    /* Clear line */
                    g_terminal.line[0] = '\0';
                    g_terminal.line_len = 0;
                    g_terminal.cursor_pos = 0;
                    terminal_clear_to_eol();
                }
                break;
                
            case KEY_CTRL_K:
                /* Kill from cursor to end of line */
                if (g_terminal.cursor_pos < g_terminal.line_len) {
                    g_terminal.line[g_terminal.cursor_pos] = '\0';
                    g_terminal.line_len = g_terminal.cursor_pos;
                    terminal_clear_to_eol();
                }
                break;
                
            case KEY_CTRL_L:
                /* Clear screen and redraw */
                terminal_clear_screen();
                write_string("esp32> ");
                write_string(g_terminal.line);
                /* Move cursor to correct position */
                if (g_terminal.cursor_pos < g_terminal.line_len) {
                    int move = g_terminal.line_len - g_terminal.cursor_pos;
                    terminal_cursor_left(move);
                }
                break;
                
            case KEY_TAB:
                /* Tab completion - just beep for now */
                /* TODO: Implement command completion */
                terminal_bell();
                break;
                
            default:
                /* Regular printable character */
                if (key >= 32 && key < 127) {
                    if (g_terminal.line_len < TERMINAL_MAX_LINE_LEN - 1) {
                        /* Insert character at cursor position */
                        if (g_terminal.cursor_pos < g_terminal.line_len) {
                            /* Shift characters right to make room */
                            memmove(&g_terminal.line[g_terminal.cursor_pos + 1],
                                    &g_terminal.line[g_terminal.cursor_pos],
                                    g_terminal.line_len - g_terminal.cursor_pos + 1);
                        }
                        
                        g_terminal.line[g_terminal.cursor_pos] = (char)key;
                        g_terminal.cursor_pos++;
                        g_terminal.line_len++;
                        g_terminal.line[g_terminal.line_len] = '\0';
                        
                        /* Echo character */
                        if (g_terminal.local_echo) {
                            write_char((char)key);
                            
                            /* If we inserted in the middle, refresh rest of line */
                            if (g_terminal.cursor_pos < g_terminal.line_len) {
                                refresh_line();
                            }
                        }
                    } else {
                        terminal_bell();  /* Line too long */
                    }
                }
                break;
        }
    }
    
    /* Should not reach here */
    return -1;
}

/**
 * @brief Read a line without editing (simple mode)
 * 
 * @param buf Buffer to store the line
 * @param size Size of buffer
 * @return Number of characters, 0 on empty, -1 on error
 */
int terminal_read_line_simple(char *buf, size_t size) {
    size_t pos = 0;
    
    while (pos < size - 1) {
        int c = read_raw_char();
        
        if (c < 0) {
            /* No character available, yield to other tasks */
#ifdef ESP_PLATFORM
            vTaskDelay(pdMS_TO_TICKS(10));
#endif
            continue;
        }
        
        if (c == '\n' || c == '\r') {
            /* End of line */
            write_char('\n');
            buf[pos] = '\0';
            return pos;
        }
        
        if (c == 0x7F || c == '\b') {
            /* Backspace */
            if (pos > 0) {
                pos--;
                write_string("\b \b");  /* Erase character on screen */
            }
            continue;
        }
        
        if (c == 0x03) {
            /* Ctrl+C */
            write_string("^C\n");
            buf[0] = '\0';
            return -1;
        }
        
        if (c >= 32 && c < 127) {
            /* Printable character */
            buf[pos++] = (char)c;
            if (g_terminal.local_echo) {
                write_char((char)c);
            }
        }
    }
    
    buf[pos] = '\0';
    return pos;
}

/* ============================================================================
 * Terminal Output Functions
 * ============================================================================ */

/**
 * @brief Write a single character to terminal
 */
void terminal_write_char(char c) {
    write_char(c);
}

/**
 * @brief Write a string to terminal
 */
void terminal_write(const char *str) {
    write_string(str);
}

/**
 * @brief Write a string with LF -> CRLF conversion
 */
void terminal_write_crlf(const char *str) {
    while (*str) {
        if (*str == '\n') {
            write_char('\r');
        }
        write_char(*str++);
    }
}

/**
 * @brief Printf-style formatted output
 */
int terminal_printf(const char *fmt, ...) {
    char buf[256];
    va_list args;
    
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    if (len > 0) {
        write_string(buf);
    }
    
    return len;
}

/**
 * @brief Flush terminal output buffer
 */
void terminal_flush(void) {
    platform_flush();
}

/* ============================================================================
 * Terminal Control Functions
 * ============================================================================ */

/**
 * @brief Clear the terminal screen
 */
void terminal_clear_screen(void) {
    write_string("\x1b[2J");   /* Clear entire screen */
    write_string("\x1b[H");    /* Move cursor to home */
}

/**
 * @brief Clear from cursor to end of line
 */
void terminal_clear_to_eol(void) {
    write_string("\x1b[K");
}

/**
 * @brief Move cursor to beginning of line
 */
void terminal_cursor_home(void) {
    write_char('\r');
}

/**
 * @brief Move cursor left by n positions
 */
void terminal_cursor_left(int n) {
    if (n > 0) {
        char buf[16];
        snprintf(buf, sizeof(buf), "\x1b[%dD", n);
        write_string(buf);
    }
}

/**
 * @brief Move cursor right by n positions
 */
void terminal_cursor_right(int n) {
    if (n > 0) {
        char buf[16];
        snprintf(buf, sizeof(buf), "\x1b[%dC", n);
        write_string(buf);
    }
}

/**
 * @brief Ring the terminal bell
 */
void terminal_bell(void) {
    write_char('\a');  /* ASCII BEL character */
}

/* ============================================================================
 * Terminal Settings
 * ============================================================================ */

void terminal_set_width(int width) {
    if (width > 0) {
        g_terminal.width = width;
    }
}

void terminal_set_height(int height) {
    if (height > 0) {
        g_terminal.height = height;
    }
}

int terminal_get_width(void) {
    return g_terminal.width;
}

int terminal_get_height(void) {
    return g_terminal.height;
}

void terminal_set_echo(int enable) {
    g_terminal.local_echo = enable ? 1 : 0;
}

/* ============================================================================
 * Command History
 * ============================================================================ */

/**
 * @brief Add a command to history
 */
void terminal_history_add(const char *cmd) {
    if (cmd == NULL || cmd[0] == '\0') {
        return;  /* Don't add empty commands */
    }
    
    /* Don't add duplicate of most recent command */
    if (g_terminal.history_count > 0 &&
        strcmp(cmd, g_terminal.history[0]) == 0) {
        return;
    }
    
    /* Shift existing history down */
    if (g_terminal.history_count < TERMINAL_HISTORY_SIZE) {
        g_terminal.history_count++;
    }
    
    for (int i = g_terminal.history_count - 1; i > 0; i--) {
        strncpy(g_terminal.history[i], g_terminal.history[i - 1],
                TERMINAL_MAX_LINE_LEN - 1);
        g_terminal.history[i][TERMINAL_MAX_LINE_LEN - 1] = '\0';
    }
    
    /* Add new command at position 0 */
    strncpy(g_terminal.history[0], cmd, TERMINAL_MAX_LINE_LEN - 1);
    g_terminal.history[0][TERMINAL_MAX_LINE_LEN - 1] = '\0';
}

/**
 * @brief Get a command from history
 */
const char* terminal_history_get(int index) {
    if (index < 0 || index >= g_terminal.history_count) {
        return NULL;
    }
    return g_terminal.history[index];
}

/**
 * @brief Get number of history entries
 */
int terminal_history_count(void) {
    return g_terminal.history_count;
}

/**
 * @brief Clear command history
 */
void terminal_history_clear(void) {
    g_terminal.history_count = 0;
    g_terminal.history_index = -1;
}

/* ============================================================================
 * Global Terminal State Access
 * ============================================================================ */

/**
 * @brief Get pointer to global terminal state
 */
terminal_state_t* terminal_get_state(void) {
    return &g_terminal;
}
