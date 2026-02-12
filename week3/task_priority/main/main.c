/**
 * @file main.c
 * @brief FreeRTOS 4-Task Priority Example for ESP32-S3
 * 
 * This program demonstrates FreeRTOS task scheduling with 4 tasks of different priorities:
 * 
 * Task1 (Priority 1 - Lowest):
 *   - Runs continuously in an infinite loop
 *   - Prints "Tsk1-P1" repeatedly
 *   - Only executes when no higher priority tasks are ready
 * 
 * Task2 (Priority 2):
 *   - Runs periodically every 500ms
 *   - Prints "Tsk2-P2 <in>" when starting execution
 *   - Prints "Tsk2-P2 <out>" when finishing (before blocking)
 *   - Preempts Task1 when it becomes ready
 * 
 * Task3 (Priority 3):
 *   - Runs periodically every 3000ms
 *   - Executes for approximately 5 seconds
 *   - Prints "Tsk3-P3 <in>" when starting execution
 *   - Prints "Tsk3-P3 <out>" when finishing (before blocking)
 *   - Preempts both Task1 and Task2
 * 
 * Task4 (Priority 4 - Highest):
 *   - Blocked on a binary semaphore
 *   - Unblocked when user presses a button (GPIO interrupt)
 *   - Runs for approximately 10 ticks (~100ms at 100Hz tick rate)
 *   - Prints "Tsk4-P4 <-" while running (arrow indicates active)
 *   - Prints "Tsk4-P4 ->" before blocking (arrow indicates going to blocked state)
 *   - Preempts all other tasks when unblocked
 * 
 * @author Generated with AI assistance
 * @date 2026
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"

/* ============================================================================
 * CONFIGURATION CONSTANTS
 * ============================================================================ */

// Logging tag for ESP_LOGx macros
static const char *TAG = "TASK_PRIORITY";

// Task priorities (higher number = higher priority)
// Priority 0 is reserved for the FreeRTOS idle task
#define TASK1_PRIORITY      1   // Lowest application priority
#define TASK2_PRIORITY      2   // Medium-low priority
#define TASK3_PRIORITY      3   // Medium-high priority
#define TASK4_PRIORITY      4   // Highest application priority

// Task stack sizes in bytes
// Each task needs sufficient stack for its local variables and function calls
#define TASK1_STACK_SIZE    2048
#define TASK2_STACK_SIZE    2048
#define TASK3_STACK_SIZE    2048
#define TASK4_STACK_SIZE    2048

// Timing constants (in milliseconds unless otherwise noted)
#define TASK1_DELAY_MS      100     // Delay between Task1 prints to avoid console flooding
#define TASK2_PERIOD_MS     500     // Task2 runs every 500ms
#define TASK3_PERIOD_MS     3000    // Task3 runs every 3000ms
#define TASK3_RUN_TIME_MS   5000    // Task3 runs for approximately 5 seconds
#define TASK4_RUN_TICKS     10      // Task4 runs for 10 ticks (~100ms at 100Hz)

// GPIO configuration for button input
// Using GPIO 0 which is typically the BOOT button on ESP32-S3 dev boards
// Change this if using a different GPIO pin for your button
#define BUTTON_GPIO         GPIO_NUM_0

// GPIO configuration for LED outputs
// 4 LEDs connected to GPIOs 4, 5, 6, and 7
// These will light up in sequence when the button is pressed
#define LED_GPIO_START      GPIO_NUM_4   // First LED GPIO (GPIO 4)
#define LED_GPIO_END        GPIO_NUM_7   // Last LED GPIO (GPIO 7)
#define NUM_LEDS            4            // Total number of LEDs

/* ============================================================================
 * GLOBAL VARIABLES
 * ============================================================================ */

// Binary semaphore handle for Task4 synchronization
// This semaphore is "given" by the button ISR and "taken" by Task4
static SemaphoreHandle_t xTask4Semaphore = NULL;

// Index tracking which LED in the sequence should light up next
// Cycles through 0, 1, 2, 3, 0, 1, 2, 3, ... with each button press
static volatile int current_led_index = 0;

