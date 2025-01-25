# How to contribute

Contributions are welcome!

## Testing
You can contribute by testing felix86 and reporting bugs.

Running the test suite or programs on RISC-V hardware is appreciated since proper RISC-V hardware with all the necessary extensions is scarce at the moment.

You need a board with the RV64GV ISA and at least 128-bit vector length.

You can also test on QEMU. See [how to use](./../how-to-use.md).

### Reporting bugs
Please [open an issue](https://github.com/OFFTKP/felix86/issues/new) if you find a bug.
Mention what x86-64 executable you tried to emulate and what you did.

You may also need to enable strace. Use the `-t` flag to do so.

## Debugging
There's some functionality to make debugging easier. The `s11` register holds the thread state. You can view it in gdb with the command `p *((ThreadState*)$s11)`. You can add breakpoints to guest addresses, so that when a specific guest address runs an illegal instruction is executed so the debugger breaks. You can use `p guest_breakpoint("Region", 0x....)`. The first argument is the region, "Executable", "Interpreter" or the library name. The second argument is the offset inside this file. You can also break on an absolute guest address using `p guest_breakpoint_abs(0x...)`.

SIGBUS signals are commonly triggered so that our signal handler patches unaligned vector memory accesses. Make sure to run `handle SIGBUS nostop noprint` to silence them.

## Coding
There are no strict coding guidelines. [Some recommendations exist](./conventions.md).
Do try to follow the coding style of the file you are editing.

Bug fixes, adding unit tests, implementing new instructions, syscalls etc. or implementing them in a more efficient way is what's needed the most.

Making the code easier to read or cleaner is welcome.

New features are welcome however it's preferred we discuss about them in the Discord server or in a Github issue.

You may find helpful information about how felix86 works in the rest of the docs in this folder.