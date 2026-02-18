# CMakeLists.txt for the main component
# This file registers the main application source files with the ESP-IDF build system
#
# The idf_component_register() function tells ESP-IDF:
#   SRCS - Which source files to compile (main.c contains our 4-task implementation)
#   INCLUDE_DIRS - Directories to search for header files ("." means current directory)

idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
)
