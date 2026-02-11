/**
 * =============================================================================
 * LED Sequence Program for ESP32-S3
 * =============================================================================
 * 
 * This program demonstrates GPIO control on the ESP32-S3 by blinking multiple
 * LEDs in a sequential pattern. Each LED turns on and off in order, creating
 * a "chasing" or "running light" effect.
 * 
 * Hardware Setup:
 * ---------------
 * Connect LEDs to the following GPIO pins (active HIGH):
 *   - LED 1: GPIO 4
 *   - LED 2: GPIO 5
 *   - LED 3: GPIO 6
 *   - LED 4: GPIO 7
 * 
 * Each LED should be connected with a 220-330 ohm current-limiting resistor
 * between the GPIO pin and the LED anode. The LED cathode connects to GND.
 * 
 *   GPIO Pin ----[220R]----[LED>|]---- GND
 * 
 * Sequence Pattern:
 * -----------------
 * The program runs through LEDs 1->2->3->4 in sequence, with each LED
 * staying on for a configurable duration before the next LED lights up.
 * 
 * Author: UCSC Embedded Firmware Essentials Class
 * Date: January 2026
 * Target: ESP32-S3
 * ESP-IDF Version: v5.3.2
 * 
 * =============================================================================
 */

/* -----------------------------------------------------------------------------
 * INCLUDE SECTION
 * -----------------------------------------------------------------------------
 * Include necessary header files for the program:
 * - stdio.h: Standard I/O for printf debugging
 * - freertos/FreeRTOS.h: FreeRTOS kernel definitions
 * - freertos/task.h: FreeRTOS task functions (vTaskDelay)
 * - driver/gpio.h: ESP-IDF GPIO driver for controlling pins
 * -------------------------------------------------------------------------- */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

/* -----------------------------------------------------------------------------
 * GPIO PIN DEFINITIONS
 * -----------------------------------------------------------------------------
 * Define the GPIO pins used for each LED. These pins are chosen because:
 * - They are general-purpose I/O pins on ESP32-S3
 * - They are not used for boot strapping
 * - They are easily accessible on most development boards
 * 
 * Modify these definitions if using different pins on your board.
 * -------------------------------------------------------------------------- */
#define LED_1_GPIO  GPIO_NUM_4   /* First LED connected to GPIO 4 */
#define LED_2_GPIO  GPIO_NUM_5   /* Second LED connected to GPIO 5 */
#define LED_3_GPIO  GPIO_NUM_6   /* Third LED connected to GPIO 6 */
#define LED_4_GPIO  GPIO_NUM_7   /* Fourth LED connected to GPIO 7 */

/* -----------------------------------------------------------------------------
 * TIMING CONFIGURATION
 * -----------------------------------------------------------------------------
 * Define timing constants for the LED sequence:
 * - LED_ON_TIME_MS: How long each LED stays on (milliseconds)
 * - LED_OFF_TIME_MS: Delay between turning off one LED and on the next
 * - SEQUENCE_DELAY_MS: Delay after completing one full sequence
 * -------------------------------------------------------------------------- */
#define LED_ON_TIME_MS      500   /* Each LED stays on for 500ms */
#define LED_OFF_TIME_MS     100   /* 100ms gap between LED transitions */
#define SEQUENCE_DELAY_MS   1000  /* 1 second pause after full sequence */

/* -----------------------------------------------------------------------------
 * LED COUNT AND ARRAY
 * -----------------------------------------------------------------------------
 * Store all LED GPIO pins in an array for easy iteration during the
 * sequence. This makes it simple to add or remove LEDs from the pattern.
 * -------------------------------------------------------------------------- */
#define NUM_LEDS 4  /* Total number of LEDs in the sequence */

/* Array holding all LED GPIO pin numbers for sequential access */
static const gpio_num_t led_pins[NUM_LEDS] = {
    LED_1_GPIO,
    LED_2_GPIO,
    LED_3_GPIO,
    LED_4_GPIO
};

/* -----------------------------------------------------------------------------
 * FUNCTION: configure_gpio_pins
 * -----------------------------------------------------------------------------
 * Description:
 *   Initializes all LED GPIO pins as outputs with no pull-up or pull-down
 *   resistors. Sets initial state of all LEDs to OFF.
 * 
 * Parameters:
 *   None
 * 
 * Returns:
 *   None
 * 
 * Notes:
 *   - Uses ESP-IDF GPIO driver functions
 *   - Must be called before attempting to control LEDs
 * -------------------------------------------------------------------------- */
static void configure_gpio_pins(void)
{
    printf("Configuring GPIO pins for LED control...\n");
    
    /* Loop through each LED pin and configure it */
    for (int i = 0; i < NUM_LEDS; i++) {
        /* Reset the GPIO pin to default state before configuration */
        gpio_reset_pin(led_pins[i]);
        
        /* Set the GPIO pin direction to OUTPUT mode
         * This allows us to control the voltage level on the pin */
        gpio_set_direction(led_pins[i], GPIO_MODE_OUTPUT);
        
        /* Initialize the LED to OFF state (LOW level)
         * This ensures all LEDs start in a known state */
        gpio_set_level(led_pins[i], 0);
        
        printf("  - GPIO %d configured as output (LED %d)\n", led_pins[i], i + 1);
    }
    
    printf("GPIO configuration complete.\n\n");
}

/* -----------------------------------------------------------------------------
 * FUNCTION: turn_on_led
 * -----------------------------------------------------------------------------
 * Description:
 *   Turns ON a specific LED by setting its GPIO pin HIGH.
 * 
 * Parameters:
 *   led_index - Index of the LED in the led_pins array (0 to NUM_LEDS-1)
 * 
 * Returns:
 *   None
 * -------------------------------------------------------------------------- */
