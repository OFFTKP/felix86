Questions and answers about JIT design in case I forget why I did something or someone wonders.

# Why not use chroot?
Having to use chroot is clunky for multiple reasons. While it resolves all of our filesystem problems it creates new problems:
- When you chroot, there's no going back. This means that the felix86 binary and libraries it uses and dynamic linker need to be copied to the rootfs on every run
- Separate the felix86 emulator and the felix86 launcher? This means the felix86 launcher will need to fork and any attached gdb needs to follow this fork, which is clunky for new time contributors
- You need to launch felix86 explicitly using the dynamic linker, otherwise the system would default to the x86-64 dynamic linker. This means gdb won't have the symbols of felix86 loaded until the dynamic linker does its job
  - And yes, you can set a different PT_INTERP path and an RPATH. This created other annoyances that I mostly forgot
- felix86 would need administrator permission, which is annoying at best
- gdb and any other tool you'd want to attach also would need administrator permission
- Thunked libraries also need to be copied on every run
- Directories need to be mounted to the rootfs instead of symlinked

All in all, it makes the project less accessible to users and debug tooling, so we go through the trouble of emulating filesystem access ourselves

# Why load/store the entire state in the dispatcher instead of loading the regs you read and storing the regs you modify OR why not only writeback volatile registers on C function calls
Because of guest asynchronous signals. They can happen at any moment. While we are in JIT, the host registers hold the correct guest register values. While we are outside the JIT (dispatcher, C++ code, ...) then the ThreadState holds the correct values.

Technically you could extract just the modified registers if a signal happens in JIT code, which was our previous implementation. However then I would ask you how you'd do the same once you start worrying about a multiple block JIT that writebacks only at the end. Additionally, that requires decoding the instructions from the start of the block until the PC, which means you need a decoder (and the RISC-V decoder options are very lacking). Also in general I think it's clunky. Loading/storing the entire state on entry/exit is the way to go.
