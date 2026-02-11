# =============================================================================
# 7-Segment Display Counter Project - Top Level CMakeLists.txt
# =============================================================================
# This is the main CMake configuration file for the 7-segment display counter
# project. It sets the minimum CMake version required, includes the ESP-IDF 
# build system, and defines the project name.
#
# Author: UCSC Embedded Firmware Essentials Class
# Date: January 2026
# Target: ESP32-S3
# =============================================================================

# Minimum CMake version required by ESP-IDF
cmake_minimum_required(VERSION 3.16)

# Include the ESP-IDF CMake build system
# This provides all the ESP-IDF specific build functions and macros
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Define the project name
# This name will be used for the output binary files
project(seven_segment_counter)
