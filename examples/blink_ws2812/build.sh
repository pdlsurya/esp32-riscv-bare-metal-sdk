cmake -Bbuild -GNinja -DTARGET_SOC=esp32p4
cmake --build build && esptool.py write_flash 0x0 build/app.bin
