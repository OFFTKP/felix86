# felix86
An x86-64 userspace emulator for RISC-V Linux

## Similar projects
felix86 is very early in development, and will not support AArch64.
If you want a more mature x86-64 userspace emulator, use one of these:

- [FEX](https://github.com/FEX-Emu/FEX)
- [box64](https://github.com/ptitSeb/box64)
- [qemu-user](https://www.qemu.org/docs/master/user/main.html)

## Dependencies
felix86 depends on a bunch of great projects:
- [FEX](https://github.com/FEX-Emu/FEX)'s incredible unit test suite & rootfs generation
- [Biscuit](https://github.com/lioncash/biscuit) for RISC-V emitting
- [Zydis](https://github.com/zyantific/zydis) for AOT decoding/disassembly
- [plog](https://github.com/SergiusTheBest/plog) for logging
- [Catch2](https://github.com/catchorg/Catch2) for unit testing
- [fmt](https://github.com/fmtlib/fmt) for string formatting
- [robin-map](https://github.com/Tessil/robin-map) for a fast hashmap implementation
- [nlohmann/json](https://github.com/nlohmann/json) for json parsing

## Why?
felix86 was started for several reasons, but to name a few:
- Learning more about x86-64, RISC-V, Linux, high-level emulation
- Learning about optimizing compilers, method JITs (SSA, RA, optimization passes, ...)
- Learning more about different memory models and low level details
- Fun challenge

## Also check out
- [Panda3DS](https://github.com/wheremyfoodat/Panda3DS), a 3DS emulator for Windows, MacOS, Linux and Android
- [shadPS4](https://github.com/shadps4-emu/shadPS4), the current greatest PS4 emulator
