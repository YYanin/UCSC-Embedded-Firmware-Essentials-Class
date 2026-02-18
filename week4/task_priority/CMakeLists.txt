# CMakeLists.txt for FreeRTOS 4-Task Priority Example
# This is the top-level CMake configuration file for the ESP-IDF project
# 
# Project: task_priority
# Description: Demonstrates FreeRTOS task scheduling with 4 tasks of different priorities
#              showing preemption and semaphore-based synchronization
#
# Tasks:
#   Task1 (P1) - Lowest priority, runs continuously
#   Task2 (P2) - Periodic every 500ms
#   Task3 (P3) - Periodic every 3000ms, runs for ~5 seconds
#   Task4 (P4) - Highest priority, semaphore-triggered by button press

# Minimum CMake version required by ESP-IDF
cmake_minimum_required(VERSION 3.16)

# Include the ESP-IDF CMake build system
# This provides all the build infrastructure for ESP32 projects
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Define the project name
# This name will be used for the output binary files
project(task_priority)
