/**
 * =============================================================================
 * 7-Segment Display Counter for ESP32-S3
 * =============================================================================
 * 
 * This program demonstrates GPIO control on the ESP32-S3 by driving a 
 * 7-segment display to count from 0 to 9 repeatedly. The display uses 7 LEDs
 * (segments a-g) arranged to form numeric digits.
 * 
 * 7-Segment Display Layout:
 * -------------------------
 *       aaaa
 *      f    b
 *      f    b
 *       gggg
 *      e    c
 *      e    c
 *       dddd
 * 
 * Hardware Setup:
 * ---------------
 * Connect the 7-segment display to the following GPIO pins:
 *   - Segment a (top):          GPIO 4
 *   - Segment b (top right):    GPIO 5
 *   - Segment c (bottom right): GPIO 6
 *   - Segment d (bottom):       GPIO 7
 *   - Segment e (bottom left):  GPIO 15
 *   - Segment f (top left):     GPIO 16
 *   - Segment g (middle):       GPIO 17
 * 
 * Each segment should be connected with a 220-330 ohm current-limiting resistor.
 * 
 * Display Types:
 * --------------
 * - Common Cathode: All cathodes connected to GND, segments are active HIGH
 * - Common Anode: All anodes connected to VCC, segments are active LOW
 * 
 * This code supports both types via the COMMON_ANODE configuration option.
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
 * DISPLAY TYPE CONFIGURATION
 * -----------------------------------------------------------------------------
 * Set to 1 for Common Anode display (segments active LOW)
 * Set to 0 for Common Cathode display (segments active HIGH)
 * -------------------------------------------------------------------------- */
#define COMMON_ANODE 0

/* -----------------------------------------------------------------------------
 * GPIO PIN DEFINITIONS FOR 7-SEGMENT DISPLAY
 * -----------------------------------------------------------------------------
 * Each segment of the display is connected to a specific GPIO pin.
 * These pins are chosen because they are general-purpose I/O pins on 
 * ESP32-S3 that are not used for boot strapping.
 * 
 * Modify these definitions if using different pins on your board.
 * -------------------------------------------------------------------------- */
#define SEG_A_GPIO  GPIO_NUM_4    /* Segment a - top horizontal */
#define SEG_B_GPIO  GPIO_NUM_5    /* Segment b - top right vertical */
#define SEG_C_GPIO  GPIO_NUM_6    /* Segment c - bottom right vertical */
#define SEG_D_GPIO  GPIO_NUM_7    /* Segment d - bottom horizontal */
#define SEG_E_GPIO  GPIO_NUM_15   /* Segment e - bottom left vertical */
#define SEG_F_GPIO  GPIO_NUM_16   /* Segment f - top left vertical */
#define SEG_G_GPIO  GPIO_NUM_17   /* Segment g - middle horizontal */

/* -----------------------------------------------------------------------------
 * TIMING CONFIGURATION
 * -----------------------------------------------------------------------------
 * Define how long each digit is displayed before moving to the next.
 * -------------------------------------------------------------------------- */
#define DIGIT_DISPLAY_TIME_MS  1000  /* Display each digit for 1 second */

/* -----------------------------------------------------------------------------
 * NUMBER OF SEGMENTS
 * -----------------------------------------------------------------------------
 * A standard 7-segment display has 7 segments (a through g).
 * -------------------------------------------------------------------------- */
#define NUM_SEGMENTS 7

/* -----------------------------------------------------------------------------
 * SEGMENT GPIO ARRAY
 * -----------------------------------------------------------------------------
 * Array of all segment GPIO pins for easy iteration.
 * Order: a, b, c, d, e, f, g (matching the bit order in digit patterns)
 * -------------------------------------------------------------------------- */
static const gpio_num_t segment_pins[NUM_SEGMENTS] = {
    SEG_A_GPIO,  /* Index 0 - Segment a */
    SEG_B_GPIO,  /* Index 1 - Segment b */
    SEG_C_GPIO,  /* Index 2 - Segment c */
    SEG_D_GPIO,  /* Index 3 - Segment d */
    SEG_E_GPIO,  /* Index 4 - Segment e */
    SEG_F_GPIO,  /* Index 5 - Segment f */
    SEG_G_GPIO   /* Index 6 - Segment g */
};

