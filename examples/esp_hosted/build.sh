rm -rf build
cmake  -Bbuild -GNinja 
cmake --build build && esptool.py  write_flash 0x0 build/app.bin