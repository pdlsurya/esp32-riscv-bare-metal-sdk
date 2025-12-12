target_include_directories(esp32_rv_sdk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_sources(esp32_rv_sdk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ws2812.c)
