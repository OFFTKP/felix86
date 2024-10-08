#pragma once

#include <array>
#include "felix86/common/utility.hpp"
#include "felix86/ir/allocation.hpp"
#include "felix86/ir/opcode.hpp"

struct BackendInstruction {
    IROpcode GetOpcode() const {
        return opcode;
    }

    u64 GetImmediateData() const {
        return immediate_data;
    }

    const Allocation& GetAllocation() const {
        return allocation;
    }

    const Allocation& GetOperand(u8 index) const {
        return operands[index];
    }
private:
    std::array<Allocation, 4> operands;
    Allocation allocation;
    u64 immediate_data;
    IROpcode opcode;
};