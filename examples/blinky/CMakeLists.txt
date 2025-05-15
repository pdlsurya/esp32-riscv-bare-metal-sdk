cmake_minimum_required(VERSION 3.13)

set(SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../..) #Adjust this accordingly

set(CMAKE_TOOLCHAIN_FILE ${SDK_PATH}/cmake/toolchain-esp32c6.cmake)

include(${SDK_PATH}/cmake/sdk-utils.cmake)

# Project name and target
project(esp32c6_blinky LANGUAGES C CXX ASM)

# Add the executable
add_executable(app.elf source/main.c)

sdk_config(app.elf)

add_extra_outputs(app.elf)
