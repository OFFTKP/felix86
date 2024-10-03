#include <fmt/format.h>
#include "felix86/common/log.hpp"
#include "felix86/common/print.hpp"
#include "felix86/ir/block.hpp"

std::string IRBlock::Print() const {
    std::string ret;

    ret += fmt::format("Block {}", GetIndex());
    if (GetStartAddress() != IR_NO_ADDRESS) {
        ret += fmt::format(" @ 0x{:016x}", GetStartAddress());
    }
    ret += "\n";

    for (const IRInstruction& phi : phi_instructions) {
        std::string inst_string = phi.Print();
        ret += "    ";
        size_t size = inst_string.size();
        while (size < 50) {
            inst_string += " ";
            size++;
        }
        ret += inst_string;
        ret += "(uses: " + std::to_string(phi.GetUseCount()) + ")";
        ret += "\n";
    }

    for (const IRInstruction& inst : instructions) {
        std::string inst_string = inst.Print();
        if (inst.GetOpcode() != IROpcode::Comment) {
            ret += "    ";
            size_t size = inst_string.size();
            while (size < 50) {
                inst_string += " ";
                size++;
            }
        }
        ret += inst_string;
        if (inst.GetOpcode() != IROpcode::Comment)
            ret += "(uses: " + std::to_string(inst.GetUseCount()) + ")";
        ret += "\n";
    }

    return ret;
}