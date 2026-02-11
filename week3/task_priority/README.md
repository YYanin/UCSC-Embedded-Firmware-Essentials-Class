# FreeRTOS 4-Task Priority Example for ESP32-S3

A demonstration of FreeRTOS task scheduling, priority preemption, and semaphore-based synchronization on the ESP32-S3 microcontroller.

## Overview

This project implements four concurrent tasks with different priorities to demonstrate how FreeRTOS handles task scheduling and preemption:

| Task   | Priority | Behavior | Output |
|--------|----------|----------|--------|
| Task1  | 1 (Lowest) | Runs continuously when idle | `Tsk1-P1` |
| Task2  | 2 | Periodic every 500ms | `Tsk2-P2 <in>` / `Tsk2-P2 <out>` |
| Task3  | 3 | Periodic every 3000ms, runs ~5s | `Tsk3-P3 <in>` / `Tsk3-P3 <out>` |
| Task4  | 4 (Highest) | Button-triggered, runs while held | `Tsk4-P4 <-` / `Tsk4-P4 ->` |

## Key Concepts Demonstrated

- **Task Priorities**: Higher priority tasks preempt lower priority tasks
- **Periodic Tasks**: Using `vTaskDelayUntil()` for accurate timing
- **Binary Semaphores**: Interrupt-to-task synchronization
- **GPIO Interrupts**: Button press detection with ISR
- **Task States**: Running, Ready, and Blocked states

## Hardware Requirements

- ESP32-S3 development board
- USB cable (data capable)
- BOOT button (GPIO 0) - typically built into the dev board

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

### Button Press (Task4)

Press and hold the **BOOT button (GPIO 0)** to trigger Task4:

```
Tsk4-P4 <-       <-- Button pressed, Task4 running (highest priority)
Tsk4-P4 <-       <-- Button still held
Tsk4-P4 <-       <-- Continues while held...
Tsk4-P4 ->       <-- Button released, Task4 blocks
```

Task4 preempts ALL other tasks when active.

## Output Symbol Guide

| Symbol | Meaning |
|--------|---------|
| `<in>` | Task entering active state |
| `<out>` | Task exiting active state (about to block) |
| `<-` | Task4 is running (arrow pointing in) |
| `->` | Task4 is blocking (arrow pointing out) |

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
