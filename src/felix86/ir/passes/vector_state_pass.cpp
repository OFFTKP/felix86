#include "felix86/ir/passes/passes.hpp"

bool IsPacked(VectorState state) {
    return state == VectorState::PackedByte || state == VectorState::PackedWord || state == VectorState::PackedDWord ||
           state == VectorState::PackedQWord;
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
                if (state == VectorState::Float) {
                    it = block.GetInstructions().erase(it);
                    continue;
                }
                state = VectorState::Float;
                break;
            }
            case IROpcode::SetVectorStateDouble: {
                if (state == VectorState::Double) {
                    it = block.GetInstructions().erase(it);
                    continue;
                }
                state = VectorState::Double;
                break;
            }
            case IROpcode::SetVectorStatePackedByte: {
                if (state == VectorState::PackedByte) {
                    it = block.GetInstructions().erase(it);
                    continue;
                }
                state = VectorState::PackedByte;
                break;
            }
            case IROpcode::SetVectorStatePackedWord: {
                if (state == VectorState::PackedWord) {
                    it = block.GetInstructions().erase(it);
                    continue;
                }
                state = VectorState::PackedWord;
                break;
            }
            case IROpcode::SetVectorStatePackedDWord: {
                if (state == VectorState::PackedDWord) {
                    it = block.GetInstructions().erase(it);
                    continue;
                }
                state = VectorState::PackedDWord;
                break;
            }
            case IROpcode::SetVectorStatePackedQWord: {
                if (state == VectorState::PackedQWord) {
                    it = block.GetInstructions().erase(it);
                    continue;
                }
                state = VectorState::PackedQWord;
                break;
            }
            case IROpcode::WriteXmmWord:
            case IROpcode::WriteXmmWordRelative:
            case IROpcode::ReadXmmWord:
            case IROpcode::ReadXmmWordRelative: {
                if (!IsPacked(state)) {
                    // Must insert a SetVectorStatePackedByte instruction before this
                    SSAInstruction inst(IROpcode::SetVectorStatePackedByte, {});
                    BackendInstruction new_inst = BackendInstruction::FromSSAInstruction(&inst);
                    it = block.GetInstructions().insert(it, new_inst);
                    continue;
                } else {
                    inst.SetCurrentState(state);
                }
                break;
            }
            default:
                break;
            }

            it++;
        }
    }
}