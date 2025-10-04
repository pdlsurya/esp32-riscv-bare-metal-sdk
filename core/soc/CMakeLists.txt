file(GLOB_RECURSE SOC_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/esp32c6/*.c)

target_sources(esp32c6_sdk PUBLIC ${SOC_SOURCES})

target_include_directories(esp32c6_sdk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/esp32c6/include)
