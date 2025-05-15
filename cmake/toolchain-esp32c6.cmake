set(CMAKE_SYSTEM_NAME Generic)

#specity the C and C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

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

