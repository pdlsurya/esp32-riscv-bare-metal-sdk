# ESP32 RISC-V Bare-Metal SDK

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
- Example applications (e.g., esp_hosted, blink_ws2812)
- Compatible with CMake and Ninja build systems

---

## Supported SDK Features

- Core platform: custom startup, interrupt setup, timers, SoC/peripheral register access
- GPIO driver: pin direction, read/write, pull-up and pull-down configuration
- I2C driver: master-mode transactions for peripheral bring-up
- SPI driver: low-level SPI transfer support
- I2S driver: TX path with DMA streaming support for continuous PCM playback
- ES8311 codec driver: register-level codec initialization and playback control
- SD/MMC and SD SPI drivers: card access paths for block reads/writes
- FAT32 library (`sdFat32`): basic file operations on SD storage
- WS2812 library: LED strip output using RMT/SPI backend
- USB serial driver: runtime logging/console output over USB serial
- ESP-Hosted host driver (ESP32-P4): SDIO transport + control/data channels for co-processor integration
- BLE host stack on ESP32-P4: NimBLE host integrated over ESP-Hosted HCI transport
- lwIP hosted networking on ESP32-P4: hosted STA data path + DHCP flow
- Bare-metal runtime model: polling-based main loop, no RTOS dependency

### Target-Specific Notes

- `esp_hosted` BLE + Wi-Fi/lwIP integration is supported on `ESP32-P4` target only.
- Generic peripheral drivers/libraries are available across supported SoCs where hardware capability matches.

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
git clone --recurse-submodules https://github.com/pdlsurya/esp32-riscv-bare-metal-sdk.git
cd esp32-riscv-bare-metal-sdk
```

If you already cloned the repository without submodules, run:

```bash
git submodule update --init --recursive
```

### Build the ESP Hosted example

```bash
cd examples/esp_hosted
mkdir build && cd build
cmake ..
make
```

### Build the Blink + WS2812 example

```bash
cd examples/blink_ws2812
mkdir build && cd build
cmake ..
make
```

### Flash to ESP32

```bash

esptool.py write_flash 0x0 app.bin
```
