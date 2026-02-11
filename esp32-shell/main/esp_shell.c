/**
 * @file esp_shell.c
 * @brief ESP32 Shell - Shell Implementation
 * 
 * This file implements the ESP32-specific shell functionality.
 * It provides a simple command-line interface that reads input from
 * the serial console and executes built-in commands.
 * 
 * File operations use the VFS abstraction layer (shell_vfs.h) which
 * provides a portable interface for both ESP32 and Linux.
 * 
 * Execution uses direct function calls (no fork/exec) with I/O
 * redirection support via executor_esp32.h.
 * 
 * Terminal I/O is handled by the terminal_esp32 module which provides
 * line editing, history navigation, and escape sequence parsing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "esp_shell.h"
#include "platform.h"
#include "shell_vfs.h"
#include "executor_esp32.h"
#include "terminal_esp32.h"
#include "parser_esp32.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "driver/gpio.h"

/* Tag for ESP logging */
static const char *TAG = "shell";

/* ============================================================================
 * Shell State Variables
 * ============================================================================ */

/* Current working directory */
static char current_dir[ESP_SHELL_MAX_PATH] = "/spiffs";

/* Command line buffer */
static char line_buffer[ESP_SHELL_MAX_LINE_LEN];

/* Command history buffer */
static char history[ESP_SHELL_HISTORY_SIZE][ESP_SHELL_MAX_LINE_LEN];
static int history_count = 0;
static int history_index = 0;

/* ============================================================================
 * Forward Declarations of Built-in Commands
 * ============================================================================ */
static int cmd_help(int argc, char **argv);
static int cmd_exit(int argc, char **argv);
static int cmd_reboot(int argc, char **argv);
static int cmd_info(int argc, char **argv);
static int cmd_free(int argc, char **argv);
static int cmd_uptime(int argc, char **argv);
static int cmd_pwd(int argc, char **argv);
static int cmd_cd(int argc, char **argv);
static int cmd_ls(int argc, char **argv);
static int cmd_cat(int argc, char **argv);
static int cmd_echo(int argc, char **argv);
static int cmd_touch(int argc, char **argv);
static int cmd_rm(int argc, char **argv);
static int cmd_mkdir(int argc, char **argv);
static int cmd_history(int argc, char **argv);
static int cmd_set(int argc, char **argv);
static int cmd_unset(int argc, char **argv);
static int cmd_env(int argc, char **argv);
static int cmd_jobs(int argc, char **argv);
static int cmd_fg(int argc, char **argv);
static int cmd_bg(int argc, char **argv);
static int cmd_gpio(int argc, char **argv);
static int cmd_format(int argc, char **argv);
static int cmd_fsinfo(int argc, char **argv);

/* ============================================================================
 * Built-in Command Table
 * ============================================================================ */

