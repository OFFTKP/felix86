#include "felix86/ir/passes/passes.hpp"

bool IsPacked(VectorState state) {
    return state == VectorState::PackedByte || state == VectorState::PackedWord || state == VectorState::PackedDWord ||
           state == VectorState::PackedQWord;
}

bool ExitsVM(IROpcode opcode) {
    switch (opcode) {
    case IROpcode::Syscall:
    case IROpcode::Cpuid:
    case IROpcode::Rdtsc:
    case IROpcode::Div128:
    case IROpcode::Divu128:
        return true;
    default:
        return false;
    }
}

void PassManager::VectorStatePass(BackendFunction* function) {
    // Block local for now
    for (auto& block : function->GetBlocks()) {
        auto it = block.GetInstructions().begin();
        VectorState state = VectorState::Null;
        while (it != block.GetInstructions().end()) {
            BackendInstruction& inst = *it;
            switch (inst.GetOpcode()) {
            case IROpcode::SetVectorStateFloat: {
                ASSERT(state != VectorState::Float); // would be redundant otherwise
                state = VectorState::Float;
                break;
            }
            case IROpcode::SetVectorStateDouble: {
                ASSERT(state != VectorState::Double); // would be redundant otherwise
                state = VectorState::Double;
                break;
            }
            case IROpcode::SetVectorStatePackedByte: {
                ASSERT(state != VectorState::PackedByte); // would be redundant otherwise
                state = VectorState::PackedByte;
                break;
            }
            case IROpcode::SetVectorStatePackedWord: {
                ASSERT(state != VectorState::PackedWord); // would be redundant otherwise
                state = VectorState::PackedWord;
                break;
            }
            case IROpcode::SetVectorStatePackedDWord: {
                ASSERT(state != VectorState::PackedDWord); // would be redundant otherwise
                state = VectorState::PackedDWord;
                break;
            }
            case IROpcode::SetVectorStatePackedQWord: {
                ASSERT(state != VectorState::PackedQWord); // would be redundant otherwise
                state = VectorState::PackedQWord;
                break;
            }
            default: {
                if (ExitsVM(inst.GetOpcode())) {
                    state = VectorState::Null;
                } else if (inst.GetVectorState() != VectorState::Null) {
                    if (inst.GetVectorState() != state) {
                        // If there's a mismatch between the previous state and this state, we need to insert
                        // a vsetivli instruction to change the vector state
                        switch (inst.GetVectorState()) {
                        case VectorState::Float: {
                            SSAInstruction inst(IROpcode::SetVectorStateFloat, {});
                            BackendInstruction backend_inst = BackendInstruction::FromSSAInstruction(&inst);
                            it = block.GetInstructions().insert(it, backend_inst);
                            continue;
                        }
                        case VectorState::Double: {
                            SSAInstruction inst(IROpcode::SetVectorStateDouble, {});
                            BackendInstruction backend_inst = BackendInstruction::FromSSAInstruction(&inst);
                            it = block.GetInstructions().insert(it, backend_inst);
                            continue;
                        }
                        case VectorState::PackedByte: {
                            SSAInstruction inst(IROpcode::SetVectorStatePackedByte, {});
                            BackendInstruction backend_inst = BackendInstruction::FromSSAInstruction(&inst);
                            it = block.GetInstructions().insert(it, backend_inst);
                            continue;
                        }
                        case VectorState::PackedWord: {
                            SSAInstruction inst(IROpcode::SetVectorStatePackedWord, {});
                            BackendInstruction backend_inst = BackendInstruction::FromSSAInstruction(&inst);
                            it = block.GetInstructions().insert(it, backend_inst);
                            continue;
                        }
                        case VectorState::PackedDWord: {
                            SSAInstruction inst(IROpcode::SetVectorStatePackedDWord, {});
                            BackendInstruction backend_inst = BackendInstruction::FromSSAInstruction(&inst);
                            it = block.GetInstructions().insert(it, backend_inst);
                            continue;
                        }
                        case VectorState::PackedQWord: {
                            SSAInstruction inst(IROpcode::SetVectorStatePackedQWord, {});
                            BackendInstruction backend_inst = BackendInstruction::FromSSAInstruction(&inst);
                            it = block.GetInstructions().insert(it, backend_inst);
                            continue;
                        }
                        case VectorState::Null:
                            UNREACHABLE();
                            break;
                        }
                    }
                }
                break;
            }
            }

            it++;
        }
    }
}