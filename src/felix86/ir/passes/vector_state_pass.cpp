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
    case IROpcode::CallHostFunction:
        return true;
    default:
        return false;
    }
}

void PassManager::VectorStatePass(BackendFunction* function) {
    // Block local for now
    for (auto& block : function->GetBlocks()) {
        auto it = block->GetInstructions().begin();
        VectorState state = VectorState::Null;
        while (it != block->GetInstructions().end()) {
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
            case IROpcode::SetVectorStateFloatBytes: {
                ASSERT(state != VectorState::FloatBytes); // would be redundant otherwise
                state = VectorState::FloatBytes;
                break;
            }
            case IROpcode::SetVectorStateDoubleBytes: {
                ASSERT(state != VectorState::DoubleBytes); // would be redundant otherwise
                state = VectorState::DoubleBytes;
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
                            it = block->GetInstructions().insert(it, backend_inst);
                            continue;
                        }
                        case VectorState::Double: {
                            SSAInstruction inst(IROpcode::SetVectorStateDouble, {});
                            BackendInstruction backend_inst = BackendInstruction::FromSSAInstruction(&inst);
                            it = block->GetInstructions().insert(it, backend_inst);
                            continue;
                        }
                        case VectorState::FloatBytes: {
                            SSAInstruction inst(IROpcode::SetVectorStateFloatBytes, {});
                            BackendInstruction backend_inst = BackendInstruction::FromSSAInstruction(&inst);
                            it = block->GetInstructions().insert(it, backend_inst);
                            continue;
                        }
                        case VectorState::DoubleBytes: {
                            SSAInstruction inst(IROpcode::SetVectorStateDoubleBytes, {});
                            BackendInstruction backend_inst = BackendInstruction::FromSSAInstruction(&inst);
                            it = block->GetInstructions().insert(it, backend_inst);
                            continue;
                        }
                        case VectorState::PackedByte: {
                            SSAInstruction inst(IROpcode::SetVectorStatePackedByte, {});
                            BackendInstruction backend_inst = BackendInstruction::FromSSAInstruction(&inst);
                            it = block->GetInstructions().insert(it, backend_inst);
                            continue;
                        }
                        case VectorState::PackedWord: {
                            SSAInstruction inst(IROpcode::SetVectorStatePackedWord, {});
                            BackendInstruction backend_inst = BackendInstruction::FromSSAInstruction(&inst);
                            it = block->GetInstructions().insert(it, backend_inst);
                            continue;
                        }
                        case VectorState::PackedDWord: {
                            SSAInstruction inst(IROpcode::SetVectorStatePackedDWord, {});
                            BackendInstruction backend_inst = BackendInstruction::FromSSAInstruction(&inst);
                            it = block->GetInstructions().insert(it, backend_inst);
                            continue;
                        }
                        case VectorState::PackedQWord: {
                            SSAInstruction inst(IROpcode::SetVectorStatePackedQWord, {});
                            BackendInstruction backend_inst = BackendInstruction::FromSSAInstruction(&inst);
                            it = block->GetInstructions().insert(it, backend_inst);
                            continue;
                        }
                        case VectorState::AnyPacked: {
                            if (!IsPacked(state)) {
                                // State is not packed, we need to set it to any packed state for this instruction
                                // and we don't care which one it is. However if we set it to some state that might
                                // be used later, that's even better.
                                // Scan forward to find the next state usage.
                                auto it2 = it;
                                VectorState next_state = VectorState::Null;
                                while (it2 != block->GetInstructions().end()) {
                                    if (it2->GetVectorState() != VectorState::Null) {
                                        next_state = it2->GetVectorState();
                                        break;
                                    }
                                    it2++;
                                }

                                if (next_state == VectorState::Null) {
                                    // Didn't find further usages, so just give it some packed state
                                    SSAInstruction inst(IROpcode::SetVectorStatePackedDWord, {});
                                    BackendInstruction backend_inst = BackendInstruction::FromSSAInstruction(&inst);
                                    it = block->GetInstructions().insert(it, backend_inst);
                                    continue;
                                } else {
                                    IROpcode opcode = IROpcode::Null;
                                    switch (next_state) {
                                    case VectorState::FloatBytes:
                                    case VectorState::DoubleBytes:
                                    case VectorState::PackedByte:
                                        opcode = IROpcode::SetVectorStatePackedByte;
                                        break;
                                    case VectorState::PackedWord:
                                        opcode = IROpcode::SetVectorStatePackedWord;
                                        break;
                                    case VectorState::AnyPacked:
                                    case VectorState::PackedDWord:
                                    case VectorState::Float:
                                        opcode = IROpcode::SetVectorStatePackedDWord;
                                        break;
                                    case VectorState::Double:
                                    case VectorState::PackedQWord:
                                        opcode = IROpcode::SetVectorStatePackedQWord;
                                        break;
                                    default:
                                        UNREACHABLE();
                                        break;
                                    }

                                    SSAInstruction inst(opcode, {});
                                    BackendInstruction backend_inst = BackendInstruction::FromSSAInstruction(&inst);
                                    it = block->GetInstructions().insert(it, backend_inst);
                                    continue;
                                }
                            }
                            break;
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