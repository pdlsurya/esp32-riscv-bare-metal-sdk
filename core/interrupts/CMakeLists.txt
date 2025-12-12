if(NOT DEFINED TARGET_SOC)
    message(FATAL_ERROR "TARGET SOC not defined")
endif()

target_sources(esp32_rv_sdk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/${TARGET_SOC}/interrupts.c)

target_include_directories(esp32_rv_sdk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/${TARGET_SOC})

if(TARGET_SOC STREQUAL "esp32c6")

    target_sources(esp32_rv_sdk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/${TARGET_SOC}/interrupt_plic.c)

elseif(TARGET_SOC STREQUAL "esp32p4")

    target_sources(esp32_rv_sdk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/${TARGET_SOC}/interrupt_clic.c)

else()
    message(FATAL_ERROR "Unsupported TARGET_SOC: ${TARGET_SOC}")
endif()

