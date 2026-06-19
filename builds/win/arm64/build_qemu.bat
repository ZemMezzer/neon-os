cmake -B build -S . -G Ninja -DCMAKE_TOOLCHAIN_FILE=toolchains/aarch64-none-elf.cmake -DPLATFORM=qemu
cmake --build build