/* Array of built-in commands - add new commands here */
static const esp_shell_cmd_t builtin_commands[] = {
    /* Shell control commands */
    {"help",    "Show available commands",       cmd_help},
    {"exit",    "Exit shell (reboot ESP32)",     cmd_exit},
    {"reboot",  "Reboot the ESP32",              cmd_reboot},
    
    /* System information commands */
    {"info",    "Show system information",       cmd_info},
    {"free",    "Show free memory",              cmd_free},
    {"uptime",  "Show system uptime",            cmd_uptime},
    {"gpio",    "Control GPIO pins",             cmd_gpio},
    
    /* Directory commands */
    {"pwd",     "Print working directory",       cmd_pwd},
    {"cd",      "Change directory",              cmd_cd},
    {"ls",      "List directory contents",       cmd_ls},
    
    /* File commands */
    {"cat",     "Display file contents",         cmd_cat},
    {"echo",    "Print text",                    cmd_echo},
    {"touch",   "Create empty file",             cmd_touch},
    {"rm",      "Remove file",                   cmd_rm},
    {"mkdir",   "Create directory",              cmd_mkdir},
    
    /* Filesystem management */
    {"fsinfo",  "Show filesystem info",          cmd_fsinfo},
    {"format",  "Format the filesystem",         cmd_format},
    
    /* Shell history */
    {"history", "Show command history",          cmd_history},
    
    /* Environment variables */
    {"set",     "Set environment variable",      cmd_set},
    {"unset",   "Remove environment variable",   cmd_unset},
    {"env",     "List environment variables",    cmd_env},
    
    /* Unavailable features (show helpful error) */
    {"jobs",    "List background jobs (N/A)",    cmd_jobs},
    {"fg",      "Foreground job (N/A)",          cmd_fg},
    {"bg",      "Background job (N/A)",          cmd_bg},
    
    /* End of command list marker */
    {NULL, NULL, NULL}
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Build absolute path from relative path
 * 
 * @param path Input path (may be relative or absolute)
 * @param buf Output buffer for absolute path
 * @param size Size of output buffer
 * @return Pointer to buf on success, NULL on failure
 */
static char* build_path(const char *path, char *buf, size_t size) {
    /* Handle "." - current directory */
    if (strcmp(path, ".") == 0) {
        strncpy(buf, current_dir, size - 1);
        buf[size - 1] = '\0';
        return buf;
    }
    
    /* If path is absolute, use it directly */
    if (path[0] == '/') {
        strncpy(buf, path, size - 1);
        buf[size - 1] = '\0';
        return buf;
    }
    
    /* Build absolute path from current directory */
    int len = snprintf(buf, size, "%s/%s", current_dir, path);
    if (len >= (int)size) {
        return NULL;  /* Path too long */
    }
    
    return buf;
}

/*
 * NOTE: parse_line is kept for backwards compatibility but is no longer used.
 * The parser_esp32 module now handles command parsing with variable expansion.
 */
#if 0  /* Disabled - replaced by parser_parse_line() */
/**
 * @brief Parse command line into argc/argv
 * 
 * Splits the command line on whitespace and handles basic quoting.
 * 
 * @param line The command line to parse (will be modified)
 * @param argv Array to store argument pointers
 * @param max_args Maximum number of arguments
 * @return Number of arguments parsed
 */
static int parse_line(char *line, char **argv, int max_args) {
    int argc = 0;
    char *p = line;
    
    while (*p && argc < max_args) {
        /* Skip leading whitespace */
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }
        
        if (!*p) break;
        
        /* Check for quoted string */
        if (*p == '"' || *p == '\'') {
            char quote = *p++;
            argv[argc++] = p;
            
            /* Find closing quote */
            while (*p && *p != quote) {
                p++;
            }
            if (*p) {
                *p++ = '\0';
            }
        } else {
            /* Regular argument - find end */
            argv[argc++] = p;
            while (*p && !isspace((unsigned char)*p)) {
                p++;
            }
            if (*p) {
                *p++ = '\0';
            }
        }
    }
    
    return argc;
}
#endif  /* Disabled parse_line */

/**
 * @brief Find command in command table
 * 
 * @param name Command name to find
 * @return Pointer to command structure, or NULL if not found
 */
static const esp_shell_cmd_t* find_command(const char *name) {
    for (int i = 0; builtin_commands[i].name != NULL; i++) {
        if (strcmp(name, builtin_commands[i].name) == 0) {
            return &builtin_commands[i];
        }
    }
    return NULL;
}

/**
 * @brief Add command to history
 * 
 * @param line Command line to add
 */
static void add_to_history(const char *line) {
    if (strlen(line) == 0) return;
    
    /* Don't add duplicates of the last command */
    if (history_count > 0 && 
        strcmp(history[(history_count - 1) % ESP_SHELL_HISTORY_SIZE], line) == 0) {
        return;
    }
    
    strncpy(history[history_count % ESP_SHELL_HISTORY_SIZE], 
            line, ESP_SHELL_MAX_LINE_LEN - 1);
    history[history_count % ESP_SHELL_HISTORY_SIZE][ESP_SHELL_MAX_LINE_LEN - 1] = '\0';
    history_count++;
    
    /* Also add to terminal history for up/down arrow navigation */
    terminal_history_add(line);
}

/* ============================================================================
 * Built-in Command Implementations
 * ============================================================================ */

/**
 * @brief help command - Show available commands
 */
static int cmd_help(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    printf("ESP32 Shell - Available commands:\n");
    printf("---------------------------------\n");
    for (int i = 0; builtin_commands[i].name != NULL; i++) {
        printf("  %-10s - %s\n", 
               builtin_commands[i].name, 
               builtin_commands[i].help);
    }
    printf("\nNote: This is a simplified shell for ESP32.\n");
    printf("Features not available:\n");
    printf("  - Pipelines (cmd1 | cmd2)\n");
    printf("  - Background processes (cmd &)\n");
    printf("  - External commands\n");
    printf("\n");
    return 0;
}

/**
 * @brief exit command - Exit shell (reboots ESP32)
 */
