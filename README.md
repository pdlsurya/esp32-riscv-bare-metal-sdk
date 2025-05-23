# ESP32-C6 Bare-Metal SDK

A minimal **bare-metal SDK for Espressif ESP32 RISC-V microcontrollers**, developed without relying on the ESP-IDF.  
This SDK provides low-level access to ESP32 hardware, enabling firmware development using **direct register-level programming, custom startup code, and linker scripts** with minimal abstraction.

Currently supported SoCs:

- **ESP32-C6**
- **ESP32-P4**

---

## Features

- No ESP-IDF dependency
- Uses direct boot mode(No second stage bootloader required)
- Direct register-level programming
- Custom linker scripts and startup code
- Example applications (e.g., Blinky)
- Compatible with CMake and Ninja build systems

---

## Supported Targets

| SoC      | Architecture | Notes                                  |
| -------- | ------------ | -------------------------------------- |
| ESP32-C6 | RISC-V       | WiFi + BLE MCU                         |
| ESP32-P4 | RISC-V       | High-performance application processor |

Targets are selected during **CMake configuration**.

---

## Prerequisites

- [Espressif RISC-V Toolchain](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-guides/tools/idf-tools.html#riscv32-esp-elf)
- [esptool.py](https://github.com/espressif/esptool) for flashing
- Build system: CMake with Make or Ninja

## Note

Some low-level components (including portions of HAL headers, register definitions, and peripheral structure declarations) are adapted from Espressif's ESP-IDF SDK. These were used as reference to avoid reverse engineering hardware register layouts and peripheral interfaces, while keeping this project independent of the ESP-IDF framework.

## Getting Started

### Clone the Repository

```bash
git clone https://github.com/pdlsurya/esp32-riscv-bare-metal-sdk.git
cd esp32-riscv-bare-metal-sdk
```

### Build the Blinky example

```bash
cd examples/blinky
mkdir build && cd build
cmake ..
make
```

### Flash to ESP32

```bash

esptool.py write_flash 0x0 app.bin
```
