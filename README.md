# felix86
An x86-64 userspace emulator for RISC-V Linux

## Similar projects
felix86 is very early in development. If you want a more complete emulator, use one of the following:

- [FEX](https://github.com/FEX-Emu/FEX)
- [box64](https://github.com/ptitSeb/box64)
- [qemu-user](https://www.qemu.org/docs/master/user/main.html)

## Useful documentation for x86-64 emulators
- [x86-64 instruction reference](https://www.felixcloutier.com/x86/)
- [x86-64 opcode tables](http://ref.x86asm.net/coder64.html)
- [sandpile.org](https://sandpile.org/)
- [An optimization guide for x86 platforms by Agner Fog](https://www.agner.org/optimize/optimizing_assembly.pdf)
- [Intel® 64 and IA-32 Architectures Software Developer’s Manual](https://software.intel.com/en-us/download/intel-64-and-ia-32-architectures-sdm-combined-volumes-1-2a-2b-2c-2d-3a-3b-3c-3d-and-4), the most complete resource however unfortunately it's hard to find what you need sometimes

## Useful documentation for emulator JITs
- [JIT wikipedia entry](https://en.wikipedia.org/wiki/Just-in-time_compilation)
- [Basic block](https://en.wikipedia.org/wiki/Basic_block)
- [Static single assignment](https://en.wikipedia.org/wiki/Static_single-assignment_form)
- [Common subexpression elimination](https://en.wikipedia.org/wiki/Common_subexpression_elimination)
- [Copy propagation](https://en.wikipedia.org/wiki/Copy_propagation), useful for after doing common subexpression elimination
- [Constant propagation](https://en.wikipedia.org/wiki/Constant_folding)
- [Dead store](https://en.wikipedia.org/wiki/Dead_store)
- [Linear scan register allocation](https://web.cs.ucla.edu/~palsberg/course/cs132/linearscan.pdf)
- [Efficiently Computing Static Single Assignment Form and the Control Dependence Graph](https://www.cs.utexas.edu/%7Epingali/CS380C/2010/papers/ssaCytron.pdf)
- [Simple and Efficient Construction of Static Single Assignment Form](https://link.springer.com/chapter/10.1007/978-3-642-37051-9_6)
- Yuzu source code
- [Dynarmic source code & docs folder](https://github.com/PabloMK7/dynarmic/)
- [Dillonb's n64 source code](https://github.com/Dillonb/n64/tree/master/src/cpu/dynarec/v2)
- [PCSX-Redux source code](https://github.com/grumpycoders/pcsx-redux)
- [Duckstation source code](https://github.com/stenzek/duckstation)

## Other useful documentation
- [How programs get run: ELF binaries](https://lwn.net/Articles/631631/)
- [ELF format specification](http://www.skyfree.org/linux/references/ELF_Format.pdf), really pleasant read
- [System-V ABI - AMD64 Architecture Supplement](./docs/sysv-x86-64.pdf), particularly Chapter 3.4 on Process Initialization and Chapter 5 on Program Loading 