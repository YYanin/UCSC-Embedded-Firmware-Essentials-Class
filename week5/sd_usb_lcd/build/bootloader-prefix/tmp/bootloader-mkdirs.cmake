# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/nordiffico/Documents/esp/esp-idf/components/bootloader/subproject"
  "/home/nordiffico/Documents/esp/projects/week5/sd_usb_lcd/build/bootloader"
  "/home/nordiffico/Documents/esp/projects/week5/sd_usb_lcd/build/bootloader-prefix"
  "/home/nordiffico/Documents/esp/projects/week5/sd_usb_lcd/build/bootloader-prefix/tmp"
  "/home/nordiffico/Documents/esp/projects/week5/sd_usb_lcd/build/bootloader-prefix/src/bootloader-stamp"
  "/home/nordiffico/Documents/esp/projects/week5/sd_usb_lcd/build/bootloader-prefix/src"
  "/home/nordiffico/Documents/esp/projects/week5/sd_usb_lcd/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/nordiffico/Documents/esp/projects/week5/sd_usb_lcd/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/nordiffico/Documents/esp/projects/week5/sd_usb_lcd/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
