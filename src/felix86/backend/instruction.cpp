#include "felix86/backend/instruction.hpp"

BackendInstruction BackendInstruction::FromSSAInstruction(const SSAInstruction* inst) {
    BackendInstruction backend_inst;
    backend_inst.opcode = inst->GetOpcode();
    backend_inst.name = inst->GetName();
    backend_inst.immediate_data = inst->GetImmediateData();

    if (inst->IsGPR()) {
        backend_inst.desired_type = AllocationType::GPR;
    } else if (inst->IsFPR()) {
        backend_inst.desired_type = AllocationType::FPR;
    } else if (inst->IsVec()) {
        backend_inst.desired_type = AllocationType::Vec;
    } else {
        backend_inst.desired_type = AllocationType::Null;
    }

    backend_inst.operand_count = inst->GetOperandCount();
    for (u8 i = 0; i < inst->GetOperandCount(); i++) {
        backend_inst.operand_names[i] = inst->GetOperandName(i);
    }

    return backend_inst;
}

BackendInstruction BackendInstruction::FromMove(u32 lhs, u32 rhs) {
    BackendInstruction inst;
    inst.opcode = IROpcode::Mov;
    inst.name = lhs;
    inst.operand_names[0] = rhs;
    inst.operand_count = 1;
    return inst;
}