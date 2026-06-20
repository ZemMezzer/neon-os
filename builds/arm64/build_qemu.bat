cmake -B build -S . -G Ninja -DCMAKE_TOOLCHAIN_FILE=toolchains/aarch64-none-elf.cmake -DPLATFORM=qemu -DLUA_ENABLED=ON
cmake --build build