static int cmd_exit(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    printf("Rebooting ESP32...\n");
    platform_sleep_ms(100);
    esp_restart();
    return 0;  /* Never reached */
}

/**
 * @brief reboot command - Reboot ESP32
 */
static int cmd_reboot(int argc, char **argv) {
    return cmd_exit(argc, argv);
}

/**
 * @brief info command - Show system information
 */
static int cmd_info(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    /* Get flash size using the newer API */
    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);
    
    printf("ESP32 System Information\n");
    printf("------------------------\n");
    printf("Chip:         ESP32 with %d CPU cores\n", chip_info.cores);
    printf("WiFi:         %s\n", (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "Yes" : "No");
    printf("Bluetooth:    %s\n", (chip_info.features & CHIP_FEATURE_BT) ? "Yes" : "No");
    printf("Flash:        %lu MB %s\n", 
           (unsigned long)(flash_size / (1024 * 1024)),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "(embedded)" : "(external)");
    printf("Free heap:    %lu bytes\n", (unsigned long)esp_get_free_heap_size());
    printf("IDF version:  %s\n", esp_get_idf_version());
    
    return 0;
}

/**
 * @brief free command - Show free memory
 */
static int cmd_free(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    printf("Free heap memory: %lu bytes\n", (unsigned long)esp_get_free_heap_size());
    printf("Minimum free:     %lu bytes\n", (unsigned long)esp_get_minimum_free_heap_size());
    
    return 0;
}

/**
 * @brief uptime command - Show system uptime
 */
static int cmd_uptime(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    unsigned long ms = platform_get_time_ms();
    unsigned long secs = ms / 1000;
    unsigned long mins = secs / 60;
    unsigned long hours = mins / 60;
    
    printf("Uptime: %lu:%02lu:%02lu (%lu ms)\n", 
           hours, mins % 60, secs % 60, ms);
    
    return 0;
}

/**
 * @brief gpio command - Control GPIO pins
 * 
 * Usage:
 *   gpio read <pin>        - Read pin state (0 or 1)
 *   gpio write <pin> <0|1> - Write pin state
 *   gpio mode <pin> <in|out> - Set pin mode
 */
static int cmd_gpio(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: gpio <read|write|mode> <pin> [value]\n");
        printf("  gpio read <pin>         - Read pin state\n");
        printf("  gpio write <pin> <0|1>  - Write HIGH/LOW\n");
        printf("  gpio mode <pin> <in|out> - Set input/output\n");
        return 1;
    }
    
    int pin = atoi(argv[2]);
    
    /* Validate pin number (ESP32-S3 has GPIO 0-48, but some are reserved) */
    if (pin < 0 || pin > 48) {
        printf("gpio: invalid pin number %d (must be 0-48)\n", pin);
        return 1;
    }
    
    if (strcmp(argv[1], "read") == 0) {
        /* Read pin state */
        int level = gpio_get_level(pin);
        printf("GPIO%d = %d\n", pin, level);
        return 0;
        
    } else if (strcmp(argv[1], "write") == 0) {
        /* Write pin state */
        if (argc < 4) {
            printf("gpio write: missing value (0 or 1)\n");
            return 1;
        }
        int value = atoi(argv[3]);
        if (value != 0 && value != 1) {
            printf("gpio write: value must be 0 or 1\n");
            return 1;
        }
        esp_err_t err = gpio_set_level(pin, value);
        if (err != ESP_OK) {
            printf("gpio write: failed (pin may not be configured as output)\n");
            return 1;
        }
        printf("GPIO%d <- %d\n", pin, value);
        return 0;
        
    } else if (strcmp(argv[1], "mode") == 0) {
        /* Set pin mode */
        if (argc < 4) {
            printf("gpio mode: missing mode (in or out)\n");
            return 1;
        }
        gpio_mode_t mode;
        if (strcmp(argv[3], "in") == 0 || strcmp(argv[3], "input") == 0) {
            mode = GPIO_MODE_INPUT;
        } else if (strcmp(argv[3], "out") == 0 || strcmp(argv[3], "output") == 0) {
            mode = GPIO_MODE_OUTPUT;
        } else {
            printf("gpio mode: invalid mode '%s' (use 'in' or 'out')\n", argv[3]);
            return 1;
        }
        
        /* Configure the GPIO */
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << pin),
            .mode = mode,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        esp_err_t err = gpio_config(&io_conf);
        if (err != ESP_OK) {
            printf("gpio mode: failed to configure pin\n");
            return 1;
        }
        printf("GPIO%d mode set to %s\n", pin, (mode == GPIO_MODE_INPUT) ? "INPUT" : "OUTPUT");
        return 0;
        
    } else {
        printf("gpio: unknown command '%s'\n", argv[1]);
        printf("Use: read, write, or mode\n");
        return 1;
    }
}

