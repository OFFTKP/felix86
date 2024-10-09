// TODO: implement this
// Architectures like x86-64 can do 64-bit immediate loads. In RISC-V a 64-bit immediate load to register can take up to
// 8 (!) instructions. We can check if a nearby immediate holds a close enough value to the one we want to load and
// perform a move + add/sub to get the value we want, reducing code size and hopefully improving performance.