/* ============================================================================
 * FUNCTION PROTOTYPES
 * ============================================================================ */

// Task function prototypes
static void vTask1_Continuous(void *pvParameters);
static void vTask2_Periodic500ms(void *pvParameters);
static void vTask3_Periodic3000ms(void *pvParameters);
static void vTask4_SemaphoreTriggered(void *pvParameters);

// ISR for button press (no IRAM_ATTR in prototype, only in definition)
static void gpio_isr_handler(void *arg);

// Initialization functions
static void init_gpio_button(void);
static void init_gpio_leds(void);
static void create_tasks(void);

/* ============================================================================
 * TASK IMPLEMENTATIONS
 * ============================================================================ */

/**
 * @brief Task1 - Lowest priority continuous task
 * 
 * This task runs continuously and only executes when no higher priority
 * tasks (Task2, Task3, Task4) are in the ready state. It demonstrates
 * that the lowest priority task gets CPU time only when the system is idle.
 * 
 * @param pvParameters Unused task parameter (required by FreeRTOS task signature)
 */
static void vTask1_Continuous(void *pvParameters)
{
    // Suppress unused parameter warning
    (void)pvParameters;
    
    // Infinite loop - Task1 never voluntarily blocks except for the small delay
    for (;;)
    {
        // Print task identifier
        // This output will only appear when Task2, Task3, and Task4 are all blocked
        printf("Tsk1-P1\n");
        
        // Small delay to prevent console flooding
        // This also gives the watchdog timer a chance to reset
        // Note: Even during this delay, higher priority tasks can preempt
        vTaskDelay(pdMS_TO_TICKS(TASK1_DELAY_MS));
    }
}

/**
 * @brief Task2 - Medium priority periodic task (500ms period)
 * 
 * This task wakes up every 500ms, performs its work (printing messages),
 * then blocks until the next period. It preempts Task1 when it becomes ready
 * but is preempted by Task3 and Task4.
 * 
 * @param pvParameters Unused task parameter (required by FreeRTOS task signature)
 */
static void vTask2_Periodic500ms(void *pvParameters)
{
    // Suppress unused parameter warning
    (void)pvParameters;
    
    // Store the last wake time for accurate periodic execution
    // vTaskDelayUntil() uses this to calculate exact wake times
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    // Infinite loop for periodic execution
    for (;;)
    {
        // Print entry message - indicates Task2 has started its execution window
        // The <in> symbol shows the task is entering its active state
        printf("Tsk2-P2 <in>\n");
        
        // Task2's "work" would go here
        // In this demo, we just print messages to show the task is running
        
        // Print exit message - indicates Task2 is about to block
        // The <out> symbol shows the task is leaving its active state
        printf("Tsk2-P2 <out>\n");
        
        // Block until the next period
        // vTaskDelayUntil provides more accurate timing than vTaskDelay
        // because it accounts for the time spent executing the task
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(TASK2_PERIOD_MS));
    }
}

/**
 * @brief Task3 - Higher priority periodic task (3000ms period, 5s execution)
 * 
 * This task wakes up every 3000ms and runs for approximately 5 seconds.
 * During its execution time, it preempts both Task1 and Task2.
 * Only Task4 (when triggered by button) can preempt this task.
 * 
 * @param pvParameters Unused task parameter (required by FreeRTOS task signature)
 */
static void vTask3_Periodic3000ms(void *pvParameters)
{
    // Suppress unused parameter warning
    (void)pvParameters;
    
    // Store the last wake time for accurate periodic execution
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    // Calculate number of iterations needed to run for ~5 seconds
    // We'll use small delays to allow other tasks to run and simulate work
    const int iterations = TASK3_RUN_TIME_MS / 100;  // 50 iterations of 100ms each
    
    // Infinite loop for periodic execution
    for (;;)
    {
        // Print entry message - indicates Task3 has started its execution window
        printf("Tsk3-P3 <in>\n");
        
        // Simulate work by running for approximately 5 seconds
        // During this time, Task3 is "active" but yields periodically
        // to allow the scheduler to run (and Task4 to preempt if triggered)
        for (int i = 0; i < iterations; i++)
        {
            // Small delay - this keeps the task "active" but allows preemption
            // Task4 can preempt during these delays if the button is pressed
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        // Print exit message - indicates Task3 is about to block
        printf("Tsk3-P3 <out>\n");
        
        // Block until the next period
        // Note: The period starts from when the task last woke, not when it finished
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(TASK3_PERIOD_MS));
    }
}

