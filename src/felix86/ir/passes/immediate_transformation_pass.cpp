// Architectures like x86-64 can do 64-bit immediate loads. In RISC-V a 64-bit immediate load to register can take up to
// 8 (!) instructions. We can check if a nearby immediate holds a close enough value to the one we want to load and
// perform a ADDI to get the value we want, reducing code size and hopefully improving performance.

#include "felix86/ir/passes/passes.hpp"

void PassManager::ImmediateTransformationPass(IRFunction* function) {
    for (IRBlock* block : function->GetBlocks()) {
        std::vector<SSAInstruction*> immediates;
        for (SSAInstruction& inst : block->GetInstructions()) {
            if (inst.IsImmediate()) {
                for (SSAInstruction* imm : immediates) {
                    i64 this_immediate = inst.GetImmediateData();
                    i64 other_immediate = imm->GetImmediateData();
                    i64 diff = this_immediate - other_immediate;
                    if (IsValidSigned12BitImm(diff)) {
                        Operands op;
                        op.operand_count = 1;
                        op.immediate_data = diff;
                        op.operands[0] = imm;
                        inst.Replace(op, IROpcode::Addi);
                        break;
                    }
                }

                immediates.push_back(&inst);
            }
        }
    }
}