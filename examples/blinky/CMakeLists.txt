cmake_minimum_required(VERSION 3.13)

set(SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../..) #Adjust this accordingly

set(CMAKE_TOOLCHAIN_FILE ${SDK_PATH}/toolchain/toolchain-esp32c6.cmake)

# Project name and target
project(esp32c6_blinky LANGUAGES C CXX ASM)

# Add the SDK
add_subdirectory(${SDK_PATH} ${CMAKE_CURRENT_BINARY_DIR}/esp32c6_sdk)

# Add the executable
add_executable(app.elf source/main.c)

# Link the SDK
target_link_libraries(app.elf PRIVATE esp32c6_sdk)

add_extra_outputs(app.elf)
