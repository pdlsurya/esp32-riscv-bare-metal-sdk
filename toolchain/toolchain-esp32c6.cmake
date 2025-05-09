set(CMAKE_SYSTEM_NAME Generic)

# Specify the cross-compiler
set(CMAKE_C_COMPILER riscv32-esp-elf-gcc)
set(CMAKE_CXX_COMPILER riscv32-esp-elf-g++)
set(CMAKE_ASM_COMPILER riscv32-esp-elf-gcc)
set(CMAKE_CPP riscv32-esp-elf-cpp)
# Specify the linker
set(CMAKE_LINKER riscv32-esp-elf-ld)

# Define flags
set(CMAKE_C_FLAGS "-march=rv32imac_zicsr_zifencei -mabi=ilp32 -nostartfiles  -Wno-unused-variable -Wno-unused-parameter -fdata-sections -ffunction-sections -fno-strict-aliasing  -fshort-enums -fno-builtin ")
set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} -x assembler-with-cpp")

add_link_options("SHELL:-Wl,--gc-sections --specs=nano.specs --specs=nosys.specs")

# Optional: Set objcopy and size utilities
set(CMAKE_OBJCOPY riscv32-esp-elf-objcopy)
set(CMAKE_OBJDUMP riscv32-esp-elf-objdump)
set(CMAKE_SIZE riscv32-esp-elf-size)

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
