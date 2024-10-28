# Tasks

Feel free to implement any of these.

- Cross-compilation toolchain - be able to cross-compile felix86 on x86-64 hardware and run it on RISC-V hardware using CMake
- Environment variables that can override command line arguments
  - Such as for setting rootfs path with an environment variable, or enabling verbose output or block printing
  - Must also work with tests
  - Must warn the user when a command line argument is overwritten by an environment variable
- Option to save the logging output to a file in the POSIX tmp directory
- Use Zydis for instruction decoding instead of our own (see frontend.cpp)