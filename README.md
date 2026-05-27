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
- Docker (optional, for containerized builds)

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

### Build SDK examples on the host

```bash
cd examples/blink_ws2812
./build.sh

cd ../esp_hosted
./build.sh
```

### Flash an SDK example

```bash
cd examples/blink_ws2812
./flash.sh
```

If you want the previous one-command flow, use `./build_flash.sh` in either example directory.

### Build SDK examples with Docker

Container builds are optional. The regular host build flow still works, and Docker is intended as a reproducible alternative for onboarding and CI.

```bash
docker build -t esp32-rv-sdk -f docker/Dockerfile .

./scripts/docker-build.sh examples/blink_ws2812
./scripts/docker-build.sh examples/esp_hosted
```

Open an interactive shell in the build container:

```bash
./scripts/docker-shell.sh
```

Notes:

- Docker support is aimed at building artifacts, not replacing host flashing.
- `./scripts/docker-build.sh` reuses the existing Docker image by default. Set `REBUILD_IMAGE=1` when you want to rebuild it after changing the Dockerfile or container dependencies.
- `./scripts/docker-build.sh ...` binds the SDK source tree from the host and builds directly into the requested example `build/` directory.
- Flashing from the host is usually the simplest path, especially on macOS where USB passthrough into Docker can be unreliable.
- The Docker image currently defaults to the Espressif `riscv32-esp-elf` `esp-14.2.0_20251107` Linux toolchain and can be overridden with Docker build arguments if you want to pin a different release.

## Building External Projects

External projects should point CMake at the SDK before calling `project(...)`, just like the SDK examples do.

Minimal `CMakeLists.txt` pattern:

```cmake
cmake_minimum_required(VERSION 3.13)

set(SDK_PATH "$ENV{SDK_PATH}" CACHE PATH "Path to esp32-rv-bare-metal-sdk")

if(SDK_PATH STREQUAL "")
    set(SDK_PATH "/absolute/path/to/esp32-rv-bare-metal-sdk")
endif()

get_filename_component(SDK_PATH "${SDK_PATH}" ABSOLUTE)

set(TARGET_SOC "esp32c6" CACHE STRING "Target SOC")
set(CMAKE_TOOLCHAIN_FILE "${SDK_PATH}/cmake/toolchain-${TARGET_SOC}.cmake" CACHE FILEPATH "SDK toolchain file")

project(my_app LANGUAGES C CXX ASM)

include(${SDK_PATH}/cmake/sdk-utils.cmake)

add_executable(app.elf source/main.c)

sdk_config(app.elf)
add_extra_outputs(app.elf)
```

Build an external project on the host:

```bash
cmake -S . -B build -GNinja -DSDK_PATH=/absolute/path/to/esp32-rv-bare-metal-sdk
cmake --build build
```

Build an external project with Docker:

```bash
docker run --rm -it \
  -v /absolute/path/to/my-project:/work/app \
  -v /absolute/path/to/esp32-rv-bare-metal-sdk:/work/sdk \
  -w /work/app \
  esp32-rv-sdk \
  bash -lc 'cmake -S . -B build -GNinja -DSDK_PATH=/work/sdk && cmake --build build'
```

If your project depends on other local components such as an RTOS or board library, mount them as additional volumes and pass matching `-D..._PATH=/work/...` arguments during CMake configure. For example:

```bash
docker run --rm -it \
  -v /absolute/path/to/my-project:/work/app \
  -v /absolute/path/to/esp32-rv-bare-metal-sdk:/work/sdk \
  -v /absolute/path/to/sanoRTOS:/work/sanoRTOS \
  -w /work/app \
  esp32-rv-sdk \
  bash -lc 'cmake -S . -B build -GNinja -DSDK_PATH=/work/sdk -DSANORTOS_PATH=/work/sanoRTOS && cmake --build build'
```
