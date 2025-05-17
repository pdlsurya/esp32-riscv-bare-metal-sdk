target_sources(esp32c6_sdk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/esp32c6/gpio_periph.c)

target_include_directories(esp32c6_sdk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/esp32c6/include)
