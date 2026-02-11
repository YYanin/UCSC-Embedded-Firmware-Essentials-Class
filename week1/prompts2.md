# Prompts for 7-Segment Display Counter Project

## Summary

This document contains all prompts necessary to complete the 7-Segment Display Counter project for the ESP32-S3. The project expands on the LED blinking project by using a 7-segment display to count from 0 to 9 and repeat continuously.

The 7-segment display consists of 7 LEDs (segments a-g) arranged to form digits. By controlling which segments are ON or OFF, we can display numbers 0-9.

```
    aaaa
   f    b
   f    b
    gggg
   e    c
   e    c
    dddd
```

---

## Phase 1: Project Setup and Hardware Configuration

### Prompt 1.1
"Create a new ESP-IDF project for the 7-segment display counter:
```
get_idf
mkdir -p ~/esp/projects/seven_segment_counter/main
cd ~/esp/projects/seven_segment_counter
```
Create the project structure with CMakeLists.txt and main/CMakeLists.txt files."

### Manual Tests to Verify Completion of Prompt 1.1
- [ ] Directory ~/esp/projects/seven_segment_counter exists
- [ ] Project CMakeLists.txt file exists
- [ ] main/CMakeLists.txt file exists
- [ ] main/main.c file exists with initial structure

---

### Prompt 1.2
"Define the GPIO pin mapping for the 7-segment display. Use the following GPIO pins for each segment:
- Segment a: GPIO 4
- Segment b: GPIO 5
- Segment c: GPIO 6
- Segment d: GPIO 7
- Segment e: GPIO 15
- Segment f: GPIO 16
- Segment g: GPIO 17

Create a lookup table that maps each digit (0-9) to the appropriate segment pattern."

### Manual Tests to Verify Completion of Prompt 1.2
- [ ] GPIO pins defined as constants in code
- [ ] Segment pattern lookup table created for digits 0-9
- [ ] Each pattern correctly represents the digit shape
- [ ] Comments explain the segment mapping

---

## Phase 2: Implement Display Logic

### Prompt 2.1
"Write the main.c file with the following functionality:
1. Configure all 7 GPIO pins as outputs
2. Create a function to display a single digit (0-9) on the 7-segment display
3. Create the main loop that counts from 0 to 9 with a 1-second delay between digits
4. Include detailed comments explaining each section of the code
5. Handle both common cathode and common anode display types"

### Manual Tests to Verify Completion of Prompt 2.1
- [ ] All 7 GPIO pins configured as outputs
- [ ] display_digit() function implemented correctly
- [ ] Segment patterns correctly light up each digit
- [ ] Main loop counts 0->1->2->...->9->0 continuously
- [ ] Code includes detailed comments throughout

---

### Prompt 2.2
"Add serial monitor output to display the current digit being shown. This helps with debugging without physical hardware connected."

### Manual Tests to Verify Completion of Prompt 2.2
- [ ] Serial monitor shows current digit (e.g., "Displaying: 5")
- [ ] Output is clear and readable
- [ ] Timing matches the display changes

---

## Phase 3: Build, Flash, and Test

### Prompt 3.1
"Build the 7-segment counter project for ESP32-S3:
```
get_idf
cd ~/esp/projects/seven_segment_counter
idf.py set-target esp32s3
idf.py build
```
Verify the build completes without errors."

### Manual Tests to Verify Completion of Prompt 3.1
- [ ] Build completes without errors or warnings
- [ ] Binary file seven_segment_counter.bin generated
- [ ] Build output shows correct target (esp32s3)

---

### Prompt 3.2
"Flash the project to the ESP32-S3 and open the serial monitor:
```
idf.py -p /dev/ttyACM0 flash monitor
```
Verify the program runs and displays counting sequence in serial output."

### Manual Tests to Verify Completion of Prompt 3.2
- [ ] Flash completes without errors
- [ ] Serial monitor shows program banner
- [ ] Serial output shows digits cycling 0-9
- [ ] GPIO configuration messages displayed

---

### Prompt 3.3
"Connect the 7-segment display to the ESP32-S3 and verify the hardware displays digits correctly. Record a demo video if required."

### Manual Tests to Verify Completion of Prompt 3.3
- [ ] 7-segment display connected to correct GPIO pins
- [ ] Resistors (220-330 ohm) in place for each segment
- [ ] Display shows digits 0-9 in sequence
- [ ] Timing is approximately 1 second per digit
- [ ] Demo video recorded (if required)

---

## Success Criteria

### Project Setup (Phase 1)
- [x] Project directory structure created
- [x] CMakeLists.txt files configured
- [x] GPIO pins mapped to segments a-g
- [x] Digit lookup table implemented

### Display Logic (Phase 2)
- [x] GPIO configuration function working
- [x] display_digit() function correctly shows each digit
- [x] Main loop counts 0-9 continuously
- [x] Serial debugging output implemented
- [x] Code fully commented

### Build and Test (Phase 3)
- [x] Project builds successfully
- [x] Program flashes to ESP32-S3
- [x] Serial monitor shows correct output
- [x] Hardware display works correctly (when connected)

---

## Additional Notes

1. **7-Segment Display Types**:
   - Common Cathode: All cathodes connected to GND, segments are active HIGH
   - Common Anode: All anodes connected to VCC, segments are active LOW
   - The code should support configuration for either type

2. **Hardware Connections**:
   ```
   ESP32-S3 GPIO -> 220 ohm resistor -> 7-Segment Pin
   
   GPIO 4  -> Segment a (top)
   GPIO 5  -> Segment b (top right)
   GPIO 6  -> Segment c (bottom right)
   GPIO 7  -> Segment d (bottom)
   GPIO 15 -> Segment e (bottom left)
   GPIO 16 -> Segment f (top left)
   GPIO 17 -> Segment g (middle)
   ```

3. **Segment Patterns for Digits (Common Cathode)**:
   ```
   Digit | a b c d e f g
   ------|--------------
     0   | 1 1 1 1 1 1 0
     1   | 0 1 1 0 0 0 0
     2   | 1 1 0 1 1 0 1
     3   | 1 1 1 1 0 0 1
     4   | 0 1 1 0 0 1 1
     5   | 1 0 1 1 0 1 1
     6   | 1 0 1 1 1 1 1
     7   | 1 1 1 0 0 0 0
     8   | 1 1 1 1 1 1 1
     9   | 1 1 1 1 0 1 1
   ```

4. **Timing Configuration**:
   - Default: 1000ms (1 second) per digit
   - Configurable via #define DIGIT_DISPLAY_TIME_MS

5. **Troubleshooting**:
   - If display shows inverted pattern, switch between common cathode/anode mode
   - If segments don't light, check resistor values and connections
   - Use serial monitor to verify GPIO state changes

6. **Project Location**: ~/esp/projects/seven_segment_counter

7. **Exit Serial Monitor**: Use Ctrl+] to exit idf.py monitor