/**
 * @brief fsinfo command - Show filesystem information
 * 
 * Displays SPIFFS partition usage (total, used, free space).
 */
static int cmd_fsinfo(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    size_t total = 0, used = 0;
    esp_err_t ret = esp_spiffs_info(NULL, &total, &used);
    
    if (ret != ESP_OK) {
        printf("fsinfo: failed to get filesystem info\n");
        return 1;
    }
    
    size_t free_space = total - used;
    int usage_percent = (total > 0) ? (int)((used * 100) / total) : 0;
    
    printf("SPIFFS Filesystem Info\n");
    printf("----------------------\n");
    printf("Mount point:  /spiffs\n");
    printf("Total size:   %lu bytes (%lu KB)\n", (unsigned long)total, (unsigned long)(total / 1024));
    printf("Used:         %lu bytes (%lu KB)\n", (unsigned long)used, (unsigned long)(used / 1024));
    printf("Free:         %lu bytes (%lu KB)\n", (unsigned long)free_space, (unsigned long)(free_space / 1024));
    printf("Usage:        %d%%\n", usage_percent);
    
    return 0;
}

/**
 * @brief format command - Format the SPIFFS filesystem
 * 
 * WARNING: This erases all data on the filesystem!
 * Requires confirmation: format --yes
 */
static int cmd_format(int argc, char **argv) {
    /* Require --yes flag to confirm */
    int confirmed = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--yes") == 0 || strcmp(argv[i], "-y") == 0) {
            confirmed = 1;
            break;
        }
    }
    
    if (!confirmed) {
        printf("WARNING: This will erase all files on /spiffs!\n");
        printf("To confirm, run: format --yes\n");
        return 1;
    }
    
    printf("Formatting SPIFFS filesystem...\n");
    
    /* Unmount first */
    esp_vfs_spiffs_unregister(NULL);
    
    /* Format the partition */
    esp_err_t ret = esp_spiffs_format(NULL);
    if (ret != ESP_OK) {
        printf("format: failed to format filesystem\n");
        /* Try to remount anyway */
        esp_vfs_spiffs_conf_t conf = {
            .base_path = "/spiffs",
            .partition_label = NULL,
            .max_files = 5,
            .format_if_mount_failed = false
        };
        esp_vfs_spiffs_register(&conf);
        return 1;
    }
    
    /* Remount */
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = false
    };
    ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        printf("format: failed to remount filesystem\n");
        return 1;
    }
    
    printf("Format complete. Filesystem is empty.\n");
    return 0;
}

/**
 * @brief pwd command - Print working directory
 */
static int cmd_pwd(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    char cwd[ESP_SHELL_MAX_PATH];
    if (vfs_getcwd(cwd, sizeof(cwd)) == 0) {
        printf("%s\n", cwd);
    } else {
        printf("%s\n", current_dir);
    }
    return 0;
}

/**
 * @brief cd command - Change directory
 */
static int cmd_cd(int argc, char **argv) {
    if (argc < 2) {
        /* cd with no args goes to root */
        if (vfs_chdir("/spiffs") == 0) {
            strcpy(current_dir, "/spiffs");
        }
        return 0;
    }
    
    /* Use VFS to change directory */
    if (vfs_chdir(argv[1]) == 0) {
        /* Update our local copy of cwd */
        vfs_getcwd(current_dir, ESP_SHELL_MAX_PATH);
        return 0;
    }
    
    printf("cd: %s: No such directory\n", argv[1]);
    return 1;
}

/**
 * @brief ls command - List directory contents
 * 
 * Uses VFS abstraction layer for portable directory listing.
 */
static int cmd_ls(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : ".";
    char path[ESP_SHELL_MAX_PATH];
    
    if (build_path(dir, path, sizeof(path)) == NULL) {
        printf("ls: path too long\n");
        return 1;
    }
    
    /* Use VFS directory functions */
    vfs_dir_t d = vfs_opendir(path);
    if (d == NULL) {
        printf("ls: cannot access '%s': No such file or directory\n", dir);
        return 1;
    }
    
    vfs_dirent_t entry;
    while (vfs_readdir(d, &entry) == 0) {
        /* Print with size info for files - use redir_printf for redirection */
        if (entry.is_dir) {
            redir_printf("%s/\n", entry.name);
        } else {
            redir_printf("%-20s %6lu bytes\n", entry.name, (unsigned long)entry.size);
        }
    }
    
    vfs_closedir(d);
    return 0;
}

