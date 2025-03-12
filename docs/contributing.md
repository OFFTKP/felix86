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
felix86 needs root access to chroot. This means that debuggers like gdb also need root access.
Make sure to pass the environment variables when running them:
```
sudo -E gdb --args ./felix86 <args>
```

There's some functionality to make debugging easier. The `s11` register holds the thread state. You can view it in gdb with the command `p *((ThreadState*)$s11)`. You can add breakpoints to guest addresses, so that when a specific guest address runs an illegal instruction is executed so the debugger breaks. You can use `p guest_breakpoint("Region", 0x....)`. The first argument is the region, "Executable", "Interpreter" or the library name. The second argument is the offset inside this file. You can also break on an absolute guest address using `p guest_breakpoint_abs(0x...)`.

SIGBUS signals are commonly triggered so that our signal handler patches unaligned vector memory accesses. Make sure to run `handle SIGBUS nostop noprint` to silence them.

Some functions are helpful when called from gdb.
`call print_address(u64)` can pretty print an address. Feel free to use `call update_symbols()` which may help find some symbols.

`FELIX86_CALLTRACE=1` environment variable will make every thread have a calltrace. They can be viewed with `call dump_states()` in gdb. `call update_symbols()` can again help here.

`call disassemble_x64(u64)` can disassemble x64 code at an address, if you're in a hurry and don't wanna use ghidra.

## Profiling
felix86 will try to heuristically detect `perf` and emit symbols at `/tmp/perf-%pid.map`. If it fails to detect perf (it will tell you "Running under perf"), use `FELIX86_PERF=1` environment variable to force it to emit those symbols.

## Instruction counts
Generally we want to reduce the amount of RISC-V instructions needed to emulate each x86-64 instruction.
The current instruction counts can be seen in `counts/`. If you modify instructions, it would be good to update this list by using the
instruction count generator. The source is in `generate_instruction_count.cpp`. By default it doesn't build as it takes a long time,
you can set `-DBUILD_INSTRUCTION_COUNTER=1` in CMake configuration to build it. Running it like `./build/felix86_instruction_counter` 
should produce the new results.

## Coding
There are no strict coding guidelines. [Some recommendations exist](./conventions.md).
Do try to follow the coding style of the file you are editing.

Bug fixes, adding unit tests, implementing new instructions, syscalls etc. or implementing them in a more efficient way is what's needed the most.

Making the code easier to read or cleaner is welcome.

New features are welcome however it's preferred we discuss about them in the Discord server or in a Github issue.

You may find helpful information about how felix86 works in the rest of the docs in this folder.