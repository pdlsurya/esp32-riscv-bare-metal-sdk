
if(NOT DEFINED TARGET_SOC)
    message(FATAL_ERROR "TARGET SOC not defined")
endif()

file(GLOB_RECURSE SOC_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/${TARGET_SOC}/*.c)
target_sources(esp32_rv_sdk PUBLIC ${SOC_SOURCES})
target_include_directories(esp32_rv_sdk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/${TARGET_SOC}/include
)

if(TARGET_SOC STREQUAL "esp32c6")

    target_include_directories(esp32_rv_sdk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/esp32c6/register)

elseif(TARGET_SOC STREQUAL "esp32p4")

    target_include_directories(esp32_rv_sdk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/esp32p4/register/hw_ver1)

else()
    message(FATAL_ERROR "Unsupported TARGET_SOC: ${TARGET_SOC}")
endif()