/* -----------------------------------------------------------------------------
 * DIGIT SEGMENT PATTERNS (For Common Cathode Display)
 * -----------------------------------------------------------------------------
 * Each digit is represented as a 7-bit pattern where:
 *   - Bit 0 = Segment a
 *   - Bit 1 = Segment b
 *   - Bit 2 = Segment c
 *   - Bit 3 = Segment d
 *   - Bit 4 = Segment e
 *   - Bit 5 = Segment f
 *   - Bit 6 = Segment g
 * 
 * A '1' bit means the segment is ON, '0' means OFF.
 * 
 * Visual representation of each digit:
 * 
 *   0: abcdef  (all except g)     5: acdFg   (a,c,d,f,g)
 *   1: bc      (b,c only)         6: acdefg  (all except b)
 *   2: abdeg   (a,b,d,e,g)        7: abc     (a,b,c only)
 *   3: abcdg   (a,b,c,d,g)        8: abcdefg (all segments)
 *   4: bcfg    (b,c,f,g)          9: abcdfg  (all except e)
 * -------------------------------------------------------------------------- */
static const uint8_t digit_patterns[10] = {
    /*        gfedcba  - bit positions */
    0b0111111,  /* 0: segments a,b,c,d,e,f ON  (g OFF) */
    0b0000110,  /* 1: segments b,c ON */
    0b1011011,  /* 2: segments a,b,d,e,g ON */
    0b1001111,  /* 3: segments a,b,c,d,g ON */
    0b1100110,  /* 4: segments b,c,f,g ON */
    0b1101101,  /* 5: segments a,c,d,f,g ON */
    0b1111101,  /* 6: segments a,c,d,e,f,g ON */
    0b0000111,  /* 7: segments a,b,c ON */
    0b1111111,  /* 8: all segments ON */
    0b1101111   /* 9: segments a,b,c,d,f,g ON */
};

/* -----------------------------------------------------------------------------
 * FUNCTION: configure_segment_pins
 * -----------------------------------------------------------------------------
 * Description:
 *   Initializes all 7 segment GPIO pins as outputs. Sets all segments to OFF
 *   initially to ensure the display starts in a known state.
 * 
 * Parameters:
 *   None
 * 
 * Returns:
 *   None
 * -------------------------------------------------------------------------- */
static void configure_segment_pins(void)
{
    printf("Configuring GPIO pins for 7-segment display...\n");
    
    /* Array of segment names for debug output */
    const char* segment_names[NUM_SEGMENTS] = {"a", "b", "c", "d", "e", "f", "g"};
    
    /* Loop through each segment pin and configure it as output */
    for (int i = 0; i < NUM_SEGMENTS; i++) {
        /* Reset the GPIO pin to default state before configuration */
        gpio_reset_pin(segment_pins[i]);
        
        /* Set the GPIO pin direction to OUTPUT mode */
        gpio_set_direction(segment_pins[i], GPIO_MODE_OUTPUT);
        
        /* Initialize segment to OFF state
         * For common cathode: OFF = LOW (0)
         * For common anode: OFF = HIGH (1) */
        int off_level = COMMON_ANODE ? 1 : 0;
        gpio_set_level(segment_pins[i], off_level);
        
        printf("  - GPIO %2d configured as output (Segment %s)\n", 
               segment_pins[i], segment_names[i]);
    }
    
    printf("GPIO configuration complete.\n\n");
}

/* -----------------------------------------------------------------------------
 * FUNCTION: display_digit
 * -----------------------------------------------------------------------------
 * Description:
 *   Displays a single digit (0-9) on the 7-segment display by setting the
 *   appropriate segments ON or OFF according to the digit pattern.
 * 
 * Parameters:
 *   digit - The digit to display (0-9). Values outside this range are ignored.
 * 
 * Returns:
 *   None
 * 
 * Notes:
 *   - For common cathode displays: segment ON = GPIO HIGH
 *   - For common anode displays: segment ON = GPIO LOW (inverted)
 * -------------------------------------------------------------------------- */
