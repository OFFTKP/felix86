// Architectures like x86-64 can do 64-bit immediate loads. In RISC-V a 64-bit immediate load to register can take up to
// 8 (!) instructions. We can check if a nearby immediate holds a close enough value to the one we want to load and
// perform a ADDI to get the value we want, reducing code size and hopefully improving performance.

#include "felix86/ir/passes/passes.hpp"

void PassManager::ImmediateTransformationPass(IRFunction* function) {
    for (IRBlock* block : function->GetBlocks()) {
        std::vector<SSAInstruction*> immediates;
        for (SSAInstruction& inst : block->GetInstructions()) {
            if (inst.IsImmediate()) {
                i64 this_immediate = inst.GetImmediateData();
                if (this_immediate == 0) {
                    continue;
                }

                // Small enough that we don't even wanna account for it, it would just be a bigger interval
                // in reg allocator for no reason
                if (static_cast<uint64_t>(static_cast<int64_t>(this_immediate << 32) >> 32) == (u64)this_immediate) {
                    continue;
                }

                bool replaced = false;
                for (SSAInstruction* imm : immediates) {
                    i64 other_immediate = imm->GetImmediateData();
                    i64 diff = this_immediate - other_immediate;
                    if (IsValidSigned12BitImm(diff)) {
                        Operands op;
                        op.operand_count = 1;
                        op.immediate_data = diff;
                        op.operands[0] = imm;
                        inst.Replace(op, IROpcode::Addi);
                        replaced = true;
                        break;
                    }
                }

                if (!replaced) {
                    immediates.push_back(&inst);
                }
            }
        }
    }
}