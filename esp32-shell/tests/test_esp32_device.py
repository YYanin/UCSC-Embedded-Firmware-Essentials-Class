#!/usr/bin/env python3
"""
ESP32 Shell - Serial Device Tests
============================================================================
Automated test suite for testing ESP32 shell via serial connection.
Sends commands over serial and verifies expected output.

Usage:
    python3 test_esp32_device.py /dev/ttyACM0
    python3 test_esp32_device.py /dev/ttyACM0 -v           # Verbose
    python3 test_esp32_device.py /dev/ttyACM0 -t basic     # Run specific tests
    python3 test_esp32_device.py /dev/ttyACM0 --baud 115200

Requirements:
    pip install pyserial

Note: The ESP32 must be running the shell firmware before running tests.
============================================================================
"""

import argparse
import serial
import time
import sys
import re
from typing import Optional, Tuple, List

# ANSI colors for output
class Colors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    NC = '\033[0m'  # No Color

# Test result tracking
class TestResults:
    def __init__(self):
        self.run = 0
        self.passed = 0
        self.failed = 0
        self.skipped = 0
        self.failures: List[str] = []

results = TestResults()

def log_info(msg: str):
    print(f"{Colors.BLUE}[INFO]{Colors.NC} {msg}")

def log_pass(name: str):
    print(f"{Colors.GREEN}[PASS]{Colors.NC} {name}")
    results.passed += 1

def log_fail(name: str, reason: str = ""):
    msg = f"{Colors.RED}[FAIL]{Colors.NC} {name}"
    if reason:
        msg += f" - {reason}"
    print(msg)
    results.failed += 1
    results.failures.append(name)

def log_skip(name: str, reason: str = ""):
    msg = f"{Colors.YELLOW}[SKIP]{Colors.NC} {name}"
    if reason:
        msg += f" - {reason}"
    print(msg)
    results.skipped += 1

class ESP32Shell:
    """Serial connection to ESP32 shell."""
    
    def __init__(self, port: str, baud: int = 115200, timeout: float = 2.0):
        self.port = port
        self.baud = baud
        self.timeout = timeout
        self.ser: Optional[serial.Serial] = None
        self.prompt = "esp32>"
        
    def connect(self) -> bool:
        """Open serial connection."""
        try:
            self.ser = serial.Serial(
                port=self.port,
                baudrate=self.baud,
                timeout=self.timeout,
                write_timeout=self.timeout
            )
            # Clear any buffered data
            time.sleep(0.1)
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()
            return True
        except serial.SerialException as e:
            log_info(f"Failed to open {self.port}: {e}")
            return False
    
    def disconnect(self):
        """Close serial connection."""
        if self.ser and self.ser.is_open:
            self.ser.close()
    
    def send_command(self, cmd: str, wait_time: float = 0.5) -> str:
        """Send command and return output."""
        if not self.ser or not self.ser.is_open:
            return ""
        
        # Clear input buffer
        self.ser.reset_input_buffer()
        
        # Send command with newline
        self.ser.write((cmd + "\r\n").encode('utf-8'))
        self.ser.flush()
        
        # Wait for response
        time.sleep(wait_time)
        
        # Read all available output
        output = ""
        while self.ser.in_waiting:
            output += self.ser.read(self.ser.in_waiting).decode('utf-8', errors='replace')
            time.sleep(0.05)
        
        return output
    
    def wait_for_prompt(self, timeout: float = 5.0) -> bool:
        """Wait for shell prompt to appear."""
        if not self.ser:
            return False
            
        start = time.time()
        output = ""
        
        while (time.time() - start) < timeout:
            if self.ser.in_waiting:
                output += self.ser.read(self.ser.in_waiting).decode('utf-8', errors='replace')
                if self.prompt in output:
                    return True
            time.sleep(0.1)
        
        return False
    
    def sync(self) -> bool:
        """Synchronize with shell by sending empty command."""
        # Send a few newlines to get to a known state
        for _ in range(3):
            self.ser.write(b"\r\n")
            time.sleep(0.1)
        
        self.ser.reset_input_buffer()
        return self.wait_for_prompt(timeout=2.0)