/**
 * @brief Task4 - Highest priority semaphore-triggered task
 * 
 * This task is the highest priority application task. It remains blocked
 * on a binary semaphore until a button press triggers the GPIO ISR,
 * which releases the semaphore. Once unblocked, Task4 lights up one of
 * the 4 LEDs (GPIOs 4-7) in sequence. The LED stays on while the button
 * is held. When the button is released, the LED turns off and the next
 * button press will light up the next LED in the sequence.
 * 
 * LED Sequence: GPIO4 -> GPIO5 -> GPIO6 -> GPIO7 -> GPIO4 -> ...
 * 
 * When running, Task4 preempts ALL other application tasks.
 * 
 * @param pvParameters Unused task parameter (required by FreeRTOS task signature)
 */
static void vTask4_SemaphoreTriggered(void *pvParameters)
{
    // Suppress unused parameter warning
    (void)pvParameters;
    
    // Infinite loop - Task4 spends most of its time blocked on the semaphore
    for (;;)
    {
        // Wait for the semaphore to be given by the button ISR
        // portMAX_DELAY means wait forever (no timeout)
        // The task will remain in BLOCKED state until xSemaphoreGive is called
        if (xSemaphoreTake(xTask4Semaphore, portMAX_DELAY) == pdTRUE)
        {
            // Semaphore acquired - button was pressed
            // Calculate which GPIO to light up based on current_led_index
            // LED GPIOs are 4, 5, 6, 7 so we add LED_GPIO_START to the index
            gpio_num_t current_led_gpio = LED_GPIO_START + current_led_index;
            
            // Turn ON the current LED in the sequence
            gpio_set_level(current_led_gpio, 1);
            
            // Print which LED is now lit
            printf("Tsk4-P4 <- LED%d (GPIO%d) ON\n", current_led_index, current_led_gpio);
            
            // Continue running while the button is held down (GPIO reads LOW when pressed)
            // The button has a pull-up resistor, so:
            //   - Button pressed = GPIO level is 0 (LOW)
            //   - Button released = GPIO level is 1 (HIGH)
            while (gpio_get_level(BUTTON_GPIO) == 0)
            {
                // LED remains on while button is held
                // Small delay to prevent flooding the scheduler
                vTaskDelay(TASK4_RUN_TICKS);
            }
            
            // Button has been released - turn OFF the current LED
            gpio_set_level(current_led_gpio, 0);
            
            // Print that the LED is now off
            printf("Tsk4-P4 -> LED%d (GPIO%d) OFF\n", current_led_index, current_led_gpio);
            
            // Advance to the next LED in the sequence for the next button press
            // Uses modulo to wrap around: 0 -> 1 -> 2 -> 3 -> 0 -> 1 -> ...
            current_led_index = (current_led_index + 1) % NUM_LEDS;
            
            // Clear any pending semaphore gives that may have occurred 
            // from button bounce during release
            xSemaphoreTake(xTask4Semaphore, 0);
            
            // Loop back to xSemaphoreTake to wait for next button press
        }
    }
}

/* ============================================================================
 * GPIO AND ISR IMPLEMENTATION
 * ============================================================================ */

