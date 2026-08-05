#include "felix86/common/state.hpp"
#include "felix86/v2/optimizer.hpp"
#include "felix86/v2/recompiler.hpp"

static bool is_gpr(ZydisDecodedOperand& operand) {
    if (operand.type == ZYDIS_OPERAND_TYPE_REGISTER) {
        ZydisRegister reg = operand.reg.value;
        return reg >= ZYDIS_REGISTER_AL && reg <= ZYDIS_REGISTER_R15;
    }
    return false;
}

static void convert_instruction(Pair& pair, ZydisRegister new_src1) {
    auto& [instruction, operands] = pair;
    ZydisMnemonic mnemonic = instruction.mnemonic;
    ZydisMnemonic custom_mnemonic;
    switch (mnemonic) {
    case ZYDIS_MNEMONIC_SUB:
        custom_mnemonic = ZYDIS_MNEMONIC_FELIX86_SUB;
        break;
    case ZYDIS_MNEMONIC_OR:
        custom_mnemonic = ZYDIS_MNEMONIC_FELIX86_OR;
        break;
    case ZYDIS_MNEMONIC_XOR:
        custom_mnemonic = ZYDIS_MNEMONIC_FELIX86_XOR;
        break;
    case ZYDIS_MNEMONIC_AND:
        custom_mnemonic = ZYDIS_MNEMONIC_FELIX86_AND;
        break;
    default: {
        UNREACHABLE();
        break;
    }
    }
    instruction.mnemonic = custom_mnemonic;
    instruction.operand_count = 3;
    instruction.operand_count_visible = 3;
    operands[2] = operands[1];
    operands[1] = operands[0];
    operands[1].reg.value = new_src1;
    operands[1].actions &= ~ZYDIS_OPERAND_ACTION_MASK_WRITE;
}

