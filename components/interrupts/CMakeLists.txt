target_sources(esp32c6_sdk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/interrupts.c
    ${CMAKE_CURRENT_SOURCE_DIR}/interrupt_plic.c)

target_include_directories(esp32c6_sdk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
