#!/bin/bash

rm -rf build
cmake  -Bbuild -GNinja
cmake --build build && esptool.py --chip esp32c6 write_flash  0x0 build/app.bin