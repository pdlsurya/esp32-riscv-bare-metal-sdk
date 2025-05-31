target_include_directories(esp32_rv_sdk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_sources(esp32_rv_sdk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdFat32.c)

if(DEFINED SDFAT32_SD_BACKEND)
    target_compile_definitions(esp32_rv_sdk PUBLIC SDFAT32_SD_BACKEND=${SDFAT32_SD_BACKEND})
endif()
