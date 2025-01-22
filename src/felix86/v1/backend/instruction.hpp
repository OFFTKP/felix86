#pragma once

#include <array>
#include "felix86/backend/allocation.hpp"
#include "felix86/backend/serialized_function.hpp"
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

    void SetName(u32 new_name) {
        name = new_name;
    }

    u32 GetOperand(u32 index) const {
        return operand_names[index];
    }

    void SetOperand(u32 index, u32 value, AllocationType type) {
        operand_names[index] = value;
        operand_desired_types[index] = type;
    }

    u8 GetOperandCount() const {
        return operand_count;
    }

    VecMask GetMask() const {
        return masked;
    }

    AllocationType GetDesiredType() const {
        return desired_type;
    }

    AllocationType GetOperandDesiredType(u32 index) const {
        return operand_desired_types[index];
    }

    VectorState GetVectorState() const {
        return vector_state;
    }

    bool IsLocked() const {
        return locked;
    }

    static BackendInstruction FromSSAInstruction(const SSAInstruction* inst);

    static BackendInstruction FromMove(u32 lhs, u32 rhs, AllocationType lhs_type, AllocationType rhs_type);

    static BackendInstruction FromStoreSpill(u32 name, u32 value, u32 spill_offset);

    static BackendInstruction FromLoadSpill(u32 name, u32 spill_offset, AllocationType type);

    static AllocationType GetAllocationType(const SSAInstruction* inst);

    [[nodiscard]] std::string Print() const;

    void Serialize(SerializedFunction& function) const;

    static BackendInstruction Deserialize(const SerializedFunction& function);

private:
    friend struct BackendBlock;
    friend struct BackendFunction;

    u64 immediate_data = 0;
    std::array<u32, 4> operand_names{};
    u32 name = 0;
    IROpcode opcode = IROpcode::Null;
    AllocationType desired_type = AllocationType::Null;
    std::array<AllocationType, 4> operand_desired_types{};
    VecMask masked = VecMask::No;
    u8 operand_count = 0;
    VectorState vector_state = VectorState::Null;
    bool locked = false;
};