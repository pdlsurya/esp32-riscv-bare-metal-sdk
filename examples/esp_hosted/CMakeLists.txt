cmake_minimum_required(VERSION 3.13)

set(SDK_PATH "$ENV{SDK_PATH}" CACHE PATH "Path to esp32-rv-bare-metal-sdk")

if(SDK_PATH STREQUAL "")
    set(SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../..")
endif()

get_filename_component(SDK_PATH "${SDK_PATH}" ABSOLUTE)

if(NOT EXISTS "${SDK_PATH}/cmake/sdk-utils.cmake")
    message(FATAL_ERROR "SDK_PATH is invalid: ${SDK_PATH}")
endif()

set(TARGET_SOC "esp32p4" CACHE STRING "Target SOC")

set(CMAKE_TOOLCHAIN_FILE "${SDK_PATH}/cmake/toolchain-${TARGET_SOC}.cmake" CACHE FILEPATH "SDK toolchain file")

project(esp_hosted LANGUAGES C CXX ASM)

include(${SDK_PATH}/cmake/sdk-utils.cmake)

if(NOT TARGET_SOC STREQUAL "esp32p4")
    message(FATAL_ERROR "examples/esp_hosted supports only TARGET_SOC=esp32p4")
endif()

# Add the executable
add_executable(app.elf source/main.c)

sdk_config(app.elf)

add_extra_outputs(app.elf)