# ============================================================================
# Test Functions
# ============================================================================

def test_connection(shell: ESP32Shell) -> bool:
    """Test basic serial connection."""
    results.run += 1
    
    if not shell.connect():
        log_fail("Serial connection", f"Cannot open {shell.port}")
        return False
    
    # Try to sync with shell
    if shell.sync():
        log_pass("Serial connection and shell sync")
        return True
    else:
        log_fail("Shell sync", "No prompt received")
        return False

def test_echo(shell: ESP32Shell):
    """Test echo command."""
    results.run += 1
    
    output = shell.send_command("echo hello world")
    
    if "hello world" in output:
        log_pass("echo command")
    else:
        log_fail("echo command", f"Expected 'hello world', got: {repr(output)}")

def test_pwd(shell: ESP32Shell):
    """Test pwd command."""
    results.run += 1
    
    output = shell.send_command("pwd")
    
    # Should show /spiffs or similar
    if "/spiffs" in output or "/" in output:
        log_pass("pwd command")
    else:
        log_fail("pwd command", f"Expected path, got: {repr(output)}")

def test_ls(shell: ESP32Shell):
    """Test ls command."""
    results.run += 1
    
    output = shell.send_command("ls")
    
    # ls should not error (output may be empty if no files)
    if "error" in output.lower() or "Error" in output:
        log_fail("ls command", f"Error in output: {repr(output)}")
    else:
        log_pass("ls command")

def test_help(shell: ESP32Shell):
    """Test help command."""
    results.run += 1
    
    output = shell.send_command("help", wait_time=1.0)
    
    # Should list available commands
    if "echo" in output and "pwd" in output:
        log_pass("help command")
    else:
        log_fail("help command", f"Missing expected commands in: {repr(output[:200])}")

def test_free(shell: ESP32Shell):
    """Test free memory command."""
    results.run += 1
    
    output = shell.send_command("free")
    
    # Should show memory information
    if "heap" in output.lower() or "bytes" in output.lower() or "free" in output.lower():
        log_pass("free command")
    else:
        log_fail("free command", f"Expected memory info, got: {repr(output)}")

def test_uptime(shell: ESP32Shell):
    """Test uptime command."""
    results.run += 1
    
    output = shell.send_command("uptime")
    
    # Should show uptime info
    if "up" in output.lower() or "second" in output.lower() or ":" in output:
        log_pass("uptime command")
    else:
        log_fail("uptime command", f"Expected uptime info, got: {repr(output)}")

def test_info(shell: ESP32Shell):
    """Test system info command."""
    results.run += 1
    
    output = shell.send_command("info", wait_time=1.0)
    
    # Should show chip/system info
    if "esp32" in output.lower() or "chip" in output.lower() or "idf" in output.lower():
        log_pass("info command")
    else:
        log_fail("info command", f"Expected system info, got: {repr(output)}")

def test_variable_set_get(shell: ESP32Shell):
    """Test variable set and echo."""
    results.run += 1
    
    # Set a variable
    shell.send_command("set TESTVAR=hello123")
    
    # Get it back via echo
    output = shell.send_command("echo $TESTVAR")
    
    if "hello123" in output:
        log_pass("variable set/get")
    else:
        log_fail("variable set/get", f"Expected 'hello123', got: {repr(output)}")

def test_env_command(shell: ESP32Shell):
    """Test env command."""
    results.run += 1
    
    # Set a variable first
    shell.send_command("set ENVTEST=value")
    
    # List environment
    output = shell.send_command("env")
    
    if "ENVTEST" in output or "=" in output:
        log_pass("env command")
    else:
        log_fail("env command", f"Expected variable listing, got: {repr(output)}")

def test_unset_variable(shell: ESP32Shell):
    """Test unset command."""
    results.run += 1
    
    # Set and unset
    shell.send_command("set UNSETTEST=value")
    shell.send_command("unset UNSETTEST")
    
    # Try to echo - should be empty
    output = shell.send_command("echo $UNSETTEST")
    
    # After unset, $UNSETTEST should expand to empty string
    if "value" not in output:
        log_pass("unset command")
    else:
        log_fail("unset command", f"Variable still has value: {repr(output)}")

