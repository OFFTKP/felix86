#include <unordered_map>
#include <string.h>
#include <unistd.h>
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/print.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/hle/cpuid.hpp"
#include "felix86/hle/syscall.hpp"
#include "felix86/ir/interpreter.hpp"
#include "felix86/ir/print.hpp"

static std::unordered_map<u32, u64> temps;
static std::unordered_map<u32, xmm_reg_t> xmm_temps;

ir_block_t* ir_interpret_instruction(felix86_recompiler_t* recompiler, ir_block_t* entry,
                                     ir_instruction_t* instruction, x86_thread_state_t* state) {
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
        x86_size_e size = (x86_size_e)temps[instruction->operands.args[3]->name];
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
        default:
            ERROR("Invalid size");
            break;
        }
        break;
    }
    case IR_EXTRACT_INTEGER_FROM_VECTOR: {
        xmm_reg_t xmm = xmm_temps[instruction->operands.args[0]->name];
        u32 index = temps[instruction->operands.args[1]->name];
        x86_size_e size = (x86_size_e)temps[instruction->operands.args[2]->name];
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
        default:
            ERROR("Invalid size");
            break;
        }
        break;
    }
    case IR_ADD: {
        temps[instruction->name] =
            temps[instruction->operands.args[0]->name] + temps[instruction->operands.args[1]->name];
        break;
    }
    case IR_SUB: {
        temps[instruction->name] =
            temps[instruction->operands.args[0]->name] - temps[instruction->operands.args[1]->name];
        break;
    }
    case IR_AND: {
        temps[instruction->name] =
            temps[instruction->operands.args[0]->name] & temps[instruction->operands.args[1]->name];
        break;
    }
    case IR_OR: {
        temps[instruction->name] =
            temps[instruction->operands.args[0]->name] | temps[instruction->operands.args[1]->name];
        break;
    }
    case IR_XOR: {
        temps[instruction->name] =
            temps[instruction->operands.args[0]->name] ^ temps[instruction->operands.args[1]->name];
        break;
    }
    case IR_SHIFT_LEFT: {
        temps[instruction->name] = temps[instruction->operands.args[0]->name]
                                   << temps[instruction->operands.args[1]->name];
        break;
    }
    case IR_SHIFT_RIGHT: {
        temps[instruction->name] = temps[instruction->operands.args[0]->name] >>
                                   temps[instruction->operands.args[1]->name];
        break;
    }
    case IR_SHIFT_RIGHT_ARITHMETIC: {
        temps[instruction->name] = (i64)temps[instruction->operands.args[0]->name] >>
                                   temps[instruction->operands.args[1]->name];
        break;
    }
    case IR_EQUAL: {
        temps[instruction->name] = temps[instruction->operands.args[0]->name] ==
                                   temps[instruction->operands.args[1]->name];
        break;
    }
    case IR_NOT_EQUAL: {
        temps[instruction->name] = temps[instruction->operands.args[0]->name] !=
                                   temps[instruction->operands.args[1]->name];
        break;
    }
    case IR_GREATER_THAN_SIGNED: {
        temps[instruction->name] = (i64)temps[instruction->operands.args[0]->name] >
                                   (i64)temps[instruction->operands.args[1]->name];
        break;
    }
    case IR_LESS_THAN_SIGNED: {
        temps[instruction->name] = (i64)temps[instruction->operands.args[0]->name] <
                                   (i64)temps[instruction->operands.args[1]->name];
        break;
    }
    case IR_GREATER_THAN_UNSIGNED: {
        temps[instruction->name] =
            temps[instruction->operands.args[0]->name] > temps[instruction->operands.args[1]->name];
        break;
    }
    case IR_LESS_THAN_UNSIGNED: {
        temps[instruction->name] =
            temps[instruction->operands.args[0]->name] < temps[instruction->operands.args[1]->name];
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
        xmm_temps[instruction->name].data[1] =
            *(u64*)(temps[instruction->operands.args[0]->name] + 8);
        break;
    }
    case IR_WRITE_BYTE: {
        *(u8*)(temps[instruction->operands.args[0]->name]) =
            temps[instruction->operands.args[1]->name];
        break;
    }
    case IR_WRITE_WORD: {
        *(u16*)(temps[instruction->operands.args[0]->name]) =
            temps[instruction->operands.args[1]->name];
        break;
    }
    case IR_WRITE_DWORD: {
        *(u32*)(temps[instruction->operands.args[0]->name]) =
            temps[instruction->operands.args[1]->name];
        break;
    }
    case IR_WRITE_QWORD: {
        *(u64*)(temps[instruction->operands.args[0]->name]) =
            temps[instruction->operands.args[1]->name];
        break;
    }
    case IR_WRITE_XMMWORD: {
        *(u64*)(temps[instruction->operands.args[0]->name]) =
            xmm_temps[instruction->operands.args[1]->name].data[0];
        *(u64*)(temps[instruction->operands.args[0]->name] + 8) =
            xmm_temps[instruction->operands.args[1]->name].data[1];
        break;
    }
    case IR_START_OF_BLOCK: {
        break;
    }
    case IR_SEXT8: {
        temps[instruction->name] = (i64)(i8)temps[instruction->operands.args[0]->name];
        break;
    }
    case IR_SEXT16: {
        temps[instruction->name] = (i64)(i16)temps[instruction->operands.args[0]->name];
        break;
    }
    case IR_SEXT32: {
        temps[instruction->name] = (i64)(i32)temps[instruction->operands.args[0]->name];
        break;
    }
    case IR_POPCOUNT: {
        temps[instruction->name] = __builtin_popcountll(temps[instruction->operands.args[0]->name]);
        break;
    }
    case IR_MOV: {
        ERROR("Interpreting MOV, this should not happen");
        temps[instruction->name] = temps[instruction->operands.args[0]->name];
        break;
    }
    case IR_IMMEDIATE: {
        temps[instruction->name] = instruction->load_immediate.immediate;
        break;
    }
    case IR_PHI: {
        ERROR("Interpreting PHI, this should not happen");
        break;
    }
    case IR_SYSCALL: {
        felix86_syscall(recompiler, state);
        break;
    }
    case IR_CPUID: {
        felix86_cpuid(state);
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
    case IR_RDTSC: {
        u64 tsc = 0;
        __asm__ __volatile__("rdtsc" : "=A"(tsc));
        temps[instruction->name] = tsc;
        WARN("Interpreting RDTSC");
        break;
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
            state->gprs[instruction->set_guest.ref - X86_REF_RAX] =
                temps[instruction->set_guest.source->name];
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
            state->xmm[instruction->set_guest.ref - X86_REF_XMM0] =
                xmm_temps[instruction->set_guest.source->name];
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
            temps[instruction->name] = state->fsbase;
            break;
        }
        case X86_REF_GS: {
            temps[instruction->name] = state->gsbase;
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

    case IR_VECTOR_UNPACK_BYTE_LOW: {
        xmm_reg_t xmm_dest = xmm_temps[instruction->operands.args[0]->name];
        xmm_reg_t xmm_src = xmm_temps[instruction->operands.args[1]->name];
        xmm_reg_t result = {0};
        u8* src1 = (u8*)&xmm_dest.data[0];
        u8* src2 = (u8*)&xmm_src.data[0];
        u8* dest = (u8*)&result.data[0];
        for (int i = 0; i < 128; i += 8) {
            int j = i >> 3;
            dest[j] = j & 1 ? src2[j >> 1] : src1[j >> 1];
        }
        xmm_temps[instruction->name] = result;
        break;
    }
    case IR_VECTOR_UNPACK_WORD_LOW: {
        xmm_reg_t xmm_dest = xmm_temps[instruction->operands.args[0]->name];
        xmm_reg_t xmm_src = xmm_temps[instruction->operands.args[1]->name];
        xmm_reg_t result = {0};
        u16* src1 = (u16*)&xmm_dest.data[0];
        u16* src2 = (u16*)&xmm_src.data[0];
        u16* dest = (u16*)&result.data[0];
        for (int i = 0; i < 128; i += 16) {
            int j = i >> 4;
            dest[j] = j & 1 ? src2[j >> 1] : src1[j >> 1];
        }
        xmm_temps[instruction->name] = result;
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
    case IR_VECTOR_UNPACK_QWORD_LOW: {
        xmm_reg_t xmm_dest = xmm_temps[instruction->operands.args[0]->name];
        xmm_reg_t xmm_src = xmm_temps[instruction->operands.args[1]->name];
        xmm_reg_t result = {0};
        result.data[0] = xmm_dest.data[0];
        result.data[1] = xmm_src.data[0];
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
    case IR_VECTOR_PACKED_XOR: {
        xmm_reg_t xmm_dest = xmm_temps[instruction->operands.args[0]->name];
        xmm_reg_t xmm_src = xmm_temps[instruction->operands.args[1]->name];
        xmm_reg_t result = {0};
        result.data[0] = xmm_dest.data[0] ^ xmm_src.data[0];
        result.data[1] = xmm_dest.data[1] ^ xmm_src.data[1];
        xmm_temps[instruction->name] = result;
        break;
    }
    case IR_VECTOR_PACKED_SHUFFLE_DWORD: {
        u8 control_byte = instruction->control_byte;
        xmm_reg_t xmm_src = xmm_temps[instruction->operands.args[0]->name];
        xmm_reg_t result = {0};
        u32* src = (u32*)&xmm_src.data[0];
        u32* dest = (u32*)&result.data[0];
        for (int i = 0; i < 4; i++) {
            dest[i] = src[control_byte & 3];
            control_byte >>= 2;
        }
        xmm_temps[instruction->name] = result;
        break;
    }
    case IR_IDIV8: {
        i16 dividend = (u16)state->gprs[X86_REF_RAX];
        i16 divisor = temps[instruction->operands.args[0]->name];
        if (divisor == 0) {
            ERROR("Division by zero");
        }

        i16 quotient = dividend / divisor;
        if (quotient > 0x7F || quotient < -0x80) {
            ERROR("Quotient overflow");
        }

        u16 remainder = dividend % divisor;
        state->gprs[X86_REF_RAX] &= ~0xFFFF;
        state->gprs[X86_REF_RAX] |= quotient & 0xFF;
        state->gprs[X86_REF_RAX] |= (remainder & 0xFF) << 8;
        break;
    }
    case IR_IDIV16: {
        i32 dividend = (u32)(u16)state->gprs[X86_REF_RDX] << 16 | (u16)state->gprs[X86_REF_RAX];
        i32 divisor = temps[instruction->operands.args[0]->name];
        if (divisor == 0) {
            ERROR("Division by zero");
        }

        i32 quotient = dividend / divisor;
        if (quotient > 0x7FFF || quotient < -0x8000) {
            ERROR("Quotient overflow");
        }

        u32 remainder = dividend % divisor;

        state->gprs[X86_REF_RAX] &= ~0xFFFF;
        state->gprs[X86_REF_RDX] &= ~0xFFFF;
        state->gprs[X86_REF_RAX] |= quotient & 0xFFFF;
        state->gprs[X86_REF_RDX] |= remainder & 0xFFFF;
        break;
    }
    case IR_IDIV32: {
        i64 dividend = (u64)(u32)state->gprs[X86_REF_RDX] << 32 | (u32)state->gprs[X86_REF_RAX];
        i64 divisor = temps[instruction->operands.args[0]->name];
        if (divisor == 0) {
            ERROR("Division by zero");
        }

        i64 quotient = dividend / divisor;
        if (quotient > 0x7FFFFFFFll || quotient < -0x80000000ll) {
            ERROR("Quotient overflow");
        }

        u64 remainder = dividend % divisor;
        state->gprs[X86_REF_RAX] = quotient;
        state->gprs[X86_REF_RDX] = remainder;
        break;
    }
    case IR_IDIV64: {
        __int128_t dividend =
            ((__int128_t)(state->gprs[X86_REF_RDX]) << 64) | state->gprs[X86_REF_RAX];
        __int128_t divisor = temps[instruction->operands.args[0]->name];
        if (divisor == 0) {
            ERROR("Division by zero");
        }

        __int128_t quotient = dividend / divisor;
        __int128_t remainder = dividend % divisor;
        state->gprs[X86_REF_RAX] = quotient;
        state->gprs[X86_REF_RDX] = remainder;
        break;
    }
    case IR_UDIV8: {
        u16 dividend = (u16)state->gprs[X86_REF_RAX];
        u16 divisor = temps[instruction->operands.args[0]->name];
        if (divisor == 0) {
            ERROR("Division by zero");
        }

        u16 quotient = dividend / divisor;
        u16 remainder = dividend % divisor;
        state->gprs[X86_REF_RAX] &= ~0xFFFF;
        state->gprs[X86_REF_RAX] |= quotient & 0xFF;
        state->gprs[X86_REF_RAX] |= (remainder & 0xFF) << 8;
        break;
    }
    case IR_UDIV16: {
        u32 dividend = (u32)(u16)state->gprs[X86_REF_RDX] << 16 | (u16)state->gprs[X86_REF_RAX];
        u32 divisor = temps[instruction->operands.args[0]->name];
        if (divisor == 0) {
            ERROR("Division by zero");
        }

        u32 quotient = dividend / divisor;
        u32 remainder = dividend % divisor;
        if (quotient > 0xFFFF) {
            ERROR("Quotient overflow");
        }

        state->gprs[X86_REF_RAX] &= ~0xFFFF;
        state->gprs[X86_REF_RDX] &= ~0xFFFF;
        state->gprs[X86_REF_RAX] |= quotient & 0xFFFF;
        state->gprs[X86_REF_RDX] |= remainder & 0xFFFF;
        break;
    }
    case IR_UDIV32: {
        u64 dividend = (u64)(u32)state->gprs[X86_REF_RDX] << 32 | (u32)state->gprs[X86_REF_RAX];
        u64 divisor = temps[instruction->operands.args[0]->name];
        if (divisor == 0) {
            ERROR("Division by zero");
        }

        u64 quotient = dividend / divisor;
        u64 remainder = dividend % divisor;
        if (quotient > 0xFFFFFFFF) {
            ERROR("Quotient overflow");
        }

        state->gprs[X86_REF_RAX] = quotient;
        state->gprs[X86_REF_RDX] = remainder;
        break;
    }
    case IR_UDIV64: {
        __uint128_t dividend =
            ((__uint128_t)(state->gprs[X86_REF_RDX]) << 64) | state->gprs[X86_REF_RAX];
        __uint128_t divisor = temps[instruction->operands.args[0]->name];
        if (divisor == 0) {
            ERROR("Division by zero");
        }

        __uint128_t quotient = dividend / divisor;
        __uint128_t remainder = dividend % divisor;
        if (quotient > 0xFFFFFFFFFFFFFFFF) {
            ERROR("Quotient overflow");
        }

        state->gprs[X86_REF_RAX] = quotient;
        state->gprs[X86_REF_RDX] = remainder;
        break;
    }
    case IR_CLZ: {
        temps[instruction->name] = __builtin_clzll(temps[instruction->operands.args[0]->name]);
        break;
    }
    case IR_CTZ: {
        temps[instruction->name] = __builtin_ctzll(temps[instruction->operands.args[0]->name]);
        break;
    }
    case IR_IMUL: {
        temps[instruction->name] = (i64)temps[instruction->operands.args[0]->name] *
                                   (i64)temps[instruction->operands.args[1]->name];
        break;
    }
    case IR_VECTOR_PACKED_SHIFT_RIGHT: {
        xmm_reg_t xmm = xmm_temps[instruction->operands.args[0]->name];
        u64 count = temps[instruction->operands.args[1]->name];
        __uint128_t data = ((__uint128_t)xmm.data[1] << 64) | xmm.data[0];
        data >>= count;
        xmm.data[0] = data & 0xFFFFFFFFFFFFFFFF;
        xmm.data[1] = data >> 64;
        xmm_temps[instruction->name] = xmm;
        break;
    }
    case IR_VECTOR_PACKED_SHIFT_LEFT: {
        xmm_reg_t xmm = xmm_temps[instruction->operands.args[0]->name];
        u64 count = temps[instruction->operands.args[1]->name];
        __uint128_t data = ((__uint128_t)xmm.data[1] << 64) | xmm.data[0];
        data <<= count;
        xmm.data[0] = data & 0xFFFFFFFFFFFFFFFF;
        xmm.data[1] = data >> 64;
        xmm_temps[instruction->name] = xmm;
        break;
    }
    case IR_VECTOR_PACKED_ADD_QWORD: {
        xmm_reg_t xmm_dest = xmm_temps[instruction->operands.args[0]->name];
        xmm_reg_t xmm_src = xmm_temps[instruction->operands.args[1]->name];
        xmm_reg_t result = {0};
        result.data[0] = xmm_dest.data[0] + xmm_src.data[0];
        result.data[1] = xmm_dest.data[1] + xmm_src.data[1];
        xmm_temps[instruction->name] = result;
        break;
    }
    case IR_VECTOR_PACKED_COMPARE_EQ_BYTE: {
        xmm_reg_t xmm_dest = xmm_temps[instruction->operands.args[0]->name];
        xmm_reg_t xmm_src = xmm_temps[instruction->operands.args[1]->name];
        xmm_reg_t result = {0};
        u8* result_dest = (u8*)&result.data[0];
        for (u32 i = 0; i < 16; i++) {
            if (((u8*)&xmm_dest.data[0])[i] == ((u8*)&xmm_src.data[0])[i]) {
                result_dest[i] = 0xFF;
            }
        }
        xmm_temps[instruction->name] = result;
        break;
    }
    case IR_VECTOR_PACKED_COMPARE_EQ_WORD: {
        xmm_reg_t xmm_dest = xmm_temps[instruction->operands.args[0]->name];
        xmm_reg_t xmm_src = xmm_temps[instruction->operands.args[1]->name];
        xmm_reg_t result = {0};
        u16* result_dest = (u16*)&result.data[0];
        for (u32 i = 0; i < 8; i++) {
            if (((u16*)&xmm_dest.data[0])[i] == ((u16*)&xmm_src.data[0])[i]) {
                result_dest[i] = 0xFFFF;
            }
        }
        xmm_temps[instruction->name] = result;
        break;
    }
    case IR_VECTOR_PACKED_COMPARE_EQ_DWORD: {
        xmm_reg_t xmm_dest = xmm_temps[instruction->operands.args[0]->name];
        xmm_reg_t xmm_src = xmm_temps[instruction->operands.args[1]->name];
        xmm_reg_t result = {0};
        u32* result_dest = (u32*)&result.data[0];
        for (u32 i = 0; i < 4; i++) {
            if (((u32*)&xmm_dest.data[0])[i] == ((u32*)&xmm_src.data[0])[i]) {
                result_dest[i] = 0xFFFFFFFF;
            }
        }
        xmm_temps[instruction->name] = result;
        break;
    }
    case IR_VECTOR_PACKED_MOVE_BYTE_MASK: {
        xmm_reg_t xmm = xmm_temps[instruction->operands.args[0]->name];
        u64 mask = 0;
        u8* data = (u8*)&xmm.data[0];
        for (u32 i = 0; i < 16; i++) {
            mask |= (data[i] >> 7) << (i * 8);
        }
        temps[instruction->name] = mask;
        break;
    }
    case IR_COMMENT: {
        WARN("Runtime comment: %s", instruction->runtime_comment.comment);
        fflush(stdout);
        // print_state(state);
        break;
    }
    case IR_LEFT_ROTATE64: {
        u64 value = temps[instruction->operands.args[0]->name];
        u64 count = temps[instruction->operands.args[1]->name];
        temps[instruction->name] = (value << count) | (value >> (64 - count));
        break;
    }
    case IR_VECTOR_PACKED_SUB_BYTE: {
        xmm_reg_t xmm_dest = xmm_temps[instruction->operands.args[0]->name];
        xmm_reg_t xmm_src = xmm_temps[instruction->operands.args[1]->name];
        xmm_reg_t result = {0};
        u8* dest = (u8*)&result.data[0];
        u8* src0 = (u8*)&xmm_dest.data[0];
        u8* src1 = (u8*)&xmm_src.data[0];
        for (u32 i = 0; i < 16; i++) {
            dest[i] = src0[i] - src1[i];
        }
        xmm_temps[instruction->name] = result;
        break;
    }
    case IR_HINT_INPUTS:
    case IR_HINT_OUTPUTS: {
        // Doesn't mean anything to the interpreter, shouldn't exist past
        // optimizations anyway
        break;
    }
    default: {
        ERROR("Invalid opcode: %d", instruction->opcode);
        break;
    }
    }
    return NULL;
}

extern "C" void ir_interpret_function(felix86_recompiler_t* recompiler, ir_function_t* function,
                                      x86_thread_state_t* state) {
    temps.clear();
    xmm_temps.clear();

    ir_block_list_t* blocks = function->entry->successors;
    ir_instruction_list_t* current = blocks->block->instructions;
    ir_block_t* next;
    ir_block_t* entry = NULL;
    ir_block_t* entry_next = blocks->block;
    while (current) {
        next = ir_interpret_instruction(recompiler, entry, &current->instruction, state);
        if (next) {
            entry = entry_next;
            entry_next = next;
            current = next->instructions;
        } else {
            current = current->next;
        }
    }
}