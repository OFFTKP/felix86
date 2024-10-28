# How to contribute

Contributions are welcome!

## Reporting bugs
Please [open an issue](https://github.com/OFFTKP/felix86/issues/new) if you find a bug.
Explain what executable you tried to use and what you did.

A verbose log is appreciated. Run felix86 with the `-v` flag, redirect its output to a file and attach it to your issue.
E.g: `./felix86 -v <other args here> > out.txt`

## Testing
Running the test suite or programs on hardware is appreciated since proper RISC-V hardware with all the necessary extensions is scarce at the moment.

You need a board with the RV64GCV ISA and at least 128-bit vector length.

You can also test on Qemu. See [how to use](./../how-to-use.md).

## Coding
There are no strict coding guidelines. [Some recommendations exist](./conventions.md).
Do try to follow the coding style of the file you are editing.

Bug fixes, adding unit tests, implementing new instructions, syscalls etc. or implementing them in a more efficient way is
what's needed the most.

Tedious tasks that are beneficial to users or improve the codebase are very appreciated. [Some are listed here](./tedious-tasks.md).

New features are welcome however it's preferred we discuss about them on the Discord server or in a Github issue.

You may find helpful information about how felix86 works in the rest of the docs in this folder.