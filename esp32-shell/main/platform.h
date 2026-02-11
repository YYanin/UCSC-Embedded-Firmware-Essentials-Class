/**
 * @file platform.h
 * @brief Platform Abstraction Layer Header
 * 
 * This header provides a platform-independent interface for operations that
 * differ between Linux and ESP32. It allows the same shell code to compile
 * and run on both platforms by abstracting away OS-specific details.
 * 
 * Usage:
 * - On ESP32: ESP_PLATFORM is defined by ESP-IDF/PlatformIO
 * - On Linux: ESP_PLATFORM is not defined, uses POSIX APIs
 * 
 * The implementation files are:
 * - platform_linux.c: Linux/POSIX implementation
 * - platform_esp32.c: ESP32/FreeRTOS implementation
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include <stddef.h>

/* ============================================================================
 * Platform Detection Macros
 * ============================================================================ */

#ifdef ESP_PLATFORM
    /* ========================================================================
     * ESP32 Platform - ESP-IDF / FreeRTOS
     * ======================================================================== */
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    #include "esp_system.h"
    #include "esp_vfs.h"
    #include "esp_log.h"
    #include <dirent.h>
    #include <sys/stat.h>
    
    #define PLATFORM_ESP32 1
    #define PLATFORM_LINUX 0
    
    /* ESP32-specific constants */
    #define PLATFORM_NAME "ESP32"
    #define PLATFORM_MAX_PATH 128
    #define PLATFORM_PATH_SEPARATOR '/'
    
#else
    /* ========================================================================
     * Linux Platform - POSIX
     * ======================================================================== */
    #define _POSIX_C_SOURCE 200809L
    #include <unistd.h>
    #include <sys/wait.h>
    #include <sys/time.h>
    #include <pthread.h>
    #include <termios.h>
    #include <dirent.h>
    #include <sys/stat.h>
    
    #define PLATFORM_ESP32 0
    #define PLATFORM_LINUX 1
    
    /* Linux-specific constants */
    #define PLATFORM_NAME "Linux"
    #define PLATFORM_MAX_PATH 4096
    #define PLATFORM_PATH_SEPARATOR '/'
    
#endif

/* ============================================================================
 * Platform Initialization and Cleanup
 * ============================================================================ */

/**
 * @brief Initialize the platform layer
 * 
 * This function must be called before any other platform functions.
 * On Linux: Sets up terminal for raw input mode
 * On ESP32: Initializes console UART
 */
void platform_init(void);

/**
 * @brief Clean up platform resources
 * 
 * Call this before exiting the program.
 * On Linux: Restores terminal settings
 * On ESP32: No-op (reboot is the only exit)
 */
void platform_cleanup(void);

/* ============================================================================
 * Console I/O Functions
 * ============================================================================ */

/**
 * @brief Read a single character from console input
 * 
 * This is a non-blocking read on ESP32, blocking on Linux.
 * 
 * @return Character read (0-255), or -1 if no character available
 */
int platform_read_char(void);

/**
 * @brief Write a single character to console output
 * 
 * @param c Character to write
 */
void platform_write_char(char c);

/**
 * @brief Write a null-terminated string to console output
 * 
 * @param s String to write
 */
void platform_write_string(const char *s);

/**
 * @brief Flush console output buffer
 * 
 * Ensures all pending output is written to the console.
 */
void platform_flush(void);

/* ============================================================================
 * Timing Functions
 * ============================================================================ */

/**
 * @brief Sleep for specified milliseconds
 * 
 * @param ms Number of milliseconds to sleep
 */
void platform_sleep_ms(int ms);

/**
 * @brief Get current time in milliseconds
 * 
 * Returns time since some reference point (system start on ESP32,
 * arbitrary on Linux). Useful for measuring elapsed time.
 * 
 * @return Current time in milliseconds
 */
unsigned long platform_get_time_ms(void);

/* ============================================================================
 * Memory Information (mainly for ESP32)
 * ============================================================================ */

/**
 * @brief Get amount of free heap memory
 * 
 * @return Free heap memory in bytes
 */
size_t platform_get_free_heap(void);

/**
 * @brief Get minimum free heap memory since boot
 * 
 * Useful for detecting memory pressure over time.
 * 
 * @return Minimum free heap memory in bytes
 */
size_t platform_get_min_free_heap(void);

/* ============================================================================
 * Task/Thread Functions (ESP32 FreeRTOS / POSIX pthreads)
 * ============================================================================ */

/**
 * @brief Task function type
 * 
 * Functions that run as tasks must have this signature.
 */
typedef void (*platform_task_func_t)(void *arg);

/**
 * @brief Task handle type
 */
#if PLATFORM_ESP32
typedef TaskHandle_t platform_task_handle_t;
#else
typedef pthread_t platform_task_handle_t;
#endif

/**
 * @brief Create a new task/thread
 * 
 * @param func Function to run in the task
 * @param name Name for the task (for debugging)
 * @param stack_size Stack size in bytes
 * @param arg Argument to pass to the task function
 * @param handle Pointer to store task handle (can be NULL)
 * @return 0 on success, negative error code on failure
 */
int platform_task_create(platform_task_func_t func, 
                         const char *name,
                         size_t stack_size,
                         void *arg,
                         platform_task_handle_t *handle);

/**
 * @brief Yield to other tasks
 * 
 * Allows other tasks to run. Use in long-running loops.
 */
void platform_task_yield(void);

/**
 * @brief Delete a task
 * 
 * @param handle Task handle (NULL for current task on ESP32)
 */
void platform_task_delete(platform_task_handle_t handle);

/* ============================================================================
 * Mutex Functions (for thread safety)
 * ============================================================================ */

/**
 * @brief Mutex handle type
 */
#if PLATFORM_ESP32
#include "freertos/semphr.h"
typedef SemaphoreHandle_t platform_mutex_t;
#else
typedef pthread_mutex_t platform_mutex_t;
#endif

/**
 * @brief Initialize a mutex
 * 
 * @param mutex Pointer to mutex to initialize
 * @return 0 on success, negative error code on failure
 */
int platform_mutex_init(platform_mutex_t *mutex);

/**
 * @brief Lock a mutex
 * 
 * Blocks until mutex is available.
 * 
 * @param mutex Pointer to mutex to lock
 * @return 0 on success, negative error code on failure
 */
int platform_mutex_lock(platform_mutex_t *mutex);

/**
 * @brief Unlock a mutex
 * 
 * @param mutex Pointer to mutex to unlock
 * @return 0 on success, negative error code on failure
 */
int platform_mutex_unlock(platform_mutex_t *mutex);

/**
 * @brief Destroy a mutex
 * 
 * @param mutex Pointer to mutex to destroy
 */
void platform_mutex_destroy(platform_mutex_t *mutex);

/* ============================================================================
 * System Functions
 * ============================================================================ */

/**
 * @brief Reboot the system
 * 
 * On ESP32: Performs a software reset
 * On Linux: Exits the program with code 0
 */
void platform_reboot(void);

/**
 * @brief Get platform name string
 * 
 * @return "ESP32" or "Linux"
 */
const char* platform_get_name(void);

#endif /* PLATFORM_H */
