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

    void SetOperand(u32 index, u32 value) {
        operand_names[index] = value;
    }

    u8 GetOperandCount() const {
        return operand_count;
    }

    VecMask GetMask() const {
        return mask;
    }

    AllocationType GetDesiredType() const {
        return desired_type;
    }

    void Rename(u32 new_name) {
        name = new_name;
    }

    void SetCurrentState(VectorState state) {
        current_state = state;
    }

    VectorState GetCurrentState() const {
        return current_state;
    }

    static BackendInstruction FromSSAInstruction(const SSAInstruction* inst);

    static BackendInstruction FromMove(u32 lhs, u32 rhs, AllocationType type);

    static BackendInstruction FromStoreSpill(u32 name, u32 value, u32 spill_offset);

    static BackendInstruction FromLoadSpill(u32 name, u32 spill_offset, AllocationType type);

    [[nodiscard]] std::string Print() const;

private:
    u64 immediate_data;
    std::array<u32, 4> operand_names;
    u32 name;
    IROpcode opcode;
    AllocationType desired_type = AllocationType::Null;
    VecMask mask = VecMask::No;
    u8 operand_count;
    VectorState current_state = VectorState::Null;
};