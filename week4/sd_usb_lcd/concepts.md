# Big Concepts - Week 4 Project

## 1. File System Abstraction Layers

```
Application Layer:    fopen("/sdcard/test.txt", "w")
                              |
VFS Layer:            ESP-IDF Virtual File System
                      Maps paths to drivers
                              |
FAT Layer:            Cluster allocation, directory entries
                      File Allocation Table management
                              |
Block Layer:          sdmmc_read_sectors() / sdmmc_write_sectors()
                      512-byte sector addressing
                              |
Hardware Layer:       SDMMC peripheral, GPIO signals
```

**Concept**: Operating systems abstract storage into layers. USB MSC bypasses VFS/FAT and talks directly to block layer - that's why both can coexist.

---

## 2. Peripheral Communication Protocols

| Protocol | Topology | Signals | Speed | Use Case |
|----------|----------|---------|-------|----------|
| **SDMMC** | Point-to-point | CLK, CMD, D0-D3 | 50 MHz | High-speed storage |
| **SPI** | Master-slave bus | MOSI, MISO, CLK, CS | ~25 MHz | General peripherals |
| **I2C** | Multi-master bus | SDA, SCL | 100-400 kHz | Low-pin-count devices |

**Concept**: Choose protocol based on speed needs, pin count, and device support. SDMMC > SPI for SD cards. I2C ideal for LCD (only 2 pins).

---

## 3. I2C Bus Electrical Requirements

```
    3.3V
     |
    [1k]  <-- External pull-up resistor
     |
SDA -+---- [Master]---- [Slave 1]---- [Slave 2]
           ESP32         LCD 0x27     MPU6050 0x68
```

**Concept**: I2C is open-drain - devices can only pull LOW, not HIGH. External resistors pull bus HIGH when released. Internal pull-ups (~45k) are too weak for capacitive loads like long wires or multiple devices.

---

## 4. Parallel LCD Interface via I2C Expander

```
I2C Bus (2 wires)          PCF8574              HD44780 LCD
                         I/O Expander           (Parallel)
    SDA ----+         +-------------+        +-------------+
    SCL ----+-------->| 8-bit Latch |------->| D4-D7 (4b)  |
                      |             |        | RS, E, RW   |
                      +-------------+        | Backlight   |
                                             +-------------+
```

**Concept**: I/O expanders convert serial protocols to parallel GPIO. One I2C write sets 8 output pins simultaneously. 4-bit mode reduces pins needed (send byte as two 4-bit nibbles).

---

## 5. USB Device Classes

```
USB Host (PC)
    |
    +-- USB HID (keyboard, mouse)
    +-- USB CDC (serial port)
    +-- USB MSC (mass storage)  <-- This project
    +-- USB Audio
    +-- USB Video
```

**Concept**: USB device classes define standard protocols. MSC class makes any device appear as a disk drive - no custom drivers needed. PC sends SCSI commands (READ_10, WRITE_10) which map to sector read/write.

---

## 6. Real-Time Operating System (RTOS) Primitives

| Primitive | Purpose | This Project |
|-----------|---------|--------------|
| **Task** | Independent execution thread | `clock_task` - updates LCD |
| **Timer** | Periodic callback | `rtc_timer` - 1Hz tick |
| **Mutex** | Mutual exclusion | `sd_access_mutex` - protects SD |
| **Semaphore** | Signaling/counting | (Not used here) |
| **Queue** | Inter-task messaging | (Not used here) |

**Concept**: RTOS provides building blocks for concurrent embedded systems. Tasks have their own stack. Timers run in daemon context (shared stack). Mutexes prevent race conditions.

---

## 7. Volatile Variables and Atomicity

```c
static volatile bool usb_msc_active = false;
```

**Concept**: `volatile` tells compiler the variable can change outside normal program flow (interrupts, other tasks). Without it, compiler might cache the value in a register and miss updates. Single-byte reads/writes are atomic on ARM - no mutex needed for simple flags.

---

## 8. Non-Blocking vs Blocking Delays

```c
// BLOCKING (bad for LCD in task context):
vTaskDelay(pdMS_TO_TICKS(1));  // Yields to scheduler, overhead

// NON-BLOCKING (good for LCD timing):
esp_rom_delay_us(500);  // Busy-wait, precise, no context switch
```

**Concept**: `vTaskDelay` releases CPU to other tasks - good for long waits. `esp_rom_delay_us` busy-waits - good for precise short delays. Using `vTaskDelay` for microsecond LCD timing caused DMA/memory issues due to scheduler overhead.

---

## 9. Callback-Driven Architecture

```
Event Source          Callback Function           Action
-----------          -----------------           ------
PC mounts USB   -->  usb_msc_mount_changed_cb  --> Set usb_msc_active=true
Timer expires   -->  rtc_timer_callback        --> Increment time
I2C complete    -->  (internal to driver)      --> Signal completion
```

**Concept**: Instead of polling "is USB connected?", register a callback that gets invoked when the event occurs. More efficient, cleaner separation of concerns.

---

## 10. Graceful Degradation Pattern

```c
ret = i2c_init();
if (ret != ESP_OK) {
    ESP_LOGW(TAG, "Continuing without LCD...");
    // Don't return - keep going
} else {
    lcd_init();
}
```

**Concept**: Embedded systems should handle partial failures. Core function (SD card) is mandatory. Peripherals (LCD, USB) are optional enhancements. Log warnings but don't halt.

---

## Summary Table

| Concept | Why It Matters |
|---------|----------------|
| Abstraction layers | Enables different access methods (file API vs sectors) |
| Protocol selection | Match protocol to device requirements |
| Pull-up resistors | I2C electrical requirement for reliable communication |
| I/O expanders | Reduce pin count for parallel interfaces |
| USB device classes | Standard protocols = no custom drivers |
| RTOS primitives | Structured concurrency in embedded systems |
| Volatile keyword | Ensures visibility of shared variables |
| Delay types | Choose based on timing precision needs |
| Callbacks | Event-driven, efficient resource usage |
| Graceful degradation | System remains functional despite component failures |
