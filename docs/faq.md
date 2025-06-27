Questions and answers about the emulator's design in case I forget why I did something or someone wonders.

# Why not use chroot?
Having to use chroot is clunky for multiple reasons. While it resolves all of our filesystem problems it creates new problems:
- When you chroot, there's no going back. This means that the felix86 binary and libraries it uses and dynamic linker need to be copied to the rootfs on every run
  - Or statically link! Which has it's own set of problems... Also did you know -static-pie is just broken on RISC-V atm?
- felix86 would need administrator permission, which is annoying at best. "But namespaces?" see "Why not use namespaces?"
- gdb and any other tool you'd want to attach also would need administrator permission
- How will we find thunk libraries and RISC-V libraries?

All in all, it makes the project less accessible to users, screws up thunking and debug tooling.

# Why not use namespaces?
Namespaces allow you to mount and chroot without elevated permissions!
For example: `unshare(CLONE_NEWNS | CLONE_NEWUSER)` will make a new mount namespace. We can mount and chroot without root perfectly fine there!

Two problems:
- Inside the namespace, we can't run setuid apps like `sudo`, `mount`. The permissions of these change inside the namespace and we just can't use them properly. This will break AppImages and other stuff
- Children inherit namespaces, which we like, but separate felix86 instances would have separate namespaces, which we don't want

# Why load/store the entire state in the dispatcher instead of loading the regs you read and storing the regs you modify OR why not only writeback volatile registers on C function calls?
Because of guest asynchronous signals. They can happen at any moment. While we are in JIT, the host registers hold the correct guest register values. While we are outside the JIT (dispatcher, C++ code, ...) then the ThreadState holds the correct values.

Technically you could extract just the modified registers if a signal happens in JIT code, which was our previous implementation. However then I would ask you how you'd do the same once you start worrying about a multiple block JIT that writebacks only at the end. Additionally, that requires decoding the instructions from the start of the block until the PC, which means you need a decoder (and the RISC-V decoder options are very lacking). Also in general I think it's clunky. Loading/storing the entire state on entry/exit is the way to go.
