target_sources(esp32_rv_sdk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/sd_spi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sd_platform.c
)
target_include_directories(esp32_rv_sdk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
