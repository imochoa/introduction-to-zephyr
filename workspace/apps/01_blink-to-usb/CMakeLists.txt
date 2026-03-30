# Minimum CMake version
cmake_minimum_required(VERSION 3.22.0)

# https://docs.zephyrproject.org/latest/build/dts/howtos.html
set(DTC_OVERLAY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/boards/esp32s3_devkitc.overlay ${CMAKE_CURRENT_SOURCE_DIR}/boards/usb_print.overlay")

# Locate the Zephyr RTOS source
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# Name the project
project(blink)

# Locate the source code for the application
target_sources(app PRIVATE src/main.c)