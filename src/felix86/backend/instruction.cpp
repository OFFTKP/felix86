#include "felix86/backend/instruction.hpp"

BackendInstruction BackendInstruction::FromSSAInstruction(const SSAInstruction* inst) {
    BackendInstruction backend_inst;
    backend_inst.opcode = inst->GetOpcode();
    backend_inst.name = inst->GetName();
    backend_inst.immediate_data = inst->GetImmediateData();
    backend_inst.masked = inst->GetMasked();
    backend_inst.vector_state = inst->GetVectorState();
    backend_inst.locked = inst->IsLocked();

    if (inst->IsGPR()) {
        backend_inst.desired_type = AllocationType::GPR;
    } else if (inst->IsVec()) {
        backend_inst.desired_type = AllocationType::Vec;
    } else if (inst->IsVoid()) {
        backend_inst.desired_type = AllocationType::Null;
    } else {
        UNREACHABLE();
    }

    backend_inst.operand_count = inst->GetOperandCount();
    for (u8 i = 0; i < inst->GetOperandCount(); i++) {
        backend_inst.operand_names[i] = inst->GetOperandName(i);
    }

    return backend_inst;
}

BackendInstruction BackendInstruction::FromMove(u32 lhs, u32 rhs, AllocationType type) {
    BackendInstruction inst;
    inst.opcode = IROpcode::Mov;
    inst.name = lhs;
    inst.operand_names[0] = rhs;
    inst.operand_count = 1;
    inst.desired_type = type;
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