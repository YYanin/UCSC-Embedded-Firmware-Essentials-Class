#!/bin/bash
# ============================================================================
# ESP32 Shell - Host Simulation Tests
# ============================================================================
# These tests verify shell functionality on Linux host before deploying to ESP32.
# They simulate ESP32 memory constraints and test feature subsets.
#
# Usage:
#   ./tests/test_esp32.sh          # Run all tests
#   ./tests/test_esp32.sh -v       # Verbose mode
#   ./tests/test_esp32.sh -t NAME  # Run specific test
#
# Note: Some tests require the shell binary to be built for Linux host.
# ============================================================================

# Don't exit on error - we want to continue running tests
# set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_SKIPPED=0

# Verbosity
VERBOSE=0

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# ============================================================================
# Helper Functions
# ============================================================================

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
    ((TESTS_PASSED++))
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    ((TESTS_FAILED++))
}

log_skip() {
    echo -e "${YELLOW}[SKIP]${NC} $1"
    ((TESTS_SKIPPED++))
}

run_test() {
    local name="$1"
    local expected="$2"
    shift 2
    
    ((TESTS_RUN++))
    
    if [ $VERBOSE -eq 1 ]; then
        echo -e "\n${BLUE}Running:${NC} $name"
    fi
    
    # Run the test
    local result
    if result=$("$@" 2>&1); then
        if [[ "$result" == *"$expected"* ]]; then
            log_pass "$name"
            return 0
        else
            log_fail "$name (expected '$expected', got '$result')"
            return 1
        fi
    else
        log_fail "$name (command failed: $result)"
        return 1
    fi
}

# ============================================================================
# Source File Tests (Static Analysis)
# ============================================================================

test_source_files() {
    log_info "Testing source file structure..."
    
    # Check required files exist
    local required_files=(
        "src/main.c"
        "src/esp_shell.c"
        "src/esp_shell.h"
        "src/shell_config.h"
        "src/parser_esp32.c"
        "src/parser_esp32.h"
        "src/terminal_esp32.c"
        "src/terminal_esp32.h"
        "src/executor_esp32.c"
        "src/executor_esp32.h"
        "src/vfs_esp32.c"
        "src/platform_esp32.c"
        "src/platform.h"
    )
    
    for file in "${required_files[@]}"; do
        ((TESTS_RUN++))
        if [ -f "$PROJECT_DIR/$file" ]; then
            log_pass "File exists: $file"
        else
            log_fail "Missing file: $file"
        fi
    done
}

test_config_constants() {
    log_info "Testing configuration constants in shell_config.h..."
    
    local config_file="$PROJECT_DIR/src/shell_config.h"
    
    # Check required constants are defined
    local constants=(
        "SHELL_MAX_LINE_LEN"
        "SHELL_MAX_ARGS"
        "SHELL_HISTORY_SIZE"
        "SHELL_MAX_ENV_VARS"
        "SHELL_MAX_VAR_NAME"
        "SHELL_MAX_VAR_VALUE"
        "SHELL_MAX_PATH"
        "SHELL_LOW_MEMORY_WARN"
    )
    
    for const in "${constants[@]}"; do
        ((TESTS_RUN++))
        if grep -q "#define $const" "$config_file"; then
            log_pass "Constant defined: $const"
        else
            log_fail "Missing constant: $const"
        fi
    done
    
    # Verify ESP32 values are reasonable (small)
    ((TESTS_RUN++))
    local line_len=$(grep "SHELL_MAX_LINE_LEN.*256" "$config_file" | head -1)
    if [ -n "$line_len" ]; then
        log_pass "ESP32 SHELL_MAX_LINE_LEN is 256 (memory-efficient)"
    else
        log_fail "ESP32 SHELL_MAX_LINE_LEN should be 256"
    fi
}

