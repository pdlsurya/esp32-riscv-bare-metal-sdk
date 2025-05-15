cmake_minimum_required(VERSION 3.13)

# Project name 
project(esp32c6_bare_metal_sdk LANGUAGES C CXX ASM)

# Define esp32c6 sdk as static library
add_library(esp32c6_sdk STATIC ${CMAKE_CURRENT_SOURCE_DIR}/core/startup/startup.c)

target_include_directories(esp32c6_sdk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk-config)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/core/delay)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/core/esp_common)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/core/esp_rom)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/core/hal)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/core/interrupts)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/core/mtimer)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/core/riscv)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/core/soc)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/drivers/gpio_drv)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/drivers/i2c_drv)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/drivers/spi_drv)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/drivers/usb_serial)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libraries/ws2812)


target_link_options(esp32c6_sdk PUBLIC
    "SHELL:-T${CMAKE_CURRENT_SOURCE_DIR}/ld/esp32c6.ld"
    "SHELL:-T${CMAKE_CURRENT_SOURCE_DIR}/core/soc/esp32c6/ld/esp32c6.peripherals.ld"
    "SHELL:-T${CMAKE_CURRENT_SOURCE_DIR}/core/esp_rom/esp32c6/ld/esp32c6.rom.ld"
    "SHELL:-T${CMAKE_CURRENT_SOURCE_DIR}/core/esp_rom/esp32c6/ld/esp32c6.rom.api.ld"
)
