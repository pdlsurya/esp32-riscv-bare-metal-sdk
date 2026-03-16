cmake_minimum_required(VERSION 3.13)

set(SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../..)

set(TARGET_SOC "esp32p4" CACHE STRING "Target SOC")

set(CMAKE_TOOLCHAIN_FILE ${SDK_PATH}/cmake/toolchain-${TARGET_SOC}.cmake)

include(${SDK_PATH}/cmake/sdk-utils.cmake)

project(blink_ws2812 LANGUAGES C CXX ASM)

add_executable(app.elf source/main.c)

sdk_config(app.elf)

add_extra_outputs(app.elf)
