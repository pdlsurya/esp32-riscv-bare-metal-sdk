target_sources(esp32_rv_sdk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/es8311.c
)
target_include_directories(esp32_rv_sdk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