/**
 * @brief GPIO Interrupt Service Routine for button press
 * 
 * This ISR is called when the button connected to BUTTON_GPIO is pressed.
 * It releases the semaphore that Task4 is waiting on, causing Task4 to
 * become ready and (because it's highest priority) immediately preempt
 * whatever task is currently running.
 * 
 * IMPORTANT: This function runs in interrupt context, so:
 *   - It must be in IRAM (IRAM_ATTR)
 *   - It can only call "FromISR" versions of FreeRTOS APIs
 *   - It should be as short as possible
 * 
 * @param arg Unused argument (could be used to pass GPIO number)
 */
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    // Suppress unused parameter warning
    (void)arg;
    
    // Variable to track if a higher priority task was woken
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Give the semaphore from ISR context
    // This unblocks Task4 if it's waiting on the semaphore
    xSemaphoreGiveFromISR(xTask4Semaphore, &xHigherPriorityTaskWoken);
    
    // If giving the semaphore woke a higher priority task, request a context switch
    // This ensures Task4 runs immediately after the ISR completes
    if (xHigherPriorityTaskWoken == pdTRUE)
    {
        portYIELD_FROM_ISR();
    }
}

/**
 * @brief Initialize GPIO for button input with interrupt
 * 
 * Configures the button GPIO pin as input with internal pull-up resistor
 * and sets up an interrupt on the falling edge (button press).
 */