/**
 * @brief cat command - Display file contents or write to file
 * 
 * Usage:
 *   cat <file>          - Display file contents
 *   cat >file            - Write stdin lines to file (overwrite)
 *   cat >>file           - Append stdin lines to file
 * 
 * In write mode, type lines and press Enter. End input with Ctrl+D or empty line.
 * Uses VFS abstraction layer for portable file operations.
 */
static int cmd_cat(int argc, char **argv) {
    /* Check for write mode: cat >file or cat >>file */
    if (argc >= 2 && argv[1][0] == '>') {
        const char *mode = "w";  /* Default: overwrite */
        const char *filename = argv[1] + 1;  /* Skip first '>' */
        
        /* Check for append mode (>>) */
        if (argv[1][1] == '>') {
            mode = "a";
            filename = argv[1] + 2;  /* Skip ">>" */
        }
        
        /* Handle "cat > file" (space between > and filename) */
        if (*filename == '\0' && argc >= 3) {
            filename = argv[2];
        }
        
        if (*filename == '\0') {
            printf("Usage: cat >file or cat >>file\n");
            return 1;
        }
        
        char path[ESP_SHELL_MAX_PATH];
        if (build_path(filename, path, sizeof(path)) == NULL) {
            printf("cat: path too long\n");
            return 1;
        }
        
        vfs_file_t f = vfs_open(path, mode);
        if (f == NULL) {
            printf("cat: cannot open '%s' for writing\n", filename);
            return 1;
        }
        
        /* Read lines from stdin until empty line or Ctrl+D */
        printf("Enter text (empty line or Ctrl+D to finish):\n");
        char line[256];
        while (1) {
            int c;
            int i = 0;
            
            /* Read a line character by character */
            while (i < (int)sizeof(line) - 1) {
                c = getchar();
                if (c == EOF || c == 4) {  /* EOF or Ctrl+D */
                    if (i == 0) goto done;  /* Empty input, finish */
                    break;
                }
                if (c == '\r' || c == '\n') {
                    break;
                }
                if (c == 0x7f || c == 0x08) {  /* Backspace */
                    if (i > 0) {
                        i--;
                        printf("\b \b");  /* Erase character on screen */
                    }
                    continue;
                }
                line[i++] = c;
                putchar(c);  /* Echo */
            }
            printf("\n");
            line[i] = '\0';
            
            /* Empty line ends input */
            if (i == 0) break;
            
            /* Write line with newline */
            vfs_write(line, i, f);
            vfs_write("\n", 1, f);
        }
        
    done:
        vfs_close(f);
        printf("File saved.\n");
        return 0;
    }
    
    /* Read mode: display file contents */
    if (argc < 2) {
        printf("Usage: cat <file> or cat >file\n");
        return 1;
    }
    
    char path[ESP_SHELL_MAX_PATH];
    if (build_path(argv[1], path, sizeof(path)) == NULL) {
        printf("cat: path too long\n");
        return 1;
    }
    
    vfs_file_t f = vfs_open(path, "r");
    if (f == NULL) {
        printf("cat: %s: No such file or directory\n", argv[1]);
        return 1;
    }
    
    char buf[128];
    size_t bytes_read;
    while ((bytes_read = vfs_read(buf, sizeof(buf) - 1, f)) > 0) {
        buf[bytes_read] = '\0';
        redir_puts(buf);  /* Use redir_puts for redirection support */
    }
    
    vfs_close(f);
    return 0;
}

/**
 * @brief echo command - Print text
 * 
 * Uses redir_printf to support output redirection (> and >>).
 */
static int cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        redir_printf("%s", argv[i]);
        if (i < argc - 1) redir_printf(" ");
    }
    redir_printf("\n");
    return 0;
}

/**
 * @brief touch command - Create empty file
 * 
 * Uses VFS abstraction layer for portable file creation.
 */
