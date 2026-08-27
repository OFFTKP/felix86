// riscv-opcodes commit 5f869fc6fead9a58a05ac2715accb8e7635e6315
#ifndef RISCV_DISASSEMBLER_H
#define RISCV_DISASSEMBLER_H
#include <cstdint>
#include <string>
std::string riscv_disassemble(uint32_t data, uint64_t addr);
void riscv_set_felix86_allocations(bool enable);
#endif
