cmake_minimum_required(VERSION 3.13)

# Project name 
project(esp32c6_bare_metal_sdk LANGUAGES C CXX ASM)

# Define esp32c6 sdk as static library
add_library(esp32c6_sdk STATIC ${CMAKE_CURRENT_SOURCE_DIR}/components/startup/startup.c)

target_include_directories(esp32c6_sdk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk-config)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/components/delay)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/components/esp_common)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/components/esp_rom)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/components/hal)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/components/i2c_driver)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/components/interrupts)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/components/mtimer)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/components/riscv)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/components/soc)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/components/spi_driver)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/components/usb_serial)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libraries/ws2812)

target_link_options(esp32c6_sdk PUBLIC
    "SHELL:-T${CMAKE_CURRENT_SOURCE_DIR}/ld/esp32c6.ld"
    "SHELL:-T${CMAKE_CURRENT_SOURCE_DIR}/ld/esp32c6.peripherals.ld"
    "SHELL:-T${CMAKE_CURRENT_SOURCE_DIR}/components/esp_rom/esp32c6/ld/esp32c6.rom.ld"
    "SHELL:-T${CMAKE_CURRENT_SOURCE_DIR}/components/esp_rom/esp32c6/ld/esp32c6.rom.api.ld"
)


