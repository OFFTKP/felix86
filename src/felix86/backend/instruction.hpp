#pragma once

#include <array>
#include "felix86/backend/allocation.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/ir/instruction.hpp"
#include "felix86/ir/opcode.hpp"

struct BackendInstruction {
    IROpcode GetOpcode() const {
        return opcode;
    }

    u64 GetImmediateData() const {
        return immediate_data;
    }

    u32 GetName() const {
        return name;
    }

    u32 GetOperand(u32 index) const {
        return operand_names[index];
    }

    u8 GetOperandCount() const {
        return operand_count;
    }

    AllocationType GetDesiredType() const {
        return desired_type;
    }

    static BackendInstruction FromSSAInstruction(const SSAInstruction* inst);

    static BackendInstruction FromMove(u32 lhs, u32 rhs, AllocationType type);

private:
    u64 immediate_data;
    std::array<u32, 4> operand_names;
    u32 name;
    IROpcode opcode;
    AllocationType desired_type = AllocationType::Null;
    u8 operand_count;
};