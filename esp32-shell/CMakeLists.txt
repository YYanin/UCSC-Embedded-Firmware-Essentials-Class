# ESP32 Shell - Main CMakeLists.txt
# ============================================================================
# Native ESP-IDF project configuration for ESP32-S3 shell.
# Build with: idf.py build
# Flash with: idf.py flash
# Monitor with: idf.py monitor
# ============================================================================

cmake_minimum_required(VERSION 3.16)

# Include ESP-IDF build system
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Project name and version
project(esp32_shell VERSION 1.0.0)
