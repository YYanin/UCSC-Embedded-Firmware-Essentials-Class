/**
 * @file platform_linux.c
 * @brief Platform Abstraction Layer - Linux Implementation
 * 
 * This file implements the platform abstraction layer for Linux/POSIX systems.
 * It wraps standard POSIX APIs to provide a consistent interface that matches
 * the ESP32 implementation.
 * 
 * Compile with: -DPLATFORM_LINUX (or just don't define ESP_PLATFORM)
 */

#ifndef ESP_PLATFORM  /* Only compile on Linux */

#define _POSIX_C_SOURCE 200809L

#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <errno.h>

/* ============================================================================
 * Static Variables
 * ============================================================================ */

/* Original terminal settings - restored on cleanup */
static struct termios orig_termios;

/* Flag to track if terminal was modified */
static int terminal_raw_mode = 0;

/* Start time for uptime calculation */
static struct timeval start_time;

/* ============================================================================
 * Platform Initialization and Cleanup
 * ============================================================================ */

/**
 * @brief Initialize the platform layer for Linux
 * 
 * Sets up terminal for character-by-character input (raw mode)
 * and records the start time for uptime calculations.
 */
void platform_init(void) {
    /* Record start time */
    gettimeofday(&start_time, NULL);
    
    /* Save original terminal settings */
    if (tcgetattr(STDIN_FILENO, &orig_termios) == 0) {
        struct termios raw = orig_termios;
        
        /* Disable canonical mode and echo 
         * We want character-by-character input without auto-echo */
        raw.c_lflag &= ~(ICANON | ECHO);
        
        /* Set minimum characters to read to 1 */
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        
        if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0) {
            terminal_raw_mode = 1;
        }
    }
}

/**
 * @brief Clean up platform resources
 * 
 * Restores the original terminal settings so the shell behaves
 * normally after our program exits.
 */
void platform_cleanup(void) {
    if (terminal_raw_mode) {
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
        terminal_raw_mode = 0;
    }
}

/* ============================================================================
 * Console I/O Functions
 * ============================================================================ */

/**
 * @brief Read a single character from stdin
 * 
 * In raw mode, this reads one character at a time.
 * 
 * @return Character read, or -1 on error/EOF
 */
int platform_read_char(void) {
    unsigned char c;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n == 1) {
        return c;
    }
    return -1;
}

/**
 * @brief Write a single character to stdout
 */
void platform_write_char(char c) {
    write(STDOUT_FILENO, &c, 1);
}

/**
 * @brief Write a null-terminated string to stdout
 */
void platform_write_string(const char *s) {
    write(STDOUT_FILENO, s, strlen(s));
}

/**
 * @brief Flush stdout
 */
void platform_flush(void) {
    fflush(stdout);
}

/* ============================================================================
 * Timing Functions
 * ============================================================================ */

/**
 * @brief Sleep for specified milliseconds
 * 
 * Uses usleep() which takes microseconds.
 */
void platform_sleep_ms(int ms) {
    usleep(ms * 1000);
}

/**
 * @brief Get current time in milliseconds since program start
 * 
 * @return Milliseconds since platform_init() was called
 */
unsigned long platform_get_time_ms(void) {
    struct timeval now;
    gettimeofday(&now, NULL);
    
    /* Calculate difference from start time */
    unsigned long secs = now.tv_sec - start_time.tv_sec;
    long usecs = now.tv_usec - start_time.tv_usec;
    
    if (usecs < 0) {
        secs--;
        usecs += 1000000;
    }
    
    return (secs * 1000) + (usecs / 1000);
}

/* ============================================================================
 * Memory Information
 * ============================================================================ */

/**
 * @brief Get free heap memory
 * 
 * On Linux, this returns a large value since heap is effectively unlimited
 * (limited by system RAM and swap).
 * 
 * @return Free memory estimate (not precise on Linux)
 */
size_t platform_get_free_heap(void) {
    /* Linux doesn't have a simple heap size limit like embedded systems.
     * Return a reasonable large value to indicate "plenty of memory" */
    return 1024 * 1024 * 100;  /* 100 MB - effectively unlimited for shell */
}

/**
 * @brief Get minimum free heap
 * 
 * Not tracked on Linux, returns same as current free heap.
 */
size_t platform_get_min_free_heap(void) {
    return platform_get_free_heap();
}

/* ============================================================================
 * Task/Thread Functions
 * ============================================================================ */

/**
 * @brief Wrapper structure for pthread arguments
 * 
 * pthreads use a different function signature (returns void*),
 * so we wrap the platform_task_func_t to match.
 */
typedef struct {
    platform_task_func_t func;
    void *arg;
} pthread_wrapper_args_t;

/**
 * @brief Wrapper function for pthread
 */
static void* pthread_wrapper(void *arg) {
    pthread_wrapper_args_t *wrapper = (pthread_wrapper_args_t*)arg;
    platform_task_func_t func = wrapper->func;
    void *task_arg = wrapper->arg;
    
    /* Free the wrapper structure */
    free(wrapper);
    
    /* Call the actual task function */
    func(task_arg);
    
    return NULL;
}

/**
 * @brief Create a new thread
 * 
 * Uses POSIX pthreads on Linux.
 */
int platform_task_create(platform_task_func_t func, 
                         const char *name,
                         size_t stack_size,
                         void *arg,
                         platform_task_handle_t *handle) {
    (void)name;        /* Thread name not easily set in POSIX */
    (void)stack_size;  /* Stack size set via pthread_attr if needed */
    
    /* Allocate wrapper for function arguments */
    pthread_wrapper_args_t *wrapper = malloc(sizeof(pthread_wrapper_args_t));
    if (wrapper == NULL) {
        return -1;
    }
    wrapper->func = func;
    wrapper->arg = arg;
    
    pthread_t thread;
    int ret = pthread_create(&thread, NULL, pthread_wrapper, wrapper);
    
    if (ret != 0) {
        free(wrapper);
        return -ret;
    }
    
    if (handle != NULL) {
        *handle = thread;
    }
    
    return 0;
}

/**
 * @brief Yield to other threads
 * 
 * Uses sched_yield() on Linux.
 */
void platform_task_yield(void) {
    sched_yield();
}

/**
 * @brief Delete/cancel a thread
 * 
 * Note: On Linux, this cancels the thread. The thread must be
 * written to handle cancellation properly.
 */
void platform_task_delete(platform_task_handle_t handle) {
    if (handle != 0) {
        pthread_cancel(handle);
    }
}

/* ============================================================================
 * Mutex Functions
 * ============================================================================ */

/**
 * @brief Initialize a mutex
 */
int platform_mutex_init(platform_mutex_t *mutex) {
    return pthread_mutex_init(mutex, NULL);
}

/**
 * @brief Lock a mutex
 */
int platform_mutex_lock(platform_mutex_t *mutex) {
    return pthread_mutex_lock(mutex);
}

/**
 * @brief Unlock a mutex
 */
int platform_mutex_unlock(platform_mutex_t *mutex) {
    return pthread_mutex_unlock(mutex);
}

/**
 * @brief Destroy a mutex
 */
void platform_mutex_destroy(platform_mutex_t *mutex) {
    pthread_mutex_destroy(mutex);
}

/* ============================================================================
 * System Functions
 * ============================================================================ */

/**
 * @brief "Reboot" on Linux - just exit the program
 */
void platform_reboot(void) {
    platform_cleanup();
    exit(0);
}

/**
 * @brief Get platform name
 */
const char* platform_get_name(void) {
    return "Linux";
}

#endif /* !ESP_PLATFORM */
