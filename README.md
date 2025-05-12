# ESP32-C6 Bare-Metal SDK

A minimal bare-metal SDK for the ESP32-C6 RISC-V microcontroller, developed without relying on the ESP-IDF. This SDK provides low-level access to ESP32-C6 hardware, enabling firmware development with direct register manipulation and minimal abstraction.

## Features

- No ESP-IDF dependency
- Uses direct boot mode(No second stage bootloader required)
- Direct register-level programming
- Custom linker scripts and startup code
- Example applications (e.g., Blinky)
- Compatible with CMake and Ninja build systems

## Prerequisites

- [Espressif RISC-V Toolchain](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-guides/tools/idf-tools.html#riscv32-esp-elf)
- [esptool.py](https://github.com/espressif/esptool) for flashing
- Build system: CMake with Make or Ninja

## Getting Started

### Clone the Repository

```bash
git clone https://github.com/pdlsurya/esp32c6-bare-metal-sdk.git
cd esp32c6-bare-metal-sdk
```
### Build the Blinky example
```bash
cd examples/blinky
mkdir build && cd build
cmake ..
make
```

### Flash to ESP32-C6

```bash

esptool.py --chip esp32c6  write_flash 0x0 app.bin
```