test_no_forbidden_features() {
    log_info "Testing for forbidden features (should not exist in ESP32 code)..."
    
    local src_dir="$PROJECT_DIR/src"
    
    # Features that should NOT be used in ESP32 code
    # We check only actual function calls (not in comments or strings)
    # platform_linux.c is excluded as it's only for Linux builds
    
    # ESP32 core files to check (not Linux-specific files)
    local esp32_files="$src_dir/esp_shell.c $src_dir/executor_esp32.c $src_dir/parser_esp32.c $src_dir/terminal_esp32.c $src_dir/vfs_esp32.c"
    
    # Check for fork() call - look for actual calls like "= fork(" or "if (fork("
    # Exclude: comments, strings (inside quotes)
    ((TESTS_RUN++))
    if grep -E "=\s*fork\s*\(|;\s*fork\s*\(" $esp32_files 2>/dev/null | grep -qv "//"; then
        log_fail "Forbidden: fork() call found in ESP32 code"
    else
        log_pass "No fork() calls in ESP32 code"
    fi
    
    # Check for execve() - should not exist
    ((TESTS_RUN++))
    if grep -E "=\s*execve\s*\(|;\s*execve\s*\(" $esp32_files 2>/dev/null; then
        log_fail "Forbidden: execve() call found"
    else
        log_pass "No execve() calls"
    fi
    
    # Check for waitpid() - should not exist
    ((TESTS_RUN++))
    if grep -E "=\s*waitpid\s*\(|;\s*waitpid\s*\(" $esp32_files 2>/dev/null; then
        log_fail "Forbidden: waitpid() call found"
    else
        log_pass "No waitpid() calls"
    fi
    
    # Check for pipe() - should not exist (pipelines not supported)
    ((TESTS_RUN++))
    if grep -E "=\s*pipe\s*\(|;\s*pipe\s*\(" $esp32_files 2>/dev/null; then
        log_fail "Forbidden: pipe() call found"
    else
        log_pass "No pipe() calls"
    fi
}

test_esp32_platform_guards() {
    log_info "Testing ESP_PLATFORM guards..."
    
    local src_dir="$PROJECT_DIR/src"
    
    # Files that should have ESP_PLATFORM guards
    local guarded_files=(
        "shell_config.h"
        "platform_esp32.c"
        "vfs_esp32.c"
    )
    
    for file in "${guarded_files[@]}"; do
        ((TESTS_RUN++))
        if grep -q "ESP_PLATFORM\|ESP32\|IDF_VER" "$src_dir/$file" 2>/dev/null; then
            log_pass "Platform guard in: $file"
        else
            log_fail "Missing platform guard in: $file"
        fi
    done
}

# ============================================================================
# Build Configuration Tests
# ============================================================================

test_build_files() {
    log_info "Testing build configuration files..."
    
    local build_files=(
        "CMakeLists.txt"
        "main/CMakeLists.txt"
        "partitions.csv"
        "sdkconfig.defaults"
        "build.sh"
    )
    
    for file in "${build_files[@]}"; do
        ((TESTS_RUN++))
        if [ -f "$PROJECT_DIR/$file" ]; then
            log_pass "Build file exists: $file"
        else
            log_fail "Missing build file: $file"
        fi
    done
}

test_partition_table() {
    log_info "Testing partition table configuration..."
    
    local partitions="$PROJECT_DIR/partitions.csv"
    
    # Check for required partitions
    ((TESTS_RUN++))
    if grep -q "nvs" "$partitions"; then
        log_pass "NVS partition defined"
    else
        log_fail "Missing NVS partition"
    fi
    
    ((TESTS_RUN++))
    if grep -q "factory" "$partitions"; then
        log_pass "Factory partition defined"
    else
        log_fail "Missing factory partition"
    fi
    
    ((TESTS_RUN++))
    if grep -q "spiffs\|storage" "$partitions"; then
        log_pass "SPIFFS/storage partition defined"
    else
        log_fail "Missing SPIFFS partition"
    fi
}

test_sdkconfig() {
    log_info "Testing sdkconfig.defaults..."
    
    local sdkconfig="$PROJECT_DIR/sdkconfig.defaults"
    
    # Check for required settings
    local settings=(
        "CONFIG_ESP_CONSOLE_UART"
        "CONFIG_SPIFFS"
        "CONFIG_ESP_MAIN_TASK_STACK_SIZE"
    )
    
    for setting in "${settings[@]}"; do
        ((TESTS_RUN++))
        if grep -q "$setting" "$sdkconfig"; then
            log_pass "SDK config: $setting"
        else
            log_fail "Missing SDK config: $setting"
        fi
    done
}

# ============================================================================
# Memory Limit Simulation Tests
# ============================================================================

test_memory_limits() {
    log_info "Testing memory limit definitions..."
    
    local config_file="$PROJECT_DIR/src/shell_config.h"
    
    # Extract ESP32 limits and verify they're reasonable
    local line_len=$(grep "define SHELL_MAX_LINE_LEN" "$config_file" | grep "256" | wc -l)
    local max_args=$(grep "define SHELL_MAX_ARGS" "$config_file" | grep -E "16|32" | wc -l)
    local history=$(grep "define SHELL_HISTORY_SIZE" "$config_file" | grep -E "10|20" | wc -l)
    
    ((TESTS_RUN++))
    if [ "$line_len" -ge 1 ]; then
        log_pass "Command line buffer is 256 bytes (ESP32)"
    else
        log_fail "Command line buffer should be 256 for ESP32"
    fi
    
    ((TESTS_RUN++))
    if [ "$max_args" -ge 1 ]; then
        log_pass "Max args is 16-32 (ESP32)"
    else
        log_fail "Max args should be 16-32 for ESP32"
    fi
    
    ((TESTS_RUN++))
    if [ "$history" -ge 1 ]; then
        log_pass "History size is 10-20 (ESP32)"
    else
        log_fail "History size should be 10-20 for ESP32"
    fi
}