static int cmd_touch(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: touch <file>\n");
        return 1;
    }
    
    char path[ESP_SHELL_MAX_PATH];
    if (build_path(argv[1], path, sizeof(path)) == NULL) {
        printf("touch: path too long\n");
        return 1;
    }
    
    vfs_file_t f = vfs_open(path, "a");
    if (f == NULL) {
        printf("touch: cannot create '%s'\n", argv[1]);
        return 1;
    }
    
    vfs_close(f);
    printf("Created: %s\n", argv[1]);
    return 0;
}

/**
 * @brief rm command - Remove file
 * 
 * Uses VFS abstraction layer for portable file removal.
 */
static int cmd_rm(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: rm <file>\n");
        return 1;
    }
    
    char path[ESP_SHELL_MAX_PATH];
    if (build_path(argv[1], path, sizeof(path)) == NULL) {
        printf("rm: path too long\n");
        return 1;
    }
    
    if (vfs_remove(path) != 0) {
        printf("rm: cannot remove '%s'\n", argv[1]);
        return 1;
    }
    
    printf("Removed: %s\n", argv[1]);
    return 0;
}

/**
 * @brief mkdir command - Create directory
 * 
 * Uses VFS abstraction layer. Note: SPIFFS does not support true 
 * directories, but this is here for future LittleFS support.
 */
static int cmd_mkdir(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: mkdir <directory>\n");
        return 1;
    }
    
    char path[ESP_SHELL_MAX_PATH];
    if (build_path(argv[1], path, sizeof(path)) == NULL) {
        printf("mkdir: path too long\n");
        return 1;
    }
    
    if (vfs_mkdir(path) != 0) {
        printf("mkdir: cannot create '%s'\n", argv[1]);
        printf("Note: SPIFFS does not support true directories.\n");
        printf("Consider using LittleFS for directory support.\n");
        return 1;
    }
    
    printf("Created directory: %s\n", argv[1]);
    return 0;
}

/**
 * @brief history command - Show command history
 * 
 * Displays the list of recently executed commands.
 * Uses redir_printf for output redirection support.
 */
static int cmd_history(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    if (history_count == 0) {
        redir_printf("No commands in history.\n");
        return 0;
    }
    
    /* Calculate starting index for display */
    int start = 0;
    int total = history_count;
    
    if (history_count > ESP_SHELL_HISTORY_SIZE) {
        start = history_count - ESP_SHELL_HISTORY_SIZE;
        total = ESP_SHELL_HISTORY_SIZE;
    }
    
    for (int i = 0; i < total; i++) {
        int idx = (start + i) % ESP_SHELL_HISTORY_SIZE;
        redir_printf("%4d  %s\n", start + i + 1, history[idx]);
    }
    
    return 0;
}

/**
 * @brief set command - Set an environment variable
 * 
 * Usage: set NAME=value
 *    or: set NAME value
 * 
 * Sets the variable NAME to the given value.
 * Variables can be referenced as $NAME or ${NAME} in commands.
 */
static int cmd_set(int argc, char **argv) {
    if (argc < 2) {
        /* No arguments - show all variables (same as env) */
        return cmd_env(argc, argv);
    }
    
    /* Check for NAME=value format in first argument */
    char name[PARSER_MAX_VAR_NAME];
    char value[PARSER_MAX_VAR_VALUE];
    
    if (parser_is_assignment(argv[1], name, value)) {
        /* NAME=value format */
        if (parser_setvar(name, value) != 0) {
            redir_printf("set: too many variables or name/value too long\n");
            return 1;
        }
        return 0;
    }
    
    /* Check for "set NAME value" format */
    if (argc >= 3) {
        if (strlen(argv[1]) >= PARSER_MAX_VAR_NAME) {
            redir_printf("set: variable name too long\n");
            return 1;
        }
        if (strlen(argv[2]) >= PARSER_MAX_VAR_VALUE) {
            redir_printf("set: value too long\n");
            return 1;
        }
        if (parser_setvar(argv[1], argv[2]) != 0) {
            redir_printf("set: too many variables\n");
            return 1;
        }
        return 0;
    }
    
    redir_printf("Usage: set NAME=value\n");
    redir_printf("   or: set NAME value\n");
    return 1;
}

/**
 * @brief unset command - Remove an environment variable
 * 
 * Usage: unset NAME
 */
static int cmd_unset(int argc, char **argv) {
    if (argc < 2) {
        redir_printf("Usage: unset NAME\n");
        return 1;
    }
    
    if (parser_unsetvar(argv[1]) != 0) {
        redir_printf("unset: variable '%s' not found\n", argv[1]);
        return 1;
    }
    
    return 0;
}

