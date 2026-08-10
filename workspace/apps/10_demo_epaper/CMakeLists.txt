cmake_minimum_required(VERSION 3.20.0)

list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# https://docs.zephyrproject.org/latest/build/dts/howtos.html
# set(DTC_OVERLAY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/boards/esp32s3_devkitc.overlay ${CMAKE_CURRENT_SOURCE_DIR}/boards/usb_print.overlay")

set(DTC_OVERLAY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/boards/esp32s3_devkitc-custom-driver.overlay ${CMAKE_CURRENT_SOURCE_DIR}/boards/usb_print.overlay")
# set(DTC_OVERLAY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/boards/esp32s3_devkitc-8179.overlay ${CMAKE_CURRENT_SOURCE_DIR}/boards/usb_print.overlay")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(demo_epaper)

# CMake Warning at /opt/toolchains/zephyr/CMakeLists.txt:953 (message):
    # No SOURCES given to Zephyr library: drivers__display
    # Excluding target from build.

# using custom driver
target_sources(app PRIVATE src/main.c drivers/display/uc8253.c)

# repurposing drivers...
# target_sources(app PRIVATE src/main.c)