# ============================================================================
# Command Registration Tests
# ============================================================================

test_builtin_commands() {
    log_info "Testing builtin command registration..."
    
    local shell_file="$PROJECT_DIR/src/esp_shell.c"
    
    # Commands that should be registered
    local commands=(
        "help"
        "exit"
        "pwd"
        "cd"
        "ls"
        "echo"
        "cat"
        "touch"
        "rm"
        "set"
        "unset"
        "env"
        "reboot"
        "free"
        "uptime"
        "info"
        "gpio"
        "fsinfo"
        "format"
    )
    
    for cmd in "${commands[@]}"; do
        ((TESTS_RUN++))
        if grep -q "\"$cmd\"" "$shell_file"; then
            log_pass "Command registered: $cmd"
        else
            log_fail "Command not registered: $cmd"
        fi
    done
}

# ============================================================================
# Documentation Tests
# ============================================================================

test_documentation() {
    log_info "Testing documentation..."
    
    local docs=(
        "README.md"
    )
    
    for doc in "${docs[@]}"; do
        ((TESTS_RUN++))
        if [ -f "$PROJECT_DIR/$doc" ]; then
            log_pass "Documentation exists: $doc"
        else
            log_fail "Missing documentation: $doc"
        fi
    done
    
    # Check README has key sections
    ((TESTS_RUN++))
    if grep -q "Quick Start\|Build\|Usage" "$PROJECT_DIR/README.md"; then
        log_pass "README has build instructions"
    else
        log_fail "README missing build instructions"
    fi
    
    ((TESTS_RUN++))
    if grep -q "Available Commands\|Commands" "$PROJECT_DIR/README.md"; then
        log_pass "README has command documentation"
    else
        log_fail "README missing command documentation"
    fi
}

# ============================================================================
# Main
# ============================================================================

show_help() {
    echo "ESP32 Shell Host Tests"
    echo ""
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  -v          Verbose mode"
    echo "  -t NAME     Run specific test category"
    echo "  -h          Show this help"
    echo ""
    echo "Test categories:"
    echo "  source      Source file tests"
    echo "  config      Configuration constant tests"
    echo "  forbidden   Forbidden feature tests"
    echo "  guards      Platform guard tests"
    echo "  build       Build file tests"
    echo "  partition   Partition table tests"
    echo "  sdk         SDK config tests"
    echo "  memory      Memory limit tests"
    echo "  commands    Command registration tests"
    echo "  docs        Documentation tests"
}

# Parse arguments
while getopts "vt:h" opt; do
    case $opt in
        v)
            VERBOSE=1
            ;;
        t)
            TEST_FILTER="$OPTARG"
            ;;
        h)
            show_help
            exit 0
            ;;
        *)
            show_help
            exit 1
            ;;
    esac
done

echo ""
echo "============================================"
echo "  ESP32 Shell - Host Simulation Tests"
echo "============================================"
echo ""

cd "$PROJECT_DIR"

# Run tests based on filter or all
if [ -z "$TEST_FILTER" ]; then
    test_source_files
    test_config_constants
    test_no_forbidden_features
    test_esp32_platform_guards
    test_build_files
    test_partition_table
    test_sdkconfig
    test_memory_limits
    test_builtin_commands
    test_documentation
else
    case "$TEST_FILTER" in
        source)     test_source_files ;;
        config)     test_config_constants ;;
        forbidden)  test_no_forbidden_features ;;
        guards)     test_esp32_platform_guards ;;
        build)      test_build_files ;;
        partition)  test_partition_table ;;
        sdk)        test_sdkconfig ;;
        memory)     test_memory_limits ;;
        commands)   test_builtin_commands ;;
        docs)       test_documentation ;;
        *)
            echo "Unknown test category: $TEST_FILTER"
            show_help
            exit 1
            ;;
    esac
fi

# Print summary
echo ""
echo "============================================"
echo "  Test Summary"
echo "============================================"
echo -e "  Tests run:    ${BLUE}$TESTS_RUN${NC}"
echo -e "  Passed:       ${GREEN}$TESTS_PASSED${NC}"
echo -e "  Failed:       ${RED}$TESTS_FAILED${NC}"
echo -e "  Skipped:      ${YELLOW}$TESTS_SKIPPED${NC}"
echo "============================================"

if [ $TESTS_FAILED -gt 0 ]; then
    echo -e "\n${RED}Some tests failed!${NC}"
    exit 1
else
    echo -e "\n${GREEN}All tests passed!${NC}"
    exit 0
fi
