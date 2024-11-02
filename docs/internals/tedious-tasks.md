# Tasks

Feel free to implement any of these.

- Cross-compilation toolchain - be able to cross-compile felix86 on x86-64 hardware and run it on RISC-V hardware using CMake
- Option to save the logging output to a file in the POSIX tmp directory
- Use Zydis for instruction decoding instead of our own (see frontend.cpp)
- Precolored registers for some function calls like Div128 instead of pushing a1, and also don't spill every register (see EmitCallHostFunction's horrible implementation)