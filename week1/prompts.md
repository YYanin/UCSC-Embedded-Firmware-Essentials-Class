# Prompts for Embedded Firmware Essentials Week 1 Assignment

## Summary

This document contains all prompts necessary to complete the Week 1 assignment for the UCSC Silicon Valley Embedded Firmware Essentials Class. The assignment involves setting up the ESP-IDF development environment for ESP32-S3, building and deploying a HelloWorld project, and creating an LED sequence program.

Note: The original README references GCC4MBED and Eclipse, but since we are using an ESP32-S3 board connected via USB, we will use the ESP-IDF (Espressif IoT Development Framework) as the development environment instead.

---

## Phase 1: Installation of Software Development Environment

### Prompt 1.1
"Install all system dependencies required for ESP-IDF development on Ubuntu. Run the following commands:
```
sudo apt-get update
sudo apt-get install -y git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
```
Provide a screenshot showing successful installation of all packages."

### Manual Tests to Verify Completion of Prompt 1.1
- [ ] Run `cmake --version` and verify CMake is installed
- [ ] Run `ninja --version` and verify Ninja is installed
- [ ] Run `python3 --version` and verify Python 3.8+ is installed
- [ ] Run `git --version` and verify Git is installed
- [ ] Screenshot saved showing package installation success

---

### Prompt 1.2
"Clone the ESP-IDF repository version v5.3.2 into the ~/esp directory:
```
mkdir -p ~/esp
cd ~/esp
git clone -b v5.3.2 --recursive https://github.com/espressif/esp-idf.git
```
Provide a screenshot showing the successful clone with all submodules."

### Manual Tests to Verify Completion of Prompt 1.2
- [ ] Directory ~/esp/esp-idf exists
- [ ] Run `ls ~/esp/esp-idf/components` and verify components directory is populated
- [ ] Run `git -C ~/esp/esp-idf describe --tags` and verify output shows v5.3.2
- [ ] Screenshot saved showing successful clone

---

### Prompt 1.3
"Run the ESP-IDF install script to set up the toolchain for ESP32-S3:
```
cd ~/esp/esp-idf
./install.sh esp32s3
```
Provide a screenshot showing the installation completed successfully."

### Manual Tests to Verify Completion of Prompt 1.3
- [ ] No errors during install script execution
- [ ] The script reports successful installation of all tools
- [ ] Screenshot saved showing successful toolchain installation

---

### Prompt 1.4
"Set up the ESP-IDF environment variables by adding an alias to your shell configuration:
```
echo 'alias get_idf=\". \$HOME/esp/esp-idf/export.sh\"' >> ~/.bashrc
source ~/.bashrc
```
Then test by running `get_idf` and provide a screenshot showing the environment is activated."

### Manual Tests to Verify Completion of Prompt 1.4
- [ ] Run `get_idf` command without errors
- [ ] After running get_idf, run `echo $IDF_PATH` and verify it shows ~/esp/esp-idf
- [ ] Run `idf.py --version` and verify ESP-IDF version is displayed
- [ ] Screenshot saved showing environment activation

---

## Phase 2: Build HelloWorld Project and Deploy to ESP32-S3

### Prompt 2.1
"Activate the ESP-IDF environment and build the hello_world example project:
```
get_idf
cd ~/esp/esp-idf/examples/get-started/hello_world
idf.py set-target esp32s3
idf.py build
```
Provide a screenshot showing the build completed successfully."

### Manual Tests to Verify Completion of Prompt 2.1
- [ ] Build completes without errors
- [ ] Binary files are generated in the build directory
- [ ] Run `ls ~/esp/esp-idf/examples/get-started/hello_world/build/*.bin` shows binary files
- [ ] Screenshot saved showing successful build output

---

### Prompt 2.2
"Connect the ESP32-S3 board via USB and identify the serial port:
```
ls /dev/ttyACM* /dev/ttyUSB*
```
If permission denied, add your user to the dialout group:
```
sudo usermod -a -G dialout $USER
```
Then log out and log back in. Provide a screenshot showing the detected serial port."

### Manual Tests to Verify Completion of Prompt 2.2
- [ ] Serial port is detected (typically /dev/ttyACM0 or /dev/ttyUSB0)
- [ ] No permission denied errors when accessing the port
- [ ] Run `groups` and verify dialout group membership if added
- [ ] Screenshot saved showing serial port detection

---

### Prompt 2.3
"Flash the hello_world project to the ESP32-S3 and open the serial monitor:
```
get_idf
cd ~/esp/esp-idf/examples/get-started/hello_world
idf.py -p /dev/ttyACM0 flash monitor
```
(Replace /dev/ttyACM0 with your actual port if different)
Provide a screenshot showing the flash completed and Hello World output in the monitor."

### Manual Tests to Verify Completion of Prompt 2.3
- [ ] Flash completes without errors (shows 100% progress)
- [ ] Serial monitor shows "Hello world!" output from the ESP32-S3
- [ ] Device restarts and runs the program successfully
- [ ] Screenshot saved showing flash success and monitor output
- [ ] Exit monitor using Ctrl+]

