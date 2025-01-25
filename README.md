# felix86
An x86-64 userspace emulator for RISC-V Linux. Early in development.

Compilation and usage instructions can be found [here](./docs/how-to-use.md).

Want to contribute but don't know what to do? [Check this out](./docs/internals/contributing.md).

## Features
- Just-in-Time recompiler
- Uses RISC-V Vector Extension for SSE instructions
- Utilizes B extension if available for bit manipulation instructions like `bsr`
- Support for a variety of optional extensions such as XThead custom extensions

## Compatibility
felix86 is very early in development, and will not support AArch64.

At the moment, felix86 can run some console-based applications such as `python3` or `lua`.

If you want a more mature x86-64 userspace emulator, use one of these:

- [FEX](https://github.com/FEX-Emu/FEX), for x86 & x86-64 on AArch64
- [box64](https://github.com/ptitSeb/box64), for x86 & x86-64 on AArch64 and RISC-V
- [qemu-user](https://www.qemu.org/docs/master/user/main.html), for basically everything on everything

## Dependencies
felix86 depends on a bunch of great projects:
- [FEX](https://github.com/FEX-Emu/FEX)'s incredible unit test suite & rootfs generation
- [Biscuit](https://github.com/lioncash/biscuit) for RISC-V emitting
- [Zydis](https://github.com/zyantific/zydis) for decoding/disassembly
- [Catch2](https://github.com/catchorg/Catch2) for unit testing
- [fmt](https://github.com/fmtlib/fmt) for string formatting
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
