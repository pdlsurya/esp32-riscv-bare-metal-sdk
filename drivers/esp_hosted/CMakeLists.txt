if(NOT TARGET_SOC STREQUAL "esp32p4")
    message(FATAL_ERROR "drivers/esp_hosted supports only TARGET_SOC=esp32p4")
endif()

target_sources(esp32_rv_sdk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/esp_hosted.c
    ${CMAKE_CURRENT_SOURCE_DIR}/esp_hosted_ctrl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/esp_hosted_sdio.c
)

target_include_directories(esp32_rv_sdk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
