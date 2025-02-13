[中文](./README_CN.md)

# felix86

felix86 is a Linux userspace emulator that allows you to run x86-64 Linux programs on RISC-V processors. It is in the early stages of development.

Compilation and usage instructions can be found [here](./docs/how-to-use.md).

Want to contribute but don't know where to start? [Check this out](./docs/contributing.md).

## Features
- Just-in-Time (JIT) recompiler
- Uses the RISC-V Vector Extension for SSE instructions
- Utilizes the B extension, if available, for bit manipulation instructions like `bsr`
- Supports a variety of optional extensions, such as XThead custom extensions

## Compatibility
felix86 is in the very early stages of development and does not support AArch64.

If you need a more mature x86-64 userspace emulator, consider using one of these:

- [FEX](https://github.com/FEX-Emu/FEX) for x86 & x86-64 on AArch64
- [box64](https://github.com/ptitSeb/box64) for x86 & x86-64 on AArch64 and RISC-V
- [qemu-user](https://www.qemu.org/docs/master/user/main.html) for a wide range of architectures

## Dependencies
felix86 relies on several great projects:

- [FEX](https://github.com/FEX-Emu/FEX)'s comprehensive unit test suite and rootfs generation
- [Biscuit](https://github.com/lioncash/biscuit) for RISC-V code emission
- [Zydis](https://github.com/zyantific/zydis) for decoding and disassembly
- [Catch2](https://github.com/catchorg/Catch2) for unit testing
- [fmt](https://github.com/fmtlib/fmt) for string formatting
- [nlohmann/json](https://github.com/nlohmann/json) for JSON parsing

## Why?
felix86 was started for several reasons, including:

- Gaining a deeper understanding of x86-64, RISC-V, Linux, and high-level emulation
- Exploring optimizing compilers and JITs (SSA, register allocation, optimization passes, etc.)
- To learn more about low level details, such as signals, syscalls, program loading
- Taking on a fun and challenging project

## Also Check Out

- [Panda3DS](https://github.com/wheremyfoodat/Panda3DS), a 3DS emulator for Windows, macOS, Linux, and Android
- [shadPS4](https://github.com/shadps4-emu/shadPS4), one of the leading PS4 emulators