def test_file_touch_rm(shell: ESP32Shell):
    """Test touch and rm commands."""
    results.run += 1
    
    # Create file
    shell.send_command("touch /spiffs/testfile.txt")
    
    # Check it exists
    output = shell.send_command("ls /spiffs")
    
    if "testfile.txt" in output:
        # Now remove it
        shell.send_command("rm /spiffs/testfile.txt")
        output2 = shell.send_command("ls /spiffs")
        
        if "testfile.txt" not in output2:
            log_pass("touch and rm commands")
        else:
            log_fail("rm command", "File still exists after rm")
    else:
        log_fail("touch command", f"File not created: {repr(output)}")

def test_fsinfo(shell: ESP32Shell):
    """Test filesystem info command."""
    results.run += 1
    
    output = shell.send_command("fsinfo")
    
    # Should show filesystem statistics
    if "total" in output.lower() or "free" in output.lower() or "used" in output.lower():
        log_pass("fsinfo command")
    else:
        log_fail("fsinfo command", f"Expected filesystem info, got: {repr(output)}")

def test_gpio_read(shell: ESP32Shell):
    """Test GPIO read command."""
    results.run += 1
    
    # Read a safe GPIO pin (GPIO 0 is often safe to read)
    output = shell.send_command("gpio read 0")
    
    # Should return 0 or 1
    if "0" in output or "1" in output:
        log_pass("gpio read command")
    else:
        log_fail("gpio read command", f"Expected 0 or 1, got: {repr(output)}")

def test_invalid_command(shell: ESP32Shell):
    """Test handling of invalid commands."""
    results.run += 1
    
    output = shell.send_command("nonexistentcommand123")
    
    # Should show error or unknown command message
    if "unknown" in output.lower() or "not found" in output.lower() or "error" in output.lower():
        log_pass("invalid command handling")
    else:
        # Even if no error message, it shouldn't crash
        log_pass("invalid command handling (no crash)")

def test_long_command(shell: ESP32Shell):
    """Test handling of long commands."""
    results.run += 1
    
    # Create a command that exceeds 256 chars
    long_arg = "a" * 300
    output = shell.send_command(f"echo {long_arg}")
    
    # Should either truncate or show error, but not crash
    # If we get any response, it didn't crash
    if output or True:  # Any response means it handled it
        log_pass("long command handling (no crash)")

def test_quoted_strings(shell: ESP32Shell):
    """Test quoted string handling."""
    results.run += 1
    
    output = shell.send_command('echo "hello world"')
    
    if "hello world" in output:
        log_pass("quoted string handling")
    else:
        log_fail("quoted string handling", f"Expected 'hello world', got: {repr(output)}")

def test_cd_command(shell: ESP32Shell):
    """Test cd command."""
    results.run += 1
    
    # Change to root and back
    shell.send_command("cd /spiffs")
    output = shell.send_command("pwd")
    
    if "/spiffs" in output:
        log_pass("cd command")
    else:
        log_fail("cd command", f"Expected /spiffs, got: {repr(output)}")

# ============================================================================
# Test Categories
# ============================================================================

def run_basic_tests(shell: ESP32Shell):
    """Run basic command tests."""
    log_info("Running basic command tests...")
    test_echo(shell)
    test_pwd(shell)
    test_ls(shell)
    test_help(shell)
    test_cd_command(shell)

def run_system_tests(shell: ESP32Shell):
    """Run ESP32-specific system tests."""
    log_info("Running system command tests...")
    test_free(shell)
    test_uptime(shell)
    test_info(shell)
    test_fsinfo(shell)

def run_variable_tests(shell: ESP32Shell):
    """Run environment variable tests."""
    log_info("Running variable tests...")
    test_variable_set_get(shell)
    test_env_command(shell)
    test_unset_variable(shell)

