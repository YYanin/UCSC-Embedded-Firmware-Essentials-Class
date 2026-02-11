/**
 * @file platform_esp32.c
 * @brief Platform Abstraction Layer - ESP32 Implementation
 * 
 * This file implements the platform abstraction layer for ESP32 using
 * ESP-IDF and FreeRTOS APIs. It provides the same interface as the
 * Linux implementation for cross-platform compatibility.
 * 
 * This file is only compiled when building for ESP32 (ESP_PLATFORM defined).
 */

#ifdef ESP_PLATFORM  /* Only compile on ESP32 */

#include "platform.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/uart.h"

/* Tag for ESP logging */
static const char *TAG = "platform";

/* UART configuration for console I/O */
#define CONSOLE_UART_NUM    UART_NUM_0
#define CONSOLE_BAUD_RATE   115200
#define CONSOLE_BUF_SIZE    256

/* ============================================================================
 * Platform Initialization and Cleanup
 * ============================================================================ */

/**
 * @brief Initialize the platform layer for ESP32
 * 
 * Sets up UART for console I/O. We need to install the UART driver
 * to use uart_read_bytes and uart_write_bytes functions.
 */
void platform_init(void) {
    ESP_LOGI(TAG, "Initializing ESP32 platform layer");
    
    /* Configure UART parameters */
    uart_config_t uart_config = {
        .baud_rate = CONSOLE_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    /* Configure UART parameters */
    esp_err_t ret = uart_param_config(CONSOLE_UART_NUM, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "UART param config failed: %s", esp_err_to_name(ret));
    }
    
    /* Install UART driver with RX and TX buffers */
    ret = uart_driver_install(CONSOLE_UART_NUM, CONSOLE_BUF_SIZE * 2, 
                               CONSOLE_BUF_SIZE * 2, 0, NULL, 0);
    if (ret == ESP_ERR_INVALID_STATE) {
        /* Driver already installed, this is OK */
        ESP_LOGI(TAG, "UART driver already installed");
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART driver install failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "UART driver installed");
    }
    
    ESP_LOGI(TAG, "Platform initialized successfully");
}

/**
 * @brief Clean up platform resources
 * 
 * On ESP32, there's no "exit" - we either run forever or reboot.
 * This function is a no-op for compatibility with the Linux version.
 */
void platform_cleanup(void) {
    ESP_LOGI(TAG, "Platform cleanup (no-op on ESP32)");
    /* Nothing to clean up - UART continues running */
}

/* ============================================================================
 * Console I/O Functions
 * ============================================================================ */

/**
 * @brief Read a single character from UART console
 * 
 * Non-blocking read. Returns -1 if no character is available.
 * 
 * @return Character read (0-255), or -1 if no character available
 */
int platform_read_char(void) {
    uint8_t c;
    
    /* Non-blocking read from UART */
    int len = uart_read_bytes(CONSOLE_UART_NUM, &c, 1, 0);
    
    if (len > 0) {
        return (int)c;
    }
    
    return -1;  /* No character available */
}

/**
 * @brief Write a single character to UART console
 */
void platform_write_char(char c) {
    uart_write_bytes(CONSOLE_UART_NUM, &c, 1);
}

/**
 * @brief Write a null-terminated string to UART console
 */
void platform_write_string(const char *s) {
    uart_write_bytes(CONSOLE_UART_NUM, s, strlen(s));
}

/**
 * @brief Flush UART output buffer
 * 
 * Waits until all bytes have been transmitted.
 */
void platform_flush(void) {
    uart_wait_tx_done(CONSOLE_UART_NUM, pdMS_TO_TICKS(100));
}

/* ============================================================================
 * Timing Functions
 * ============================================================================ */

/**
 * @brief Sleep for specified milliseconds
 * 
 * Uses FreeRTOS vTaskDelay which yields to other tasks during the delay.
 */