static void display_digit(int digit)
{
    /* Validate input - only accept digits 0-9 */
    if (digit < 0 || digit > 9) {
        printf("Error: Invalid digit %d (must be 0-9)\n", digit);
        return;
    }
    
    /* Get the segment pattern for this digit */
    uint8_t pattern = digit_patterns[digit];
    
    /* Set each segment according to the pattern */
    for (int seg = 0; seg < NUM_SEGMENTS; seg++) {
        /* Extract the bit for this segment (bit 0 = seg a, bit 1 = seg b, etc.) */
        int segment_on = (pattern >> seg) & 0x01;
        
        /* For common anode displays, invert the logic
         * Common cathode: ON = HIGH, OFF = LOW
         * Common anode: ON = LOW, OFF = HIGH */
        int gpio_level;
        if (COMMON_ANODE) {
            gpio_level = segment_on ? 0 : 1;  /* Inverted for common anode */
        } else {
            gpio_level = segment_on;           /* Normal for common cathode */
        }
        
        /* Set the GPIO level for this segment */
        gpio_set_level(segment_pins[seg], gpio_level);
    }
}

/* -----------------------------------------------------------------------------
 * FUNCTION: clear_display
 * -----------------------------------------------------------------------------
 * Description:
 *   Turns OFF all segments of the 7-segment display.
 * 
 * Parameters:
 *   None
 * 
 * Returns:
 *   None
 * -------------------------------------------------------------------------- */
static void clear_display(void)
{
    for (int seg = 0; seg < NUM_SEGMENTS; seg++) {
        /* Set segment to OFF state
         * Common cathode: OFF = LOW
         * Common anode: OFF = HIGH */
        int off_level = COMMON_ANODE ? 1 : 0;
        gpio_set_level(segment_pins[seg], off_level);
    }
}

/* -----------------------------------------------------------------------------
 * FUNCTION: app_main
 * -----------------------------------------------------------------------------
 * Description:
 *   Main entry point for the ESP32-S3 application. This function:
 *   1. Prints a startup banner with configuration info
 *   2. Configures all GPIO pins for the 7-segment display
 *   3. Enters an infinite loop that counts 0->9 repeatedly
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
     * Print program information to serial monitor
     * --------------------------------------------------------------------- */
    printf("\n");
    printf("================================================\n");
    printf("   7-Segment Display Counter for ESP32-S3\n");
    printf("   UCSC Embedded Firmware Essentials\n");
    printf("================================================\n");
    printf("Display type: %s\n", COMMON_ANODE ? "Common Anode" : "Common Cathode");
    printf("Digit display time: %d ms\n", DIGIT_DISPLAY_TIME_MS);
    printf("================================================\n");
    printf("\nSegment to GPIO mapping:\n");
    printf("  Segment a (top):          GPIO %d\n", SEG_A_GPIO);
    printf("  Segment b (top right):    GPIO %d\n", SEG_B_GPIO);
    printf("  Segment c (bottom right): GPIO %d\n", SEG_C_GPIO);
    printf("  Segment d (bottom):       GPIO %d\n", SEG_D_GPIO);
    printf("  Segment e (bottom left):  GPIO %d\n", SEG_E_GPIO);
    printf("  Segment f (top left):     GPIO %d\n", SEG_F_GPIO);
    printf("  Segment g (middle):       GPIO %d\n", SEG_G_GPIO);
    printf("================================================\n\n");
    
    /* -------------------------------------------------------------------------
     * GPIO INITIALIZATION
     * Configure all segment pins as outputs
     * --------------------------------------------------------------------- */
    configure_segment_pins();
    
    /* Ensure display starts blank */
    clear_display();
    
    /* -------------------------------------------------------------------------
     * MAIN COUNTING LOOP
     * Count from 0 to 9 repeatedly, displaying each digit for 1 second
     * --------------------------------------------------------------------- */
    printf("Starting 0-9 counter...\n\n");
    
    while (1) {
        /* Loop through digits 0 to 9 */
        for (int digit = 0; digit <= 9; digit++) {
            /* Print current digit to serial monitor for debugging */
            printf("Displaying: %d\n", digit);
            
            /* Display the digit on the 7-segment display */
            display_digit(digit);
            
            /* Wait before showing the next digit
             * vTaskDelay uses FreeRTOS ticks, pdMS_TO_TICKS converts ms to ticks */
            vTaskDelay(pdMS_TO_TICKS(DIGIT_DISPLAY_TIME_MS));
        }
        
        /* Print separator when sequence completes */
        printf("--- Sequence complete, restarting ---\n\n");
    }
    
    /* Note: This point is never reached due to infinite loop */
}
