# Function to add extra post-build outputs
function(add_extra_outputs target)
    # Strip the .elf suffix if it exists
    string(REGEX REPLACE "\\.elf$" "" base_name "${target}")

    set(elf_file ${CMAKE_BINARY_DIR}/${target})
    set(bin_file ${CMAKE_BINARY_DIR}/${base_name}.bin)
    set(asm_file ${CMAKE_BINARY_DIR}/${base_name}.asm)

    set(CMD_GENERATE_BIN ${CMAKE_OBJCOPY} -O binary ${elf_file} ${bin_file})

    # Generate .bin from .elf file
    add_custom_command(
        TARGET ${target} POST_BUILD
        COMMAND ${CMD_GENERATE_BIN}
        COMMENT "Converting ${target} to ${base_name}.bin"
    )

    # Generate disassembly file
    add_custom_command(
        TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_OBJDUMP} -d ${elf_file} > ${asm_file}
        COMMENT "Generating ${base_name}.asm from ${target}"
    )

    # Get size
    add_custom_command(
        TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_SIZE} ${elf_file}
        COMMENT "Generating size for ${target}"
    )
endfunction()

function(sdk_config target)
    # Add the SDK
    add_subdirectory(${SDK_PATH} ${CMAKE_CURRENT_BINARY_DIR}/sdk_build)
    # Link the SDK
    target_link_libraries(${target} PRIVATE esp32_rv_sdk)

    # The SDK currently exposes many sources as PUBLIC on esp32_rv_sdk, so some
    # consumers compile SDK sources directly as part of their own target. Recent
    # esp-nimble revisions use esp_err_t from transport/host sources without
    # including esp_err.h themselves, so inject it here for C compilation on
    # consumer targets as well.
    target_compile_options(${target} PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-include>
        $<$<COMPILE_LANGUAGE:C>:esp_err.h>
    )

endfunction(sdk_config)
