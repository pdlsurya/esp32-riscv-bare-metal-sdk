cmake_minimum_required(VERSION 3.13)

# Project name 
project(esp32_rv_bare_metal_sdk LANGUAGES C CXX ASM)

# Define esp32p4 sdk as static library
add_library(esp32_rv_sdk STATIC ${CMAKE_CURRENT_SOURCE_DIR}/core/startup/${TARGET_SOC}/startup.c)

target_include_directories(esp32_rv_sdk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk-config ${CMAKE_CURRENT_SOURCE_DIR}/core/startup/${TARGET_SOC})

string(TOUPPER ${TARGET_SOC} TARGET_SOC_UPPER)

target_compile_definitions(esp32_rv_sdk PUBLIC TARGET_SOC_${TARGET_SOC_UPPER})

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/core/delay)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/core/esp_common)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/core/esp_hw_support)
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


target_link_options(esp32_rv_sdk PUBLIC
    "SHELL:-T${CMAKE_CURRENT_SOURCE_DIR}/ld/${TARGET_SOC}/${TARGET_SOC}.ld"
    "SHELL:-T${CMAKE_CURRENT_SOURCE_DIR}/core/soc/${TARGET_SOC}/ld/${TARGET_SOC}.peripherals.ld"
    "SHELL:-T${CMAKE_CURRENT_SOURCE_DIR}/core/esp_rom/${TARGET_SOC}/ld/${TARGET_SOC}.rom.ld"
    "SHELL:-T${CMAKE_CURRENT_SOURCE_DIR}/core/esp_rom/${TARGET_SOC}/ld/${TARGET_SOC}.rom.api.ld"
)