void Optimizer::pass(u64 start_rip, InstructionVector& instructions, const FlagAccessData& flag_access) {
    struct State {
        int copy_of[16];
        int copy_index[16];
    } state;
    for (int i = 0; i < 16; i++) {
        state.copy_of[i] = -1;
        state.copy_index[i] = -1;
    }
    u64 current_rip = start_rip;
    for (size_t index = 0; index < instructions.size(); index++) {
        auto& [instruction, operands] = instructions[index];
        ZydisMnemonic mnemonic = instruction.mnemonic;
        ZydisMnemonic next_mnemonic = ZYDIS_MNEMONIC_INVALID;
        if (index != instructions.size() - 1) {
            next_mnemonic = instructions[index + 1].first.mnemonic;
        }
        bool is_copy =
            mnemonic == ZYDIS_MNEMONIC_MOV && (is_gpr(operands[0]) && is_gpr(operands[1]) && operands[0].size == 64 && operands[1].size == 64);
        const u64 next_rip = current_rip + instruction.length;
        if (is_copy) {
            const ZydisRegister dst_ref = operands[0].reg.value;
            const ZydisRegister src_ref = operands[1].reg.value;
            ASSERT(dst_ref >= ZYDIS_REGISTER_RAX && dst_ref <= ZYDIS_REGISTER_R15);
            ASSERT(src_ref >= ZYDIS_REGISTER_RAX && src_ref <= ZYDIS_REGISTER_R15);
            state.copy_of[dst_ref - ZYDIS_REGISTER_RAX] = src_ref - ZYDIS_REGISTER_RAX;
            state.copy_index[dst_ref - ZYDIS_REGISTER_RAX] = index;
            for (int i = 0; i < 16; i++) {
                if (state.copy_of[i] == (dst_ref - ZYDIS_REGISTER_RAX)) {
                    state.copy_of[i] = -1;
                    state.copy_index[i] = -1;
                }
            }
        } else {
            switch (mnemonic) {
            case ZYDIS_MNEMONIC_SUB:
            case ZYDIS_MNEMONIC_OR:
            case ZYDIS_MNEMONIC_XOR:
            case ZYDIS_MNEMONIC_AND: {
                if (is_gpr(operands[0]) && is_gpr(operands[1]) && operands[0].size == 64 && operands[1].size == 64) {
                    const ZydisRegister dst_ref = operands[0].reg.value;
                    const ZydisRegister src_ref = operands[1].reg.value;
                    ASSERT(dst_ref >= ZYDIS_REGISTER_RAX && dst_ref <= ZYDIS_REGISTER_R15);
                    ASSERT(src_ref >= ZYDIS_REGISTER_RAX && src_ref <= ZYDIS_REGISTER_R15);
                    if (dst_ref == src_ref) {
                        break;
                    }
                    int dst_index = dst_ref - ZYDIS_REGISTER_RAX;
                    if (state.copy_of[dst_index] != -1) {
                        int copy_index = state.copy_index[dst_index];
                        ASSERT(copy_index != -1);
                        convert_instruction(instructions[index], (ZydisRegister)(ZYDIS_REGISTER_RAX + state.copy_of[dst_index]));
                        instructions[copy_index].first.mnemonic = ZYDIS_MNEMONIC_NOP;
                        state.copy_of[dst_index] = -1;
                        state.copy_index[dst_index] = -1;
                    }
                }
                break;
            }
            case ZYDIS_MNEMONIC_CMP: {
                // Don't fuse the CMP if it has attributes that may be checked during a Recompiler::lea
                if (instruction.attributes & (ZYDIS_ATTRIB_HAS_SEGMENT | ZYDIS_ATTRIB_HAS_ADDRESSSIZE)) {
                    break;
                }
                switch (next_mnemonic) {
                case ZYDIS_MNEMONIC_CMOVL:
                case ZYDIS_MNEMONIC_CMOVNL:
                case ZYDIS_MNEMONIC_CMOVLE:
                case ZYDIS_MNEMONIC_CMOVNLE:
                case ZYDIS_MNEMONIC_CMOVBE:
                case ZYDIS_MNEMONIC_CMOVNBE: {
                    u64 needs_any_flag = flag_access.getFlagsNeeded(next_rip, ALL_CPUFLAGS);
                    if (needs_any_flag == 0) {
                        ZydisMnemonic new_mnemonic;
                        switch (next_mnemonic) {
                        case ZYDIS_MNEMONIC_CMOVL: {
                            new_mnemonic = ZYDIS_MNEMONIC_FELIX86_CMOVL;
                            break;
                        }
                        case ZYDIS_MNEMONIC_CMOVNL: {
                            new_mnemonic = ZYDIS_MNEMONIC_FELIX86_CMOVNL;
                            break;
                        }
                        case ZYDIS_MNEMONIC_CMOVLE: {
                            new_mnemonic = ZYDIS_MNEMONIC_FELIX86_CMOVLE;
                            break;
                        }
                        case ZYDIS_MNEMONIC_CMOVNLE: {
                            new_mnemonic = ZYDIS_MNEMONIC_FELIX86_CMOVNLE;
                            break;
                        }
                        case ZYDIS_MNEMONIC_CMOVBE: {
                            new_mnemonic = ZYDIS_MNEMONIC_FELIX86_CMOVBE;
                            break;
                        }
                        case ZYDIS_MNEMONIC_CMOVNBE: {
                            new_mnemonic = ZYDIS_MNEMONIC_FELIX86_CMOVNBE;
                            break;
                        }
                        default: {
                            __builtin_unreachable();
                        }
                        }

                        instruction.mnemonic = ZYDIS_MNEMONIC_NOP;
                        instructions[index + 1].second[3] = operands[0];
                        instructions[index + 1].second[4] = operands[1];
                        instructions[index + 1].first.mnemonic = new_mnemonic;
                    }
                    break;
                }
                default: {
                    break;
                }
                }
                break;
            }
            default: {
                break;
            }
            }

            for (int i = 0; i < instruction.operand_count; i++) {
                // While we could invalidate only destination registers, we choose to trust the compiler
                // register allocation and invalidate our copy array on source accesses too
                // So for example:
                // mov rax, rbx
                // mov dword[...], rax
                // sub rax, rdi
                // Could elide the mov rax, rbx and replace rax with rbx in the store, but we don't do it
                // Usually compilers will do this themselves when necessary so we don't burden ourselves with trying to do that
                if (is_gpr(operands[i])) {
                    x86_ref_e ref = Recompiler::zydisToRef(operands[i].reg.value);
                    ASSERT(ref >= X86_REF_RAX && ref <= X86_REF_R15);
                    const u8 index = ref - X86_REF_RAX;
                    for (int i = 0; i < 16; i++) {
                        if (state.copy_of[i] == index) {
                            state.copy_of[i] = -1;
                            state.copy_index[i] = -1;
                        }
                    }
                    state.copy_of[index] = -1;
                    state.copy_index[index] = -1;
                }
            }
        }

        current_rip = next_rip;
    }
}