static void init_gpio_button(void)
{
    // GPIO configuration structure
    gpio_config_t io_conf = {
        // Bit mask of the GPIO pin to configure
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        
        // Set as input mode
        .mode = GPIO_MODE_INPUT,
        
        // Enable internal pull-up resistor
        // The button should connect GPIO to GND when pressed
        .pull_up_en = GPIO_PULLUP_ENABLE,
        
        // Disable pull-down resistor
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        
        // Interrupt on falling edge (high to low transition = button press)
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    
    // Apply the configuration
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    
    // Install GPIO ISR service
    // 0 = default flags, allocates interrupt on any available CPU
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    
    // Attach our ISR handler to the button GPIO
    ESP_ERROR_CHECK(gpio_isr_handler_add(BUTTON_GPIO, gpio_isr_handler, NULL));
    
    ESP_LOGI(TAG, "Button GPIO %d configured with interrupt on falling edge", BUTTON_GPIO);
}

/**
 * @brief Initialize GPIOs for LED outputs
 * 
 * Configures GPIO pins 4 through 7 as outputs for the 4 LEDs.
 * All LEDs start in the OFF state (low level).
 * The LEDs will be controlled by Task4 in a sequential pattern.
 */
static void init_gpio_leds(void)
{
    // Create a bitmask for all 4 LED GPIOs (GPIO 4, 5, 6, 7)
    // Bit shift creates: 0b11110000 in the appropriate position
    uint64_t led_pin_mask = 0;
    for (int i = LED_GPIO_START; i <= LED_GPIO_END; i++)
    {
        led_pin_mask |= (1ULL << i);
    }
    
    // GPIO configuration structure for LED outputs
    gpio_config_t io_conf = {
        // Bit mask of the GPIO pins to configure (GPIOs 4, 5, 6, 7)
        .pin_bit_mask = led_pin_mask,
        
        // Set as output mode - we control these pins to turn LEDs on/off
        .mode = GPIO_MODE_OUTPUT,
        
        // Disable pull-up resistor (not needed for outputs)
        .pull_up_en = GPIO_PULLUP_DISABLE,
        
        // Disable pull-down resistor (not needed for outputs)
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        
        // No interrupt for output pins
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    // Apply the configuration to all LED GPIO pins
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    
    // Initialize all LEDs to OFF state (low level)
    // This ensures LEDs start in a known state
    for (int i = LED_GPIO_START; i <= LED_GPIO_END; i++)
    {
        gpio_set_level(i, 0);
    }
    
    ESP_LOGI(TAG, "LED GPIOs %d-%d configured as outputs (all OFF)", LED_GPIO_START, LED_GPIO_END);
}

/* ============================================================================
 * TASK CREATION
 * ============================================================================ */

/**
 * @brief Create all four application tasks
 * 
 * Creates the tasks in order of priority (lowest to highest).
 * Each task is created with its specified priority and stack size.
 * Error checking ensures tasks are created successfully.
 */
static void create_tasks(void)
{
    BaseType_t xReturned;
    
    // Create Task1 - Lowest priority continuous task
    xReturned = xTaskCreate(
        vTask1_Continuous,      // Task function
        "Task1",                // Task name (for debugging)
        TASK1_STACK_SIZE,       // Stack size in bytes
        NULL,                   // Task parameter (unused)
        TASK1_PRIORITY,         // Task priority (1 = lowest app priority)
        NULL                    // Task handle (not needed)
    );
    if (xReturned != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create Task1!");
    }
    else
    {
        ESP_LOGI(TAG, "Task1 created with priority %d", TASK1_PRIORITY);
    }
    
    // Create Task2 - Periodic 500ms task
    xReturned = xTaskCreate(
        vTask2_Periodic500ms,
        "Task2",
        TASK2_STACK_SIZE,
        NULL,
        TASK2_PRIORITY,
        NULL
    );
    if (xReturned != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create Task2!");
    }
    else
    {
        ESP_LOGI(TAG, "Task2 created with priority %d", TASK2_PRIORITY);
    }
    
    // Create Task3 - Periodic 3000ms task (runs for 5 seconds)
    xReturned = xTaskCreate(
        vTask3_Periodic3000ms,
        "Task3",
        TASK3_STACK_SIZE,
        NULL,
        TASK3_PRIORITY,
        NULL
    );
    if (xReturned != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create Task3!");
    }
    else
    {
        ESP_LOGI(TAG, "Task3 created with priority %d", TASK3_PRIORITY);
    }
    
    // Create Task4 - Highest priority semaphore-triggered task
    xReturned = xTaskCreate(
        vTask4_SemaphoreTriggered,
        "Task4",
        TASK4_STACK_SIZE,
        NULL,
        TASK4_PRIORITY,
        NULL
    );
    if (xReturned != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create Task4!");
    }
    else
    {
        ESP_LOGI(TAG, "Task4 created with priority %d", TASK4_PRIORITY);
    }
}

/* ============================================================================
 * MAIN APPLICATION ENTRY POINT
 * ============================================================================ */

/**
 * @brief Application entry point
 * 
 * This is the main function called by the ESP-IDF framework after
 * system initialization. It sets up the semaphore, GPIO, and creates
 * all four tasks. After this function returns, the FreeRTOS scheduler
 * takes over and begins running the tasks based on their priorities.
 */
void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "FreeRTOS 4-Task Priority Example");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Task1 (P1): Continuous, lowest priority");
    ESP_LOGI(TAG, "Task2 (P2): Periodic 500ms");
    ESP_LOGI(TAG, "Task3 (P3): Periodic 3000ms, runs 5s");
    ESP_LOGI(TAG, "Task4 (P4): Button-triggered, highest priority");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "LED Control: 4 LEDs on GPIOs %d-%d", LED_GPIO_START, LED_GPIO_END);
    ESP_LOGI(TAG, "Press BOOT button (GPIO %d) to light LEDs in sequence", BUTTON_GPIO);
    ESP_LOGI(TAG, "Hold button = LED stays on, Release = LED off");
    ESP_LOGI(TAG, "========================================");
    
    // Step 1: Create the binary semaphore for Task4
    // IMPORTANT: Semaphore must be created before creating Task4
    // Binary semaphores are ideal for interrupt-to-task synchronization
    xTask4Semaphore = xSemaphoreCreateBinary();
    if (xTask4Semaphore == NULL)
    {
        ESP_LOGE(TAG, "Failed to create semaphore!");
        return;  // Cannot continue without semaphore
    }
    ESP_LOGI(TAG, "Binary semaphore created for Task4");
    
    // Step 2: Initialize GPIO for button input with interrupt
    init_gpio_button();
    
    // Step 3: Initialize GPIO for LED outputs
    init_gpio_leds();
    
    // Step 4: Create all four tasks
    create_tasks();
    
    ESP_LOGI(TAG, "All tasks created. Scheduler running...");
    ESP_LOGI(TAG, "========================================");
    
    // app_main() can return - the scheduler is already running
    // The tasks will continue to execute based on their priorities
}
