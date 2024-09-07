#include "felix86/ir/interpreter.h"
#include "felix86/common/log.h"
#include "felix86/common/utility.h"
#include "felix86/ir/print.h"
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

static u64 temps[4096] = {0};
static xmm_reg_t xmm_temps[256] = {0};

ir_block_t* ir_interpret_instruction(ir_block_t* entry, ir_instruction_t* instruction, x86_state_t* state)
{
    switch (instruction->opcode) {
        case IR_NULL: {
            ERROR("Interpreting null, this should not happen\n");
            break;
        }
        case IR_VECTOR_FROM_INTEGER: {
            u64 value = temps[instruction->operands.args[0]->name];
            xmm_reg_t xmm = {0};
            xmm.data[0] = value;
            xmm_temps[instruction->name] = xmm;
            break;
        }
        case IR_INTEGER_FROM_VECTOR: {
            xmm_reg_t xmm = xmm_temps[instruction->operands.args[0]->name];
            temps[instruction->name] = xmm.data[0];
            break;
        }
        case IR_INSERT_INTEGER_TO_VECTOR: {
            xmm_reg_t xmm = xmm_temps[instruction->operands.args[0]->name];
            u32 index = temps[instruction->operands.args[2]->name];
            x86_size_e size = temps[instruction->operands.args[3]->name];
            switch (size) {
                case X86_SIZE_BYTE: {
                    if (index > 15) {
                        ERROR("Invalid index");
                    }

                    u8* data = (u8*)&xmm.data[index];
                    *data = (u8)temps[instruction->operands.args[1]->name];
                    xmm_temps[instruction->name] = xmm;
                    break;
                }
                case X86_SIZE_WORD: {
                    if (index > 7) {
                        ERROR("Invalid index");
                    }

                    u16* data = (u16*)&xmm.data[index];
                    *data = (u16)temps[instruction->operands.args[1]->name];
                    xmm_temps[instruction->name] = xmm;
                    break;
                }
                case X86_SIZE_DWORD: {
                    if (index > 3) {
                        ERROR("Invalid index");
                    }

                    u32* data = (u32*)&xmm.data[index];
                    *data = (u32)temps[instruction->operands.args[1]->name];
                    xmm_temps[instruction->name] = xmm;
                    break;
                }
                case X86_SIZE_QWORD: {
                    if (index > 1) {
                        ERROR("Invalid index");
                    }

                    u64* data = (u64*)&xmm.data[index];
                    *data = temps[instruction->operands.args[1]->name];
                    xmm_temps[instruction->name] = xmm;
                    break;
                }
                default: ERROR("Invalid size"); break;
            }
            break;
        }
        case IR_EXTRACT_INTEGER_FROM_VECTOR: {
            xmm_reg_t xmm = xmm_temps[instruction->operands.args[0]->name];
            u32 index = temps[instruction->operands.args[1]->name];
            x86_size_e size = temps[instruction->operands.args[2]->name];
            switch (size) {
                case X86_SIZE_BYTE: {
                    if (index > 15) {
                        ERROR("Invalid index");
                    }

                    u8* data = (u8*)&xmm.data[index];
                    temps[instruction->name] = *data;
                    break;
                }
                case X86_SIZE_WORD: {
                    if (index > 7) {
                        ERROR("Invalid index");
                    }

                    u16* data = (u16*)&xmm.data[index];
                    temps[instruction->name] = *data;
                    break;
                }
                case X86_SIZE_DWORD: {
                    if (index > 3) {
                        ERROR("Invalid index");
                    }

                    u32* data = (u32*)&xmm.data[index];
                    temps[instruction->name] = *data;
                    break;
                }
                case X86_SIZE_QWORD: {
                    if (index > 1) {
                        ERROR("Invalid index");
                    }

                    u64* data = (u64*)&xmm.data[index];
                    temps[instruction->name] = *data;
                    break;
                }
                default: ERROR("Invalid size"); break;
            }
            break;
        }
        case IR_ADD: {
            temps[instruction->name] = temps[instruction->operands.args[0]->name] + temps[instruction->operands.args[1]->name];
            break;
        }
        case IR_SUB: {
            temps[instruction->name] = temps[instruction->operands.args[0]->name] - temps[instruction->operands.args[1]->name];
            break;
        }
        case IR_AND: {
            temps[instruction->name] = temps[instruction->operands.args[0]->name] & temps[instruction->operands.args[1]->name];
            break;
        }
        case IR_OR: {
            temps[instruction->name] = temps[instruction->operands.args[0]->name] | temps[instruction->operands.args[1]->name];
            break;
        }
        case IR_XOR: {
            temps[instruction->name] = temps[instruction->operands.args[0]->name] ^ temps[instruction->operands.args[1]->name];
            break;
        }
        case IR_SHIFT_LEFT: {
            temps[instruction->name] = temps[instruction->operands.args[0]->name] << temps[instruction->operands.args[1]->name];
            break;
        }
        case IR_SHIFT_RIGHT: {
            temps[instruction->name] = temps[instruction->operands.args[0]->name] >> temps[instruction->operands.args[1]->name];
            break;
        }
        case IR_SHIFT_RIGHT_ARITHMETIC: {
            temps[instruction->name] = (i64)temps[instruction->operands.args[0]->name] >> temps[instruction->operands.args[1]->name];
            break;
        }
        case IR_EQUAL: {
            temps[instruction->name] = temps[instruction->operands.args[0]->name] == temps[instruction->operands.args[1]->name];
            break;
        }
        case IR_NOT_EQUAL: {
            temps[instruction->name] = temps[instruction->operands.args[0]->name] != temps[instruction->operands.args[1]->name];
            break;
        }
        case IR_GREATER_THAN_SIGNED: {
            temps[instruction->name] = (i64)temps[instruction->operands.args[0]->name] > (i64)temps[instruction->operands.args[1]->name];
            break;
        }
        case IR_LESS_THAN_SIGNED: {
            temps[instruction->name] = (i64)temps[instruction->operands.args[0]->name] < (i64)temps[instruction->operands.args[1]->name];
            break;
        }
        case IR_GREATER_THAN_UNSIGNED: {
            temps[instruction->name] = temps[instruction->operands.args[0]->name] > temps[instruction->operands.args[1]->name];
            break;
        }
        case IR_LESS_THAN_UNSIGNED: {
            temps[instruction->name] = temps[instruction->operands.args[0]->name] < temps[instruction->operands.args[1]->name];
            break;
        }
        case IR_LEA: {
            u64 base = temps[instruction->operands.args[0]->name];
            u64 index = temps[instruction->operands.args[1]->name];
            u64 scale = temps[instruction->operands.args[2]->name];
            u64 displacement = temps[instruction->operands.args[3]->name];
            temps[instruction->name] = base + index * scale + displacement;
            break;
        }
        case IR_READ_BYTE: {
            temps[instruction->name] = *(u8*)(temps[instruction->operands.args[0]->name]);
            break;
        }
        case IR_READ_WORD: {
            temps[instruction->name] = *(u16*)(temps[instruction->operands.args[0]->name]);
            break;
        }
        case IR_READ_DWORD: {
            temps[instruction->name] = *(u32*)(temps[instruction->operands.args[0]->name]);
            break;
        }
        case IR_READ_QWORD: {
            temps[instruction->name] = *(u64*)(temps[instruction->operands.args[0]->name]);
            break;
        }
        case IR_READ_XMMWORD: {
            xmm_temps[instruction->name].data[0] = *(u64*)(temps[instruction->operands.args[0]->name]);
            xmm_temps[instruction->name].data[1] = *(u64*)(temps[instruction->operands.args[0]->name] + 8);
            break;
        }
        case IR_WRITE_BYTE: {
            *(u8*)(temps[instruction->operands.args[0]->name]) = temps[instruction->operands.args[1]->name];
            break;
        }
        case IR_WRITE_WORD: {
            *(u16*)(temps[instruction->operands.args[0]->name]) = temps[instruction->operands.args[1]->name];
            break;
        }
        case IR_WRITE_DWORD: {
            *(u32*)(temps[instruction->operands.args[0]->name]) = temps[instruction->operands.args[1]->name];
            break;
        }
        case IR_WRITE_QWORD: {
            *(u64*)(temps[instruction->operands.args[0]->name]) = temps[instruction->operands.args[1]->name];
            break;
        }
        case IR_START_OF_BLOCK: {
            break;
        }
        case IR_SEXT_GPR8: {
            temps[instruction->name] = (i64)(i8)temps[instruction->operands.args[0]->name];
            break;
        }
        case IR_SEXT_GPR16: {
            temps[instruction->name] = (i64)(i16)temps[instruction->operands.args[0]->name];
            break;
        }
        case IR_SEXT_GPR32: {
            temps[instruction->name] = (i64)(i32)temps[instruction->operands.args[0]->name];
            break;
        }
        case IR_POPCOUNT: {
            temps[instruction->name] = __builtin_popcountll(temps[instruction->operands.args[0]->name]);
            break;
        }
        case IR_MOV: {
            WARN("Interpreting MOV, this should not happen");
            temps[instruction->name] = temps[instruction->operands.args[0]->name];
            break;
        }
        case IR_IMMEDIATE: {
            temps[instruction->name] = instruction->load_immediate.immediate;
            break;
        }
        case IR_PHI: {
            ir_phi_node_t* node = instruction->phi.list;
            while (node) {
                if (entry == node->block) {
                    temps[instruction->name] = temps[node->value->name];
                    return NULL;
                }

                node = node->next;
            }
            ERROR("Entry not found while interpreting PHI node");
            break;
        }
        case IR_SYSCALL: {
            u64 opcode = state->gprs[0];
            u64 arg1 = state->gprs[X86_REF_RDI - X86_REF_RAX];
            u64 arg2 = state->gprs[X86_REF_RSI - X86_REF_RAX];
            u64 arg3 = state->gprs[X86_REF_RDX - X86_REF_RAX];
            u64 arg4 = state->gprs[X86_REF_R10 - X86_REF_RAX];
            u64 arg5 = state->gprs[X86_REF_R8 - X86_REF_RAX];
            u64 arg6 = state->gprs[X86_REF_R9 - X86_REF_RAX];
            VERBOSE("Syscall number: %016lx", opcode);
            VERBOSE("Syscall argument 1: %016lx", arg1);
            VERBOSE("Syscall argument 2: %016lx", arg2);
            VERBOSE("Syscall argument 3: %016lx", arg3);
            VERBOSE("Syscall argument 4: %016lx", arg4);
            VERBOSE("Syscall argument 5: %016lx", arg5);
            VERBOSE("Syscall argument 6: %016lx", arg6);
            syscall(opcode, arg1, arg2, arg3, arg4, arg5, arg6);
            break;
        }
        case IR_CPUID: {
            u64 eax = state->gprs[X86_REF_RAX];
            WARN("Interpreting CPUID, unimplemented");
            break;
        }
        case IR_NOT: {
            temps[instruction->name] = ~temps[instruction->operands.args[0]->name];
            break;
        }
        case IR_JUMP: {
            return instruction->jump.target;
        }
        case IR_EXIT: {
            return NULL;
        }
        case IR_JUMP_REGISTER: {
            state->rip = temps[instruction->operands.args[0]->name];
            return NULL;
        }
        case IR_JUMP_CONDITIONAL: {
            if (temps[instruction->jump_conditional.condition->name]) {
                return instruction->jump_conditional.target_true;
            } else {
                return instruction->jump_conditional.target_false;
            }
            break;
        }
        case IR_SET_GUEST: {
            switch (instruction->set_guest.ref) {
                case X86_REF_RAX ... X86_REF_R15: {
                    state->gprs[instruction->set_guest.ref - X86_REF_RAX] = temps[instruction->set_guest.source->name];
                    break;
                }
                case X86_REF_CF: {
                    if (temps[instruction->set_guest.source->name] > 1) {
                        ERROR("Invalid value for CF");
                    }
                    state->cf = temps[instruction->set_guest.source->name];
                    break;
                }
                case X86_REF_PF: {
                    if (temps[instruction->set_guest.source->name] > 1) {
                        ERROR("Invalid value for PF");
                    }
                    state->pf = temps[instruction->set_guest.source->name];
                    break;
                }
                case X86_REF_AF: {
                    if (temps[instruction->set_guest.source->name] > 1) {
                        ERROR("Invalid value for AF");
                    }
                    state->af = temps[instruction->set_guest.source->name];
                    break;
                }
                case X86_REF_ZF: {
                    if (temps[instruction->set_guest.source->name] > 1) {
                        ERROR("Invalid value for ZF");
                    }
                    state->zf = temps[instruction->set_guest.source->name];
                    break;
                }
                case X86_REF_SF: {
                    if (temps[instruction->set_guest.source->name] > 1) {
                        ERROR("Invalid value for SF");
                    }
                    state->sf = temps[instruction->set_guest.source->name];
                    break;
                }
                case X86_REF_OF: {
                    if (temps[instruction->set_guest.source->name] > 1) {
                        ERROR("Invalid value for OF");
                    }
                    state->of = temps[instruction->set_guest.source->name];
                    break;
                }
                case X86_REF_RIP: {
                    WARN("Setting RIP to %016lx", temps[instruction->set_guest.source->name]);
                    state->rip = temps[instruction->set_guest.source->name];
                    break;
                }
                case X86_REF_XMM0 ... X86_REF_XMM15: {
                    state->xmm[instruction->set_guest.ref - X86_REF_XMM0] = xmm_temps[instruction->set_guest.source->name];
                    break;
                }
                default: {
                    ERROR("Invalid reg reference: %d", instruction->set_guest.ref);
                    break;
                }
            }
            break;
        }
        case IR_GET_GUEST: {
            switch (instruction->get_guest.ref) {
                case X86_REF_RAX ... X86_REF_R15: {
                    temps[instruction->name] = state->gprs[instruction->get_guest.ref - X86_REF_RAX];
                    break;
                }
                case X86_REF_CF: {
                    temps[instruction->name] = state->cf;
                    break;
                }
                case X86_REF_PF: {
                    temps[instruction->name] = state->pf;
                    break;
                }
                case X86_REF_AF: {
                    temps[instruction->name] = state->af;
                    break;
                }
                case X86_REF_ZF: {
                    temps[instruction->name] = state->zf;
                    break;
                }
                case X86_REF_SF: {
                    temps[instruction->name] = state->sf;
                    break;
                }
                case X86_REF_OF: {
                    temps[instruction->name] = state->of;
                    break;
                }
                case X86_REF_RIP: {
                    temps[instruction->name] = state->rip;
                    break;
                }
                case X86_REF_FS: {
                    temps[instruction->name] = state->fs;
                    break;
                }
                case X86_REF_GS: {
                    temps[instruction->name] = state->gs;
                    break;
                }
                case X86_REF_XMM0 ... X86_REF_XMM15: {
                    xmm_temps[instruction->name] = state->xmm[instruction->get_guest.ref - X86_REF_XMM0];
                    break;
                }
                default: {
                    ERROR("Invalid reg reference: %d", instruction->get_guest.ref);
                    break;
                }
            }
            break;
        }
        case IR_SELECT: {
            if (temps[instruction->operands.args[0]->name] & ~1) {
                ERROR("Invalid select condition");
            }

            if (temps[instruction->operands.args[0]->name]) {
                temps[instruction->name] = temps[instruction->operands.args[1]->name];
            } else {
                temps[instruction->name] = temps[instruction->operands.args[2]->name];
            }
            break;
        }
        case IR_VECTOR_UNPACK_DWORD_LOW: {
            xmm_reg_t xmm_dest = xmm_temps[instruction->operands.args[0]->name];
            xmm_reg_t xmm_src = xmm_temps[instruction->operands.args[1]->name];
            xmm_reg_t result = {0};
            u64 first = (u32)xmm_dest.data[0] | ((u64)(u32)xmm_src.data[0] << 32);
            u64 second = (xmm_dest.data[0] >> 32) | (xmm_src.data[0] & 0xFFFFFFFF00000000);
            result.data[0] = first;
            result.data[1] = second;
            xmm_temps[instruction->name] = result;
            break;
        }
        case IR_VECTOR_PACKED_AND: {
            xmm_reg_t xmm_dest = xmm_temps[instruction->operands.args[0]->name];
            xmm_reg_t xmm_src = xmm_temps[instruction->operands.args[1]->name];
            xmm_reg_t result = {0};
            result.data[0] = xmm_dest.data[0] & xmm_src.data[0];
            result.data[1] = xmm_dest.data[1] & xmm_src.data[1];
            xmm_temps[instruction->name] = result;
            break;
        }
        case IR_HINT_INPUTS:
        case IR_HINT_OUTPUTS: {
            // Doesn't mean anything to the interpreter, shouldn't exist past optimizations anyway
            break;
        }
        default: {
            ERROR("Invalid opcode: %d", instruction->opcode);
            break;
        }
    }
    return NULL;
}

void ir_interpret_function(ir_function_t* function, x86_state_t* state) {
    memset(temps, 0, sizeof(temps));
    ir_block_list_t* blocks = function->first;
    ir_instruction_list_t* current = blocks->block->instructions;
    ir_block_t* next;
    ir_block_t* entry = NULL;
    ir_block_t* entry_next = blocks->block;
    while (current) {
        next = ir_interpret_instruction(entry, &current->instruction, state);
        if (next) {
            entry = entry_next;
            entry_next = next;
            current = next->instructions;
        } else {
            current = current->next;
        }
    }
}