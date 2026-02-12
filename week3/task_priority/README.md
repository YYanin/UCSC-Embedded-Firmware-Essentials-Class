# FreeRTOS 4-Task Priority Example for ESP32-S3

A demonstration of FreeRTOS task scheduling, priority preemption, semaphore-based synchronization, and sequential LED control on the ESP32-S3 microcontroller.

## Overview

This project implements four concurrent tasks with different priorities to demonstrate how FreeRTOS handles task scheduling and preemption. Task4 also controls 4 LEDs in a sequential pattern:

| Task   | Priority | Behavior | Output |
|--------|----------|----------|--------|
| Task1  | 1 (Lowest) | Runs continuously when idle | `Tsk1-P1` |
| Task2  | 2 | Periodic every 500ms | `Tsk2-P2 <in>` / `Tsk2-P2 <out>` |
| Task3  | 3 | Periodic every 3000ms, runs ~5s | `Tsk3-P3 <in>` / `Tsk3-P3 <out>` |
| Task4  | 4 (Highest) | Button-triggered, controls LEDs in sequence | `Tsk4-P4 <- LEDn ON` / `Tsk4-P4 -> LEDn OFF` |

## Key Concepts Demonstrated

- **Task Priorities**: Higher priority tasks preempt lower priority tasks
- **Periodic Tasks**: Using `vTaskDelayUntil()` for accurate timing
- **Binary Semaphores**: Interrupt-to-task synchronization
- **GPIO Interrupts**: Button press detection with ISR
- **GPIO Outputs**: LED control with sequential state machine
- **Task States**: Running, Ready, and Blocked states

## Hardware Requirements

- ESP32-S3 development board
- USB cable (data capable)
- BOOT button (GPIO 0) - typically built into the dev board
- 4 LEDs connected to GPIOs 4, 5, 6, and 7 (active high)

## Software Requirements

- ESP-IDF v5.3.x or later
- Python 3.8+
- CMake and Ninja build tools

## Building and Flashing

1. **Set up ESP-IDF environment**:
   ```bash
   source ~/esp/esp-idf/export.sh
   ```

2. **Navigate to project directory**:
   ```bash
   cd ~/esp/projects/week3/task_priority
   ```

3. **Set target to ESP32-S3** (first time only):
   ```bash
   idf.py set-target esp32s3
   ```

4. **Build the project**:
   ```bash
   idf.py build
   ```

5. **Flash and monitor**:
   ```bash
   idf.py -p /dev/ttyACM0 flash monitor
   ```
   
   Replace `/dev/ttyACM0` with your actual serial port if different.

6. **Exit monitor**: Press `Ctrl+]`

## Usage

Once flashed, the program starts automatically and displays task output on the serial monitor:

### Normal Operation

```
Tsk1-P1          <-- Task1 running (lowest priority, fills idle time)
Tsk1-P1
Tsk2-P2 <in>     <-- Task2 wakes up (preempts Task1)
Tsk2-P2 <out>    <-- Task2 blocks
Tsk1-P1          <-- Task1 resumes
...
Tsk3-P3 <in>     <-- Task3 wakes up every 3 seconds (preempts Task1 & Task2)
...              <-- Task3 runs for ~5 seconds
Tsk3-P3 <out>    <-- Task3 blocks
```

### Button Press (Task4) - LED Sequencing

Press and hold the **BOOT button (GPIO 0)** to trigger Task4 and light an LED:

```
Tsk4-P4 <- LED0 (GPIO4) ON    <-- Button pressed, LED0 lights up
                               <-- LED stays on while button is held
Tsk4-P4 -> LED0 (GPIO4) OFF   <-- Button released, LED0 turns off

(Next button press)
Tsk4-P4 <- LED1 (GPIO5) ON    <-- Button pressed, LED1 lights up
Tsk4-P4 -> LED1 (GPIO5) OFF   <-- Button released, LED1 turns off

(Continues in sequence: LED2 -> LED3 -> LED0 -> LED1 -> ...)
```

**LED Sequence Behavior:**
- Press 1: GPIO4 (LED0) lights
- Press 2: GPIO5 (LED1) lights
- Press 3: GPIO6 (LED2) lights
- Press 4: GPIO7 (LED3) lights
- Press 5: GPIO4 (LED0) lights (wraps around)

Task4 preempts ALL other tasks when active.

## Output Symbol Guide

| Symbol | Meaning |
|--------|---------|
| `<in>` | Task entering active state |
| `<out>` | Task exiting active state (about to block) |
| `<- LEDn ON` | Task4 is running, LED n is lit (arrow pointing in) |
| `-> LEDn OFF` | Task4 is blocking, LED n turned off (arrow pointing out) |

## Project Structure

```
task_priority/
├── CMakeLists.txt          # Top-level CMake configuration
├── README.md               # This file
├── sdkconfig               # ESP-IDF configuration
├── main/
│   ├── CMakeLists.txt      # Main component configuration
│   └── main.c              # Application source code
└── build/                  # Build output (generated)
```

## Configuration

Key parameters can be modified in `main/main.c`:

```c
#define TASK1_DELAY_MS      100     // Task1 print interval
#define TASK2_PERIOD_MS     500     // Task2 period
#define TASK3_PERIOD_MS     3000    // Task3 period
#define TASK3_RUN_TIME_MS   5000    // Task3 execution time
#define TASK4_RUN_TICKS     10      // Task4 loop delay (~100ms)
#define BUTTON_GPIO         GPIO_NUM_0  // Button GPIO pin
#define LED_GPIO_START      GPIO_NUM_4  // First LED GPIO
#define LED_GPIO_END        GPIO_NUM_7  // Last LED GPIO
#define NUM_LEDS            4           // Total number of LEDs
```

## Technical Details

### FreeRTOS Configuration

- **Tick Rate**: 100Hz (1 tick = 10ms)
- **Stack Size**: 2048 bytes per task
- **Scheduler**: Preemptive with priority-based scheduling

### GPIO Configuration

- **Button**: GPIO 0 with internal pull-up resistor
- **Interrupt**: Falling edge (button press detection)
- **ISR**: Uses `xSemaphoreGiveFromISR()` for safe semaphore signaling
- **LEDs**: GPIOs 4-7 configured as outputs (active high)
- **LED Sequence**: Controlled by `current_led_index` variable (0-3)

### Task Synchronization

- **Task4 Semaphore**: Binary semaphore created with `xSemaphoreCreateBinary()`
- **Button ISR**: Gives semaphore to wake Task4
- **Button Hold**: Task4 polls GPIO level while running

## Troubleshooting

### Serial Port Not Found

```bash
ls /dev/ttyACM* /dev/ttyUSB*
```

If permission denied:
```bash
sudo usermod -a -G dialout $USER
# Log out and back in
```

### Flash Failed

1. Hold BOOT button while pressing RESET
2. Release both buttons
3. Run flash command again

### Build Errors

Ensure ESP-IDF environment is sourced:
```bash
source ~/esp/esp-idf/export.sh
```

## License

This project is part of the UCSC Silicon Valley Embedded Firmware Essentials course.

## Author

Generated with AI assistance for educational purposes.

## References

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/RTOS_book.html)
- [ESP32-S3 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)