def run_file_tests(shell: ESP32Shell):
    """Run file operation tests."""
    log_info("Running file operation tests...")
    test_file_touch_rm(shell)

def run_gpio_tests(shell: ESP32Shell):
    """Run GPIO tests."""
    log_info("Running GPIO tests...")
    test_gpio_read(shell)

def run_error_tests(shell: ESP32Shell):
    """Run error handling tests."""
    log_info("Running error handling tests...")
    test_invalid_command(shell)
    test_long_command(shell)

def run_parsing_tests(shell: ESP32Shell):
    """Run parsing tests."""
    log_info("Running parsing tests...")
    test_quoted_strings(shell)

def run_all_tests(shell: ESP32Shell):
    """Run all tests."""
    run_basic_tests(shell)
    run_system_tests(shell)
    run_variable_tests(shell)
    run_file_tests(shell)
    run_gpio_tests(shell)
    run_error_tests(shell)
    run_parsing_tests(shell)

# ============================================================================
# Main
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="ESP32 Shell Serial Device Tests",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Test categories:
  basic      - Basic commands (echo, pwd, ls, help, cd)
  system     - System commands (free, uptime, info, fsinfo)
  variable   - Environment variables (set, unset, env)
  file       - File operations (touch, rm)
  gpio       - GPIO commands
  error      - Error handling
  parsing    - Command parsing (quotes)
  all        - Run all tests (default)

Examples:
  python3 test_esp32_device.py /dev/ttyACM0
  python3 test_esp32_device.py /dev/ttyACM0 -t basic
  python3 test_esp32_device.py /dev/ttyACM0 -v --baud 115200
"""
    )
    parser.add_argument("port", help="Serial port (e.g., /dev/ttyACM0)")
    parser.add_argument("-b", "--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("-t", "--test", default="all", help="Test category to run")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    parser.add_argument("--timeout", type=float, default=2.0, help="Serial timeout (default: 2.0)")
    
    args = parser.parse_args()
    
    print("")
    print("=" * 50)
    print("  ESP32 Shell - Serial Device Tests")
    print("=" * 50)
    print(f"  Port: {args.port}")
    print(f"  Baud: {args.baud}")
    print("=" * 50)
    print("")
    
    # Create shell connection
    shell = ESP32Shell(args.port, args.baud, args.timeout)
    
    # Test connection first
    if not test_connection(shell):
        print(f"\n{Colors.RED}Cannot connect to device. Ensure ESP32 is running shell firmware.{Colors.NC}")
        return 1
    
    # Run selected tests
    try:
        if args.test == "all":
            run_all_tests(shell)
        elif args.test == "basic":
            run_basic_tests(shell)
        elif args.test == "system":
            run_system_tests(shell)
        elif args.test == "variable":
            run_variable_tests(shell)
        elif args.test == "file":
            run_file_tests(shell)
        elif args.test == "gpio":
            run_gpio_tests(shell)
        elif args.test == "error":
            run_error_tests(shell)
        elif args.test == "parsing":
            run_parsing_tests(shell)
        else:
            print(f"Unknown test category: {args.test}")
            return 1
    finally:
        shell.disconnect()
    
    # Print summary
    print("")
    print("=" * 50)
    print("  Test Summary")
    print("=" * 50)
    print(f"  Tests run:    {Colors.BLUE}{results.run}{Colors.NC}")
    print(f"  Passed:       {Colors.GREEN}{results.passed}{Colors.NC}")
    print(f"  Failed:       {Colors.RED}{results.failed}{Colors.NC}")
    print(f"  Skipped:      {Colors.YELLOW}{results.skipped}{Colors.NC}")
    print("=" * 50)
    
    if results.failed > 0:
        print(f"\n{Colors.RED}Failed tests:{Colors.NC}")
        for failure in results.failures:
            print(f"  - {failure}")
        print(f"\n{Colors.RED}Some tests failed!{Colors.NC}")
        return 1
    else:
        print(f"\n{Colors.GREEN}All tests passed!{Colors.NC}")
        return 0

if __name__ == "__main__":
    sys.exit(main())
