// A bad register allocator for debugging purposes, just spills everything

#include "felix86/backend/registers.hpp"
#include "felix86/common/x86.hpp"
#include "felix86/ir/passes/passes.hpp"

void ir_spill_everything_pass(IRFunction* function) {
    tsl::robin_map<u32, Allocation> allocations;
    u32 spill_count = 1;
    auto postorder = function->GetBlocksPostorder();
    auto it = postorder.rbegin();
    auto end = postorder.rend();
    for (; it != end; it++) {
        IRBlock* block = *it;
        for (const ReducedInstruction& inst : block->GetReducedInstructions()) {
            // Just spill it somewhere
            BackendInstruction binst(Allocation(spill_count++));
            binst.opcode = inst.opcode;

            switch (inst.opcode) {
            case IROpcode::LoadGuestFromMemory: {
                // Break this to a regular load
                Allocation address = Registers::ThreadStatePointer();
                switch (inst.ref) {
                case X86_REF_RAX ... X86_REF_R15: {
                    binst.opcode = IROpcode::ReadQWordRelative;
                    binst.operand_count = 1;
                    binst.operands[0] = address;
                    binst.immediate_data = offsetof(ThreadState, gprs[inst.ref - X86_REF_RAX]);
                    break;
                }
                case X86_REF_RIP: {
                    binst.opcode = IROpcode::ReadQWordRelative;
                    binst.operand_count = 1;
                    binst.operands[0] = address;
                    binst.immediate_data = offsetof(ThreadState, rip);
                    break;
                }
                case X86_REF_CF: {
                    binst.opcode = IROpcode::ReadByteRelative;
                    binst.operand_count = 1;
                    binst.operands[0] = address;
                    binst.immediate_data = offsetof(ThreadState, cf);
                    break;
                }
                case X86_REF_ZF: {
                    binst.opcode = IROpcode::ReadByteRelative;
                    binst.operand_count = 1;
                    binst.operands[0] = address;
                    binst.immediate_data = offsetof(ThreadState, zf);
                    break;
                }
                case X86_REF_AF: {
                    binst.opcode = IROpcode::ReadByteRelative;
                    binst.operand_count = 1;
                    binst.operands[0] = address;
                    binst.immediate_data = offsetof(ThreadState, af);
                    break;
                }
                case X86_REF_PF: {
                    binst.opcode = IROpcode::ReadByteRelative;
                    binst.operand_count = 1;
                    binst.operands[0] = address;
                    binst.immediate_data = offsetof(ThreadState, pf);
                    break;
                }
                case X86_REF_SF: {
                    binst.opcode = IROpcode::ReadByteRelative;
                    binst.operand_count = 1;
                    binst.operands[0] = address;
                    binst.immediate_data = offsetof(ThreadState, sf);
                    break;
                }
                case X86_REF_OF: {
                    binst.opcode = IROpcode::ReadByteRelative;
                    binst.operand_count = 1;
                    binst.operands[0] = address;
                    binst.immediate_data = offsetof(ThreadState, of);
                    break;
                }
                default: {
                    UNIMPLEMENTED();
                    break;
                }
                }
                break;
            }
            case IROpcode::StoreGuestToMemory: {
                // Break this to a regular store
                Allocation address = Registers::ThreadStatePointer();
                if (allocations.find(inst.operands[0]) == allocations.end()) {
                    ERROR("Use not dominated by definition while allocating, operand name: %s", GetNameString(inst.operands[0]).c_str());
                }
                switch (inst.ref) {
                case X86_REF_RAX ... X86_REF_R15: {
                    binst.opcode = IROpcode::WriteQWordRelative;
                    binst.operand_count = 2;
                    binst.operands[0] = address;
                    binst.operands[1] = allocations[inst.operands[0]];
                    binst.immediate_data = offsetof(ThreadState, gprs[inst.ref - X86_REF_RAX]);
                    break;
                }
                case X86_REF_RIP: {
                    binst.opcode = IROpcode::WriteQWordRelative;
                    binst.operand_count = 2;
                    binst.operands[0] = address;
                    binst.operands[1] = allocations[inst.operands[0]];
                    binst.immediate_data = offsetof(ThreadState, rip);
                    break;
                }
                case X86_REF_CF: {
                    binst.opcode = IROpcode::WriteByteRelative;
                    binst.operand_count = 2;
                    binst.operands[0] = address;
                    binst.operands[1] = allocations[inst.operands[0]];
                    binst.immediate_data = offsetof(ThreadState, cf);
                    break;
                }
                case X86_REF_ZF: {
                    binst.opcode = IROpcode::WriteByteRelative;
                    binst.operand_count = 2;
                    binst.operands[0] = address;
                    binst.operands[1] = allocations[inst.operands[0]];
                    binst.immediate_data = offsetof(ThreadState, zf);
                    break;
                }
                case X86_REF_AF: {
                    binst.opcode = IROpcode::WriteByteRelative;
                    binst.operand_count = 2;
                    binst.operands[0] = address;
                    binst.operands[1] = allocations[inst.operands[0]];
                    binst.immediate_data = offsetof(ThreadState, af);
                    break;
                }
                case X86_REF_PF: {
                    binst.opcode = IROpcode::WriteByteRelative;
                    binst.operand_count = 2;
                    binst.operands[0] = address;
                    binst.operands[1] = allocations[inst.operands[0]];
                    binst.immediate_data = offsetof(ThreadState, pf);
                    break;
                }
                case X86_REF_SF: {
                    binst.opcode = IROpcode::WriteByteRelative;
                    binst.operand_count = 2;
                    binst.operands[0] = address;
                    binst.operands[1] = allocations[inst.operands[0]];
                    binst.immediate_data = offsetof(ThreadState, sf);
                    break;
                }
                case X86_REF_OF: {
                    binst.opcode = IROpcode::WriteByteRelative;
                    binst.operand_count = 2;
                    binst.operands[0] = address;
                    binst.operands[1] = allocations[inst.operands[0]];
                    binst.immediate_data = offsetof(ThreadState, of);
                    break;
                }
                default: {
                    UNIMPLEMENTED();
                    break;
                }
                }
                break;
            }

            case IROpcode::Immediate: {
                if (inst.immediate_data == 0) {
                    allocations[inst.name] = Allocation(Registers::Zero());
                    continue;
                }
                [[fallthrough]];
            }
            default: {
                binst.operand_count = inst.operand_count;
                for (u8 i = 0; i < inst.operand_count; i++) {
                    if (allocations.find(inst.operands[i]) == allocations.end()) {
                        ERROR("Use not dominated by definition while allocating, opcode: %s, operand: %s", GetOpcodeString(inst.opcode).c_str(),
                              GetNameString(i).c_str());
                    }

                    binst.operands[i] = allocations[inst.operands[i]];
                }
                break;
            }
            }

            if (inst.name == block->GetConditionName()) {
                block->SetConditionAllocation(binst.allocation);
            }

            block->InsertBackendInstruction(std::move(binst));
            allocations[inst.name] = binst.allocation;
        }

        if (block->GetTermination() == Termination::JumpConditional && !block->GetConditionAllocation().IsValid()) {
            ERROR("Condition not defined");
        }
    }
}