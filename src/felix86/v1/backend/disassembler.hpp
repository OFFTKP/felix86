#include <string>
#include "felix86/common/utility.hpp"
#include "fmt/format.h"

extern "C" {
#include "riscv-disas.h"
}

struct Disassembler {
    static std::string Disassemble(void* data, u64 size) {
        char buf[256];
        size_t i = 0;
        rv_inst inst;
        size_t length;
        std::string result;
        while (i < size) {
            u64 address = (u64)data + i;
            inst_fetch((u8*)data + i, &inst, &length);
            disasm_inst(buf, sizeof(buf), rv64, address, inst);
            result += fmt::format("0x{:x}: {}\n", address, (const char*)buf);
            i += length;
        }
        return result;
    }
};