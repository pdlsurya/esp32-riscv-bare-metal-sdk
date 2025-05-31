if(NOT DEFINED TARGET_SOC)
    message(FATAL_ERROR "TARGET SOC not defined")
endif()

target_include_directories(esp32_rv_sdk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/${TARGET_SOC}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/${TARGET_SOC}/include/${TARGET_SOC}
    ${CMAKE_CURRENT_SOURCE_DIR}/${TARGET_SOC}
)