/**
 * @brief Callback for env command to print variables
 */
static void env_print_callback(const char *name, const char *value, void *user_data) {
    (void)user_data;
    redir_printf("%s=%s\n", name, value);
}

/**
 * @brief env command - List all environment variables
 * 
 * Usage: env
 */
static int cmd_env(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    int count = parser_var_count();
    if (count == 0) {
        redir_printf("No environment variables defined.\n");
        return 0;
    }
    
    parser_list_vars(env_print_callback, NULL);
    return 0;
}

/* ============================================================================
 * Unavailable Feature Stubs
 * 
 * These commands exist on Linux but are not available on ESP32 due to
 * hardware/OS limitations (no fork(), no POSIX signals, etc).
 * They provide helpful error messages instead of "command not found".
 * ============================================================================ */

/**
 * @brief jobs command stub - Not available on ESP32
 * 
 * ESP32 runs on FreeRTOS without fork() so background jobs are not possible.
 */
static int cmd_jobs(int argc, char **argv) {
    (void)argc;
    (void)argv;
    printf("jobs: not available on ESP32\n");
    printf("  ESP32 does not support background processes (no fork).\n");
    printf("  All commands run in the foreground.\n");
    return 1;
}

/**
 * @brief fg command stub - Not available on ESP32
 */
static int cmd_fg(int argc, char **argv) {
    (void)argc;
    (void)argv;
    printf("fg: not available on ESP32\n");
    printf("  No background jobs to bring to foreground.\n");
    return 1;
}

/**
 * @brief bg command stub - Not available on ESP32
 */
static int cmd_bg(int argc, char **argv) {
    (void)argc;
    (void)argv;
    printf("bg: not available on ESP32\n");
    printf("  ESP32 does not support background processes.\n");
    return 1;
}

/* ============================================================================
 * Shell Public API Implementation
 * ============================================================================ */

/**
 * @brief Initialize the ESP32 shell
 * 
 * Sets up the shell and VFS layer for filesystem operations.
 */
int esp_shell_init(void) {
    ESP_LOGI(TAG, "Initializing shell...");
    
    /* Initialize the command parser and variable system */
    parser_init();
    
    /* Clear history */
    memset(history, 0, sizeof(history));
    history_count = 0;
    history_index = 0;
    
    /* Initialize VFS layer - this mounts SPIFFS on ESP32 */
    if (vfs_init() != 0) {
        ESP_LOGE(TAG, "Failed to initialize VFS");
        /* Continue anyway - some commands may still work */
    }
    
    /* Set initial directory from VFS */
    if (vfs_getcwd(current_dir, ESP_SHELL_MAX_PATH) != 0) {
        strcpy(current_dir, "/spiffs");
    }
    
    ESP_LOGI(TAG, "Shell initialized with %d built-in commands", 
             (int)(sizeof(builtin_commands) / sizeof(builtin_commands[0]) - 1));
    
    return 0;
}

/**
 * @brief Execute a single command string
 * 
 * Parses the command line, extracts I/O redirection operators,
 * and executes the command using direct function calls.
 * 
 * Supports:
 * - > file  : Redirect stdout to file (truncate)
 * - >> file : Redirect stdout to file (append)
 * - < file  : (Not yet implemented) Redirect stdin from file
 * - |       : (Not supported) Pipelines not available on ESP32
 * 
 * Variable expansion is performed by the parser:
 * - $VAR expands to the value of VAR
 * - ${VAR} expands to the value of VAR
 */
