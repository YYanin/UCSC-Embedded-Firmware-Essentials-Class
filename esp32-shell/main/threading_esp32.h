/**
 * @file threading_esp32.h
 * @brief Thread and Mutex Abstraction for ESP32/Linux Portability
 * 
 * This header provides a unified interface for threading primitives
 * that works on both ESP32 (FreeRTOS) and Linux (POSIX pthreads).
 * 
 * On ESP32: Uses FreeRTOS semaphores and tasks
 * On Linux: Uses POSIX pthreads and mutexes
 * 
 * Usage:
 *   shell_mutex_t my_mutex;
 *   shell_mutex_init(&my_mutex);
 *   shell_mutex_lock(&my_mutex);
 *   // ... critical section ...
 *   shell_mutex_unlock(&my_mutex);
 *   shell_mutex_destroy(&my_mutex);
 * 
 * Note: This header provides compatibility macros. For the full platform
 * abstraction API, see platform.h which provides additional functionality
 * like task creation and timing functions.
 */

#ifndef THREADING_ESP32_H
#define THREADING_ESP32_H

#ifdef ESP_PLATFORM
/* ============================================================================
 * ESP32 / FreeRTOS Implementation
 * ============================================================================ */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

/**
 * @brief Mutex type for ESP32 (FreeRTOS semaphore)
 */
typedef SemaphoreHandle_t shell_mutex_t;

/**
 * @brief Initialize a mutex
 * 
 * Creates a FreeRTOS mutex semaphore.
 * 
 * @param m Pointer to mutex to initialize
 * @return pdTRUE on success, pdFALSE on failure (cast to int: 1 or 0)
 */
#define shell_mutex_init(m)    \
    ((*(m) = xSemaphoreCreateMutex()) != NULL ? 0 : -1)

/**
 * @brief Lock a mutex
 * 
 * Blocks until the mutex is available (with portMAX_DELAY timeout).
 * 
 * @param m Pointer to mutex to lock
 */
#define shell_mutex_lock(m)    \
    xSemaphoreTake(*(m), portMAX_DELAY)

/**
 * @brief Unlock a mutex
 * 
 * @param m Pointer to mutex to unlock
 */
#define shell_mutex_unlock(m)  \
    xSemaphoreGive(*(m))

/**
 * @brief Destroy a mutex
 * 
 * Frees the mutex semaphore.
 * 
 * @param m Pointer to mutex to destroy
 */
#define shell_mutex_destroy(m) \
    vSemaphoreDelete(*(m))

/**
 * @brief Task handle type
 */
typedef TaskHandle_t shell_task_handle_t;

/**
 * @brief Yield to other tasks
 */
#define shell_task_yield()     taskYIELD()

/**
 * @brief Sleep for specified milliseconds
 */
#define shell_sleep_ms(ms)     vTaskDelay(pdMS_TO_TICKS(ms))

#else
/* ============================================================================
 * Linux / POSIX Implementation
 * ============================================================================ */

#include <pthread.h>
#include <unistd.h>

/**
 * @brief Mutex type for Linux (POSIX pthread mutex)
 */
typedef pthread_mutex_t shell_mutex_t;

/**
 * @brief Initialize a mutex
 * 
 * Creates a POSIX mutex with default attributes.
 * 
 * @param m Pointer to mutex to initialize
 * @return 0 on success, error code on failure
 */
#define shell_mutex_init(m)    \
    pthread_mutex_init(m, NULL)

/**
 * @brief Lock a mutex
 * 
 * Blocks until the mutex is available.
 * 
 * @param m Pointer to mutex to lock
 */
#define shell_mutex_lock(m)    \
    pthread_mutex_lock(m)

/**
 * @brief Unlock a mutex
 * 
 * @param m Pointer to mutex to unlock
 */
#define shell_mutex_unlock(m)  \
    pthread_mutex_unlock(m)

/**
 * @brief Destroy a mutex
 * 
 * Frees mutex resources.
 * 
 * @param m Pointer to mutex to destroy
 */
#define shell_mutex_destroy(m) \
    pthread_mutex_destroy(m)

/**
 * @brief Task handle type (pthread on Linux)
 */
typedef pthread_t shell_task_handle_t;

/**
 * @brief Yield to other threads
 */
#define shell_task_yield()     sched_yield()

/**
 * @brief Sleep for specified milliseconds
 */
#define shell_sleep_ms(ms)     usleep((ms) * 1000)

#endif /* ESP_PLATFORM */

/* ============================================================================
 * Configuration Options
 * ============================================================================ */

/**
 * @brief Enable/disable threading features
 * 
 * Set to 0 to disable multi-threaded command execution.
 * This can save memory on constrained devices.
 */
#ifndef SHELL_THREADING_ENABLED
#ifdef ESP_PLATFORM
#define SHELL_THREADING_ENABLED 0  /* Disabled on ESP32 by default */
#else
#define SHELL_THREADING_ENABLED 1  /* Enabled on Linux by default */
#endif
#endif

#endif /* THREADING_ESP32_H */
