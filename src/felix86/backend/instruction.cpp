#include "felix86/backend/instruction.hpp"

BackendInstruction BackendInstruction::FromSSAInstruction(const SSAInstruction* inst) {
    BackendInstruction backend_inst;
    backend_inst.opcode = inst->GetOpcode();
    backend_inst.name = inst->GetName();
    backend_inst.immediate_data = inst->GetImmediateData();
    backend_inst.masked = inst->GetMasked();
    backend_inst.vector_state = inst->GetVectorState();
    backend_inst.locked = inst->IsLocked();
    backend_inst.desired_type = GetAllocationType(inst);

    backend_inst.operand_count = inst->GetOperandCount();
    for (u8 i = 0; i < inst->GetOperandCount(); i++) {
        backend_inst.operand_names[i] = inst->GetOperandName(i);
        backend_inst.operand_desired_types[i] = GetAllocationType(inst->GetOperand(i));
    }

    return backend_inst;
}

AllocationType BackendInstruction::GetAllocationType(const SSAInstruction* inst) {
    auto should_allocate_gpr = [](const SSAInstruction* inst) {
        bool not_zero = !inst->IsImmediate() || (inst->IsImmediate() && inst->GetImmediateData() != 0);
        return inst->IsGPR() && inst->GetOpcode() != IROpcode::GetThreadStatePointer && not_zero;
    };

    if (should_allocate_gpr(inst)) {
        return AllocationType::GPR;
    } else if (inst->IsVec()) {
        return AllocationType::Vec;
    } else {
        // If it's a zero immediate or thread state pointer or void we don't allocate it here
        return AllocationType::Null;
    }
}

BackendInstruction BackendInstruction::FromMove(u32 lhs, u32 rhs, AllocationType lhs_type, AllocationType rhs_type) {
    BackendInstruction inst;
    inst.opcode = IROpcode::Mov;
    inst.name = lhs;
    inst.operand_names[0] = rhs;
    inst.operand_count = 1;
    inst.desired_type = lhs_type;
    inst.operand_desired_types[0] = rhs_type;
    return inst;
}

BackendInstruction BackendInstruction::FromStoreSpill(u32 name, u32 value, u32 spill_offset) {
    BackendInstruction inst;
    inst.opcode = IROpcode::StoreSpill;
    inst.name = name;
    inst.immediate_data = spill_offset;
    inst.operand_count = 1;
    inst.operand_names[0] = value;
    inst.desired_type = AllocationType::Null;
    inst.locked = true;
    return inst;
}

BackendInstruction BackendInstruction::FromLoadSpill(u32 name, u32 spill_offset, AllocationType type) {
    BackendInstruction inst;
    inst.opcode = IROpcode::LoadSpill;
    inst.name = name;
    inst.immediate_data = spill_offset;
    inst.operand_count = 0;
    inst.desired_type = type;
    inst.locked = true;
    return inst;
}

extern std::string Print(IROpcode opcode, x86_ref_e ref, u32 name, const u32* operands, u64 immediate_data);

std::string BackendInstruction::Print() const {
    return ::Print(opcode, X86_REF_COUNT, name, operand_names.data(), immediate_data);
}

void BackendInstruction::Serialize(SerializedFunction& function) const {
    function.Push(name);
    function.Push(operand_count);
    for (int i = 0; i < operand_count; i++) {
        function.Push(operand_names[i]);
    }
    function.Push(immediate_data);
    function.Push(static_cast<u8>(opcode));
    function.Push(static_cast<u8>(desired_type));
    function.Push(static_cast<u8>(masked));
    function.Push(static_cast<u8>(vector_state));
}

BackendInstruction BackendInstruction::Deserialize(const SerializedFunction& function) {
    BackendInstruction inst;
    inst.name = function.Pop<u32>();
    inst.operand_count = function.Pop<u8>();
    for (int i = 0; i < inst.operand_count; i++) {
        inst.operand_names[i] = function.Pop<u32>();
    }

    inst.immediate_data = function.Pop<u64>();
    inst.opcode = static_cast<IROpcode>(function.Pop<u8>());
    inst.desired_type = static_cast<AllocationType>(function.Pop<u8>());
    inst.masked = static_cast<VecMask>(function.Pop<u8>());
    inst.vector_state = static_cast<VectorState>(function.Pop<u8>());

    return inst;
}