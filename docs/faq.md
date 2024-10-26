# FAQ

### How to use?
See [how-to-use.md](./how-to-use.md)

### Is felix86 a sandbox?
felix86 makes a faithful attempt to sandbox the emulated application, but should **not** be considered a security application and has absolutely no security guarantees.

felix86 generally tries to only allow the emulated app to access files inside the rootfs directory. However it may be the case that due
to oversights or bugs in the code an emulated app might be able to exit this sandbox. Only use the emulator with applications you know
for sure are not malicious.

### Why place an emphasis on compiler optimizations, when a JIT is supposed to compile fast?
This project started with the idea of an x86-64 to RISC-V AOT compiler. AOT compilation is not always possible, but there's still the goal
of attempting to AOT compile entire binaries in the future, and falling back to a JIT on rare occasions such as recompiled code inside the app, or if AOT fails to find some address.
Thus felix86 started as an emulator with a JIT compiler, that hopes to one day evolve into an AOT compiler with a JIT fallback, like Rosetta.

### Will this project support AArch64 as a backend? (x86-64 on AArch64)
Most likely not for a long while. Since felix86 is made with optimization in mind, AArch64 backend support would require making extensive
use of the flags (NZCV) to emulate the x86-64 flags. There's currently no such functionality in the IR as RISC-V doesn't have flags,
so it would require extensive refactoring which I am not interested in doing.
Furthermore there's already some well established x86-64 on AArch64 userspace emulators that can be used.

### What about some other architecture as a backend? (PowerPC, MIPS, ???)
Probably not as there's very little demand for such a thing.

### Will this project support AArch64 as a frontend? (AArch64 on RISC-V)
It would be interesting to do some day.