int esp_shell_execute(const char *cmdline) {
    /* Use the enhanced parser with variable expansion */
    parser_result_t result;
    parser_error_t err = parser_parse_line(cmdline, &result);
    
    /* Handle parser errors */
    if (err == PARSER_ERR_EMPTY) {
        return 0;  /* Empty line - not an error */
    }
    
    if (err != PARSER_OK) {
        printf("parse error: %s\n", parser_error_string(err));
        return 1;
    }
    
    /* Check for unsupported pipeline operator */
    if (has_pipeline(result.argc, result.argv)) {
        printf("error: pipelines not supported on ESP32\n");
        parser_free_result(&result);
        return 1;
    }
    
    /* Check for unsupported background operator (&) */
    if (has_background(result.argc, result.argv)) {
        printf("error: background processes (&) not supported on ESP32\n");
        printf("  ESP32 runs on FreeRTOS without fork() support.\n");
        parser_free_result(&result);
        return 1;
    }
    
    /* Build a parsed_cmd_t from the parser result */
    parsed_cmd_t cmd;
    cmd.argc = result.argc;
    /* Point directly to parser's argv array - it remains valid until parser_free_result */
    cmd.argv = result.argv;
    /* Cast redir type since parser uses int, executor uses redir_type_t */
    cmd.stdout_redir = (redir_type_t)result.stdout_redir;
    cmd.stdin_redir = (redir_type_t)result.stdin_redir;
    /* Cast away const - the executor doesn't modify these */
    cmd.stdout_file = (char *)result.stdout_file;
    cmd.stdin_file = (char *)result.stdin_file;
    
    /* Find the command */
    const esp_shell_cmd_t *builtin = find_command(cmd.argv[0]);
    if (builtin == NULL) {
        printf("%s: command not found\n", cmd.argv[0]);
        parser_free_result(&result);
        return 127;
    }
    
    /* Execute with redirection support */
    int ret;
    if (cmd.stdout_redir != REDIR_NONE || cmd.stdin_redir != REDIR_NONE) {
        ret = execute_with_redirection(&cmd, builtin->func);
    } else {
        /* No redirection - execute directly */
        ret = builtin->func(cmd.argc, cmd.argv);
    }
    
    parser_free_result(&result);
    return ret;
}

/*
 * NOTE: read_line is kept for backwards compatibility but is no longer used.
 * The terminal_esp32 module now provides terminal_read_line() with
 * enhanced line editing (arrow keys, history navigation, etc).
 */
#if 0  /* Disabled - replaced by terminal_read_line() */
/**
 * @brief Read a line of input from the console
 * 
 * Simple line editor with backspace support.
 * 
 * @param buf Buffer to store the line
 * @param size Size of buffer
 * @return Number of characters read, or -1 on error
 */
static int read_line(char *buf, size_t size) {
    size_t pos = 0;
    
    while (pos < size - 1) {
        int c = platform_read_char();
        
        if (c < 0) {
            /* No character available, yield to other tasks */
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        
        if (c == '\n' || c == '\r') {
            /* End of line */
            platform_write_char('\n');
            buf[pos] = '\0';
            return pos;
        }
        
        if (c == 0x7F || c == '\b') {
            /* Backspace */
            if (pos > 0) {
                pos--;
                /* Erase character on screen: backspace, space, backspace */
                platform_write_string("\b \b");
            }
            continue;
        }
        
        if (c == 0x03) {
            /* Ctrl+C - cancel current line */
            platform_write_string("^C\n");
            buf[0] = '\0';
            return 0;
        }
        
        if (c >= 32 && c < 127) {
            /* Printable character */
            buf[pos++] = (char)c;
            platform_write_char((char)c);
        }
    }
    
    buf[pos] = '\0';
    return pos;
}
#endif  /* Disabled read_line */

/**
 * @brief Run the shell main loop
 * 
 * Main loop uses terminal_read_line for enhanced line editing including:
 * - Left/right arrow keys for cursor movement
 * - Up/down arrow keys for history navigation
 * - Ctrl+A/E for home/end
 * - Ctrl+U/K for line kill
 * - Ctrl+L for clear screen
 */
void esp_shell_run(void) {
    ESP_LOGI(TAG, "Entering shell main loop");
    
    /* Initialize terminal subsystem for line editing */
    terminal_init();
    
    while (1) {
        /* Print prompt */
        printf("%s", ESP_SHELL_PROMPT);
        fflush(stdout);
        
        /* Read command line with full editing support */
        int result = terminal_read_line(line_buffer, sizeof(line_buffer));
        
        if (result < 0) {
            /* Cancelled (Ctrl+C) - just continue to next prompt */
            continue;
        }
        
        /* Skip empty lines */
        if (strlen(line_buffer) == 0) {
            continue;
        }
        
        /* Add to history */
        add_to_history(line_buffer);
        
        /* Execute the command */
        esp_shell_execute(line_buffer);
    }
}

/**
 * @brief Get current working directory
 */
char* esp_shell_getcwd(char *buf, size_t size) {
    strncpy(buf, current_dir, size - 1);
    buf[size - 1] = '\0';
    return buf;
}

/**
 * @brief Change current working directory
 */
int esp_shell_chdir(const char *path) {
    char *args[2] = {"cd", (char*)path};
    return cmd_cd(2, args);
}
