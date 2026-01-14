if(NOT DEFINED TARGET_SOC)
    message(FATAL_ERROR "TARGET SOC not defined")
endif()

if(TARGET_SOC STREQUAL "esp32p4")
    target_sources(esp32_rv_sdk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdmmc.c)
    target_include_directories(esp32_rv_sdk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
endif()