static void turn_on_led(int led_index)
{
    if (led_index >= 0 && led_index < NUM_LEDS) {
        gpio_set_level(led_pins[led_index], 1);  /* Set HIGH to turn ON */
    }
}

/* -----------------------------------------------------------------------------
 * FUNCTION: turn_off_led
 * -----------------------------------------------------------------------------
 * Description:
 *   Turns OFF a specific LED by setting its GPIO pin LOW.
 * 
 * Parameters:
 *   led_index - Index of the LED in the led_pins array (0 to NUM_LEDS-1)
 * 
 * Returns:
 *   None
 * -------------------------------------------------------------------------- */
static void turn_off_led(int led_index)
{
    if (led_index >= 0 && led_index < NUM_LEDS) {
        gpio_set_level(led_pins[led_index], 0);  /* Set LOW to turn OFF */
    }
}

/* -----------------------------------------------------------------------------
 * FUNCTION: turn_off_all_leds
 * -----------------------------------------------------------------------------
 * Description:
 *   Turns OFF all LEDs in the sequence. Useful for initialization and
 *   resetting the display state.
 * 
 * Parameters:
 *   None
 * 
 * Returns:
 *   None
 * -------------------------------------------------------------------------- */
static void turn_off_all_leds(void)
{
    for (int i = 0; i < NUM_LEDS; i++) {
        turn_off_led(i);
    }
}

/* -----------------------------------------------------------------------------
 * FUNCTION: run_led_sequence
 * -----------------------------------------------------------------------------
 * Description:
 *   Executes one complete LED sequence cycle. Each LED turns on in order
 *   (1 -> 2 -> 3 -> 4), stays on for LED_ON_TIME_MS, then turns off with
 *   a brief LED_OFF_TIME_MS delay before the next LED lights up.
 * 
 * Parameters:
 *   None
 * 
 * Returns:
 *   None
 * 
 * Sequence Timing:
 *   LED1 ON -> wait -> LED1 OFF -> short delay ->
 *   LED2 ON -> wait -> LED2 OFF -> short delay ->
 *   LED3 ON -> wait -> LED3 OFF -> short delay ->
 *   LED4 ON -> wait -> LED4 OFF -> sequence delay
 * -------------------------------------------------------------------------- */
static void run_led_sequence(void)
{
    /* Iterate through each LED in the sequence */
    for (int i = 0; i < NUM_LEDS; i++) {
        /* Print status message for debugging via serial monitor */
        printf("LED %d ON  (GPIO %d)\n", i + 1, led_pins[i]);
        
        /* Turn ON the current LED */
        turn_on_led(i);
        
        /* Keep the LED on for the specified duration
         * vTaskDelay uses FreeRTOS ticks, pdMS_TO_TICKS converts ms to ticks */
        vTaskDelay(pdMS_TO_TICKS(LED_ON_TIME_MS));
        
        /* Turn OFF the current LED */
        turn_off_led(i);
        printf("LED %d OFF\n", i + 1);
        
        /* Brief delay before lighting the next LED
         * This creates a visible gap in the sequence */
        vTaskDelay(pdMS_TO_TICKS(LED_OFF_TIME_MS));
    }
    
    /* Print separator for clarity in serial monitor output */
    printf("--- Sequence complete ---\n\n");
}

/* -----------------------------------------------------------------------------
 * FUNCTION: app_main
 * -----------------------------------------------------------------------------
 * Description:
 *   Main entry point for the ESP32-S3 application. This function is called
 *   by the ESP-IDF framework after system initialization is complete.
 * 
 *   The function:
 *   1. Prints a startup banner
 *   2. Configures all GPIO pins for LED control
 *   3. Enters an infinite loop running the LED sequence
 * 
 * Parameters:
 *   None (ESP-IDF entry point signature)
 * 
 * Returns:
 *   None (runs indefinitely)
 * -------------------------------------------------------------------------- */
void app_main(void)
{
    /* -------------------------------------------------------------------------
     * STARTUP BANNER
     * Print program information to serial monitor for identification
     * --------------------------------------------------------------------- */
    printf("\n");
    printf("============================================\n");
    printf("   LED Sequence Program for ESP32-S3\n");
    printf("   UCSC Embedded Firmware Essentials\n");
    printf("============================================\n");
    printf("Number of LEDs: %d\n", NUM_LEDS);
    printf("LED ON time: %d ms\n", LED_ON_TIME_MS);
    printf("LED OFF time: %d ms\n", LED_OFF_TIME_MS);
    printf("Sequence delay: %d ms\n", SEQUENCE_DELAY_MS);
    printf("============================================\n\n");
    
    /* -------------------------------------------------------------------------
     * GPIO INITIALIZATION
     * Configure all LED pins as outputs before starting the sequence
     * --------------------------------------------------------------------- */
    configure_gpio_pins();
    
    /* Ensure all LEDs start in OFF state */
    turn_off_all_leds();
    
    /* -------------------------------------------------------------------------
     * MAIN LOOP
     * Continuously run the LED sequence pattern
     * The loop runs forever - this is typical for embedded applications
     * --------------------------------------------------------------------- */
    printf("Starting LED sequence...\n\n");
    
    while (1) {
        /* Execute one complete LED sequence cycle */
        run_led_sequence();
        
        /* Wait before starting the next sequence cycle
         * This pause makes it easier to see when one cycle ends 
         * and the next begins */
        vTaskDelay(pdMS_TO_TICKS(SEQUENCE_DELAY_MS));
    }
    
    /* Note: This point is never reached because of the infinite loop above.
     * In embedded systems, the main function typically never returns. */
}
