[中文](./README_CN.md) | [Website](https://felix86.com) | **[Discord](https://discord.gg/TgBxgFwByU)**

# felix86

felix86 is a Linux userspace emulator that allows you to run x86 and x86-64 programs on RISC-V processors

## Getting started

### Ubuntu/Debian/Bianbu and maybe others
Run the following command:

```bash
curl -s https://felix86.com/install.sh | /bin/bash
```

This command downloads and runs the installer script, which fetches the latest felix86 artifact and lets you either download a rootfs or use your own.

[Read the usage guide](./docs/how-to-use.md) for more information.

If you are a developer, [read the contributing guide](./docs/contributing.md).

Feel free to join our **Discord server**: [https://discord.gg/TgBxgFwByU](https://discord.gg/TgBxgFwByU)

## Features
- Just-in-Time (JIT) recompiler
- Uses the RISC-V Vector Extension for SSE instructions
- Utilizes the B extension, if available, for bit manipulation instructions like `bsr`
- Supports a variety of optional extensions, such as XThead custom extensions
- Can use the host libraries in some cases for improved performance

## Compatibility
felix86 is in the early stages of development.

A compatibility list can be found here: https://felix86.com/compat

## Dependencies
felix86 relies on several great projects:

- [FEX](https://github.com/FEX-Emu/FEX)'s comprehensive unit test suite
- [Biscuit](https://github.com/lioncash/biscuit) for RISC-V code emission
- [Zydis](https://github.com/zyantific/zydis) for decoding and disassembly
- [Catch2](https://github.com/catchorg/Catch2) for unit testing
- [fmt](https://github.com/fmtlib/fmt) for string formatting
- [nlohmann/json](https://github.com/nlohmann/json) for JSON parsing
- [toml11](https://github.com/ToruNiina/toml11) for TOML parsing

## Why?
felix86 was started for several reasons, including:

- Gaining a deeper understanding of x86-64, RISC-V, Linux, and high-level emulation
- Exploring optimizing compilers and JITs (SSA, register allocation, optimization passes, etc.)
- To learn more about low level details, such as signals, syscalls, program loading
- Taking on a fun and challenging project

## Also Check Out

- [Panda3DS](https://github.com/wheremyfoodat/Panda3DS), a 3DS emulator for Windows, macOS, Linux, and Android
- [shadPS4](https://github.com/shadps4-emu/shadPS4), one of the leading PS4 emulators
- [ChonkyStation3](https://github.com/liuk7071/ChonkyStation3), experimental HLE PS3 emulator for Windows, MacOS and Linux