---

## Phase 3: LED Sequence Program

### Prompt 3.1
"Create a new ESP-IDF project for the LED sequence program:
```
get_idf
mkdir -p ~/esp/projects/led_sequence
cd ~/esp/projects/led_sequence
```
Create the project structure with CMakeLists.txt, main/CMakeLists.txt, and main/main.c files. The project should control multiple LEDs in a specific sequence pattern."

### Manual Tests to Verify Completion of Prompt 3.1
- [ ] Directory ~/esp/projects/led_sequence exists
- [ ] Project CMakeLists.txt file exists
- [ ] main/CMakeLists.txt file exists
- [ ] main/main.c file exists with placeholder code

---

### Prompt 3.2
"Write the LED sequence program in main/main.c that:
1. Configures GPIO pins for multiple LEDs (use appropriate GPIO pins for ESP32-S3)
2. Implements a sequence pattern where LEDs blink in order
3. Uses proper timing delays between LED state changes
4. Includes detailed comments explaining each section of the code
5. Runs in an infinite loop

Use FreeRTOS vTaskDelay for timing. Example GPIO pins for ESP32-S3: GPIO 1, 2, 3, 4 (verify with your board pinout)."

### Manual Tests to Verify Completion of Prompt 3.2
- [ ] Code compiles without warnings or errors
- [ ] Code includes detailed comments throughout
- [ ] GPIO pins are properly configured as outputs
- [ ] LED sequence logic is implemented correctly
- [ ] Timing delays are appropriate for visual verification

---

### Prompt 3.3
"Build and flash the LED sequence program to the ESP32-S3:
```
get_idf
cd ~/esp/projects/led_sequence
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```
Verify the LEDs blink in the correct sequence and timing."

### Manual Tests to Verify Completion of Prompt 3.3
- [ ] Build completes without errors
- [ ] Flash completes successfully
- [ ] LEDs physically connected to the specified GPIO pins
- [ ] LEDs blink in the correct sequence order
- [ ] Timing between LED changes matches the code specification

---

### Prompt 3.4
"Record a demo video showing the LED sequence working on the ESP32-S3 board. The video should clearly show:
1. The ESP32-S3 board with connected LEDs
2. The LEDs blinking in the proper sequence order
3. The correct timing between LED state changes

Save the video file for submission."

### Manual Tests to Verify Completion of Prompt 3.4
- [ ] Video clearly shows the ESP32-S3 board
- [ ] All LEDs are visible in the video
- [ ] LED sequence order is correct as per program design
- [ ] Timing matches the specified delays in the code
- [ ] Video quality is sufficient to demonstrate functionality

---

## Success Criteria

### Problem 1 - Installation (Phase 1)
- [x] All system dependencies installed successfully
- [x] ESP-IDF v5.3.2 cloned with all submodules
- [x] ESP32-S3 toolchain installed
- [x] Environment variables configured
- [x] Screenshots captured for all installation steps (submit in Word/PDF)

### Problem 2 - HelloWorld Build and Deploy (Phase 2)
- [x] hello_world example builds successfully for ESP32-S3
- [x] ESP32-S3 board detected on USB serial port
- [x] Firmware flashed successfully to the board
- [x] Serial monitor shows "Hello world!" output
- [x] Screenshots captured for build and deploy (submit in Word/PDF)

### Problem 3 - LED Sequence Program (Phase 3)
- [x] LED sequence code written with detailed comments
- [x] Code submitted in Word/PDF document
- [x] Demo video recorded showing LEDs blinking in proper order
- [x] Timing in video matches code specification
- [x] Video submitted for grading

---

## Additional Notes

1. **Hardware Requirements**: ESP32-S3 development board, USB cable (data capable, not charge-only), LEDs with appropriate resistors, breadboard and jumper wires for LED connections.

2. **GPIO Pin Selection**: Common GPIO pins for ESP32-S3 that are safe to use for LEDs: GPIO 1, 2, 3, 4, 5, 6, 7, 8, 9, 10. Avoid GPIO 0 (boot pin) and strapping pins.

3. **LED Circuit**: Each LED should be connected with a current-limiting resistor (220-330 ohm) between the GPIO pin and the LED anode, with the cathode connected to GND.

4. **Serial Port Variations**: The serial port may appear as /dev/ttyACM0, /dev/ttyACM1, /dev/ttyUSB0, or /dev/ttyUSB1 depending on your system configuration.

5. **Troubleshooting Flash Issues**: If flashing fails, hold the BOOT button while pressing RESET, then release both. This puts the ESP32-S3 into download mode.

6. **Documentation Reference**: Refer to Espressif-Doc.md in the workspace for detailed ESP-IDF setup instructions and troubleshooting tips.

7. **Exit Serial Monitor**: Use Ctrl+] to exit the idf.py monitor.

8. **Original Assignment Context**: The README mentions GCC4MBED and Eclipse (designed for MBED platforms), but since we are using ESP32-S3, ESP-IDF is the appropriate development framework.