void platform_sleep_ms(int ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/**
 * @brief Get current time in milliseconds since boot
 * 
 * Uses ESP high-resolution timer for accurate timing.
 * 
 * @return Milliseconds since system boot
 */
unsigned long platform_get_time_ms(void) {
    /* esp_timer_get_time() returns microseconds since boot */
    return (unsigned long)(esp_timer_get_time() / 1000);
}

/* ============================================================================
 * Memory Information
 * ============================================================================ */

/**
 * @brief Get free heap memory
 * 
 * @return Free heap memory in bytes
 */
size_t platform_get_free_heap(void) {
    return esp_get_free_heap_size();
}

/**
 * @brief Get minimum free heap memory since boot
 * 
 * Useful for detecting memory leaks or high watermark usage.
 * 
 * @return Minimum free heap memory in bytes
 */
size_t platform_get_min_free_heap(void) {
    return esp_get_minimum_free_heap_size();
}

/* ============================================================================
 * Task Functions (FreeRTOS wrappers)
 * ============================================================================ */

/**
 * @brief Create a new FreeRTOS task
 * 
 * Wraps xTaskCreate to match our platform API.
 * 
 * @param func Task function to run
 * @param name Task name (for debugging)
 * @param stack_size Stack size in bytes
 * @param arg Argument to pass to task function
 * @param handle Optional pointer to store task handle
 * @return 0 on success, -1 on failure
 */
int platform_task_create(platform_task_func_t func, 
                         const char *name,
                         size_t stack_size,
                         void *arg,
                         platform_task_handle_t *handle) {
    TaskHandle_t task_handle;
    
    /* FreeRTOS stack size is in words, not bytes on ESP32
     * But ESP-IDF xTaskCreate takes bytes, so we use it directly */
    BaseType_t ret = xTaskCreate(
        func,
        name,
        stack_size / sizeof(StackType_t),  /* Convert bytes to words */
        arg,
        tskIDLE_PRIORITY + 1,              /* Priority just above idle */
        &task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task '%s'", name);
        return -1;
    }
    
    if (handle != NULL) {
        *handle = task_handle;
    }
    
    ESP_LOGI(TAG, "Created task '%s'", name);
    return 0;
}

/**
 * @brief Yield to other tasks
 * 
 * Calls taskYIELD() to allow other tasks to run.
 */
void platform_task_yield(void) {
    taskYIELD();
}

/**
 * @brief Delete a task
 * 
 * @param handle Task handle to delete, or NULL to delete current task
 */
void platform_task_delete(platform_task_handle_t handle) {
    vTaskDelete(handle);
}

/* ============================================================================
 * Mutex Functions (FreeRTOS semaphore wrappers)
 * ============================================================================ */

/**
 * @brief Initialize a mutex
 * 
 * Creates a FreeRTOS mutex semaphore.
 * 
 * @return 0 on success, -1 on failure
 */
int platform_mutex_init(platform_mutex_t *mutex) {
    *mutex = xSemaphoreCreateMutex();
    if (*mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return -1;
    }
    return 0;
}

/**
 * @brief Lock a mutex
 * 
 * Blocks until mutex is available (with a timeout).
 * 
 * @return 0 on success, -1 on timeout/failure
 */
int platform_mutex_lock(platform_mutex_t *mutex) {
    if (xSemaphoreTake(*mutex, pdMS_TO_TICKS(10000)) != pdTRUE) {
        ESP_LOGE(TAG, "Mutex lock timeout");
        return -1;
    }
    return 0;
}

/**
 * @brief Unlock a mutex
 * 
 * @return 0 on success, -1 on failure
 */
int platform_mutex_unlock(platform_mutex_t *mutex) {
    if (xSemaphoreGive(*mutex) != pdTRUE) {
        ESP_LOGE(TAG, "Mutex unlock failed");
        return -1;
    }
    return 0;
}

/**
 * @brief Destroy a mutex
 */
void platform_mutex_destroy(platform_mutex_t *mutex) {
    if (*mutex != NULL) {
        vSemaphoreDelete(*mutex);
        *mutex = NULL;
    }
}

/* ============================================================================
 * System Functions
 * ============================================================================ */

/**
 * @brief Reboot the ESP32
 * 
 * Performs a software reset.
 */
void platform_reboot(void) {
    ESP_LOGI(TAG, "Rebooting...");
    platform_flush();
    platform_sleep_ms(100);  /* Allow log message to be sent */
    esp_restart();
    /* Never returns */
}

/**
 * @brief Get platform name
 */
const char* platform_get_name(void) {
    return "ESP32";
}

#endif /* ESP_PLATFORM */
