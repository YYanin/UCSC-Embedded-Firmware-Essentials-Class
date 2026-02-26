# Top-level CMakeLists.txt for SD Card, USB Mass Storage, and LCD Clock project
# 
# This file configures the CMake build system for the ESP-IDF project.
# It sets the minimum required CMake version, includes the ESP-IDF build system,
# and defines the project name.
#
# Project: SD Card with FAT file system, USB Mass Storage device, and I2C LCD clock display
# Target: ESP32-S3

# Minimum CMake version required by ESP-IDF v5.x
cmake_minimum_required(VERSION 3.16)

# Include the ESP-IDF CMake build system
# This provides all the functions and macros needed to build ESP-IDF projects
# The $ENV{IDF_PATH} variable must be set (done by sourcing export.sh)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Define the project name
# This name is used for the output binary files (.elf, .bin)
project(sd_usb_lcd)
