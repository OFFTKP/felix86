#include <Zydis/Zydis.h>
#include "felix86/common/state.hpp"
#include "felix86/v2/recompiler.hpp"

#define INTR_HANDLE(name)                                                                                                                            \
    void interpret_##name(Recompiler& rec, ThreadState* state, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands)

#define RUN(name) interpret_##name(rec, state, instruction, operands)

u64 GetEffectiveAddress(ThreadState* state, ZydisDecodedOperand& operand) {
    u64 base_full = 0;
    u64 index_full = 0;
    u8 scale = operand.mem.scale;
    u64 disp = operand.mem.disp.value;

    if (operand.mem.base != ZYDIS_REGISTER_NONE) {
        x86_ref_e base = Recompiler::zydisToRef(operand.mem.base);
        int base_index = base - X86_REF_RAX;
        base_full = state->gprs[base_index];
    }

    if (operand.mem.index != ZYDIS_REGISTER_NONE) {
        x86_ref_e index = Recompiler::zydisToRef(operand.mem.index);
        int index_index = index - X86_REF_RAX;
        index_full = state->gprs[index_index];
    }

    u64 effective_address = base_full + index_full * scale + disp;

    if (operand.mem.segment == ZYDIS_REGISTER_FS) {
        effective_address += state->fsbase;
    } else if (operand.mem.segment == ZYDIS_REGISTER_GS) {
        effective_address += state->gsbase;
    }

    if ((operand.attributes & ZYDIS_ATTRIB_HAS_ADDRESSSIZE) || g_mode32) {
        if (!g_mode32) {
            WARN("Address size override prefix in 64-bit mode?");
        }
        effective_address &= 0xFFFF'FFFF;
    }

    effective_address += g_address_space_base;

    return effective_address;
}

u64 GetOperand(ThreadState* state, ZydisDecodedOperand& operand) {
    switch (operand.type) {
    case ZYDIS_OPERAND_TYPE_REGISTER: {
        x86_ref_e ref = Recompiler::zydisToRef(operand.reg.value);
        x86_size_e size = Recompiler::zydisToSize(operand.size);
        int index = ref - X86_REF_RAX;
        u64 full = state->gprs[index];
        if (size == X86_SIZE_BYTE_HIGH) {
            full >>= 8;
        }
        if (operand.size != 64) {
            full &= 1ull << (operand.size - 1);
        }
        return full;
    }
    case ZYDIS_OPERAND_TYPE_MEMORY: {
        u64 effective_address = GetEffectiveAddress(state, operand);

        u64 data;
        memcpy(&data, (void*)effective_address, operand.size / 8);

        return data;
    }
    case ZYDIS_OPERAND_TYPE_IMMEDIATE: {
        return operand.imm.value.u;
    }
    default: {
        UNREACHABLE();
        return 0;
    }
    }
}

void SetOperand(ThreadState* state, ZydisDecodedOperand& operand, u64 value) {
    switch (operand.type) {
    case ZYDIS_OPERAND_TYPE_REGISTER: {
        x86_ref_e ref = Recompiler::zydisToRef(operand.reg.value);
        x86_size_e size = Recompiler::zydisToSize(operand.size);
        int index = ref - X86_REF_RAX;
        u64 previous = state->gprs[index];
        switch (operand.size) {
        case 64: {
            state->gprs[index] = value;
            break;
        }
        case 32: {
            state->gprs[index] = value & 0xFFFF'FFFF;
            break;
        }
        case 16: {
            state->gprs[index] = (previous & ~0xFFFF) | (value & 0xFFFF);
            break;
        }
        case 8: {
            if (size == X86_SIZE_BYTE_HIGH) {
                state->gprs[index] = (previous & ~0xFF00) | ((value & 0xFF) << 8);
            } else {
                state->gprs[index] = (previous & ~0xFF) | (value & 0xFF);
            }
            break;
        }
        default: {
            UNREACHABLE();
            return;
        }
        }
        break;
    }
    case ZYDIS_OPERAND_TYPE_MEMORY: {
        u64 effective_address = GetEffectiveAddress(state, operand);

        memcpy((void*)effective_address, &value, operand.size / 8);

        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }
}

u64 Zext(u64 value, u8 size) {
    switch (size) {
    case 8:
        return value & 0xFF;
    case 16:
        return value & 0xFFFF;
    case 32:
        return value & 0xFFFF'FFFF;
    case 64:
        return value;
    default:
        UNREACHABLE();
        return 0;
    }
}

void ClearCF(ThreadState* state) {
    state->cf = 0;
}

void ClearOF(ThreadState* state) {
    state->of = 0;
}

void SetZero(ThreadState* state, u64 value, u8 size) {
    state->zf = Zext(value, size) == 0;
}

void SetParity(ThreadState* state, u64 value) {
    static bool bitcount[] = {
        1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0,
        1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1,
        1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0,
        1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0,
        1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0,
        1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    };
    u8 parity = value & 0xFF;
    state->pf = bitcount[parity];
}

void SetSign(ThreadState* state, u64 value, u8 size) {
    state->sf = (value >> (size - 1)) & 1;
}

void SetOverflowAdd(ThreadState* state, u64 lhs, u64 rhs, u64 result, u8 size) {
    u64 data = ((lhs & rhs) | ((~result) & (lhs | rhs)));
    bool b1 = (data >> (size - 1)) & 1;
    bool b2 = (data >> (size - 2)) & 1;
    state->of = b1 ^ b2;
}

void SetAuxiliaryAdd(ThreadState* state, u64 lhs, u64 result) {
    state->af = (result & 0xF) < (lhs & 0xF);
}

void SetCarryAdd(ThreadState* state, u64 lhs, u64 result, u8 size) {
    u64 zext = Zext(result, size);
    state->cf = zext < lhs;
}

void SetOverflowSub(ThreadState* state, u64 lhs, u64 rhs, u64 result, u8 size) {
    u64 data = (result & (~lhs | rhs)) | (~lhs & rhs);
    bool b1 = (data >> (size - 1)) & 1;
    bool b2 = (data >> (size - 2)) & 1;
    state->of = b1 ^ b2;
}

void SetAuxiliarySub(ThreadState* state, u64 lhs, u64 rhs) {
    state->af = (lhs & 0xF) < (rhs & 0xF);
}

void SetCarrySub(ThreadState* state, u64 lhs, u64 rhs) {
    state->cf = lhs < rhs;
}

u8* GetVector(ThreadState* state, ZydisDecodedOperand& operand) {
    switch (operand.type) {
    case ZYDIS_OPERAND_TYPE_REGISTER: {
        x86_ref_e ref = Recompiler::zydisToRef(operand.reg.value);
        return (u8*)&state->xmm[ref - X86_REF_XMM0].data[0];
    }
    case ZYDIS_OPERAND_TYPE_MEMORY: {
        u64 effective_address = GetEffectiveAddress(state, operand);
        return (u8*)effective_address;
    }
    default: {
        UNREACHABLE();
        return nullptr;
    }
    }
}

template <class Type>
void ElementOperation(ThreadState* state, ZydisDecodedOperand* operands, void (*func)(Type* lhs, Type* rhs)) {
    u8* dst = GetVector(state, operands[0]);
    u8* src = GetVector(state, operands[1]);
    func((Type*)dst, (Type*)src);
}

INTR_HANDLE(ADD) {
    u64 op1 = GetOperand(state, operands[0]);
    u64 op2 = GetOperand(state, operands[1]);
    u64 result = op1 + op2;
    SetOperand(state, operands[0], result);

    u8 size = operands[0].size;
    SetZero(state, result, size);
    SetParity(state, result);
    SetSign(state, result, size);
    SetCarryAdd(state, op1, result, size);
    SetOverflowAdd(state, op1, op2, result, size);
    SetAuxiliaryAdd(state, op1, result);
}

INTR_HANDLE(SUB) {
    u64 op1 = GetOperand(state, operands[0]);
    u64 op2 = GetOperand(state, operands[1]);
    u64 result = op1 - op2;
    SetOperand(state, operands[0], result);

    u8 size = operands[0].size;
    SetZero(state, result, size);
    SetParity(state, result);
    SetSign(state, result, size);
    SetCarrySub(state, op1, op2);
    SetOverflowSub(state, op1, op2, result, size);
    SetAuxiliarySub(state, op1, op2);
}

INTR_HANDLE(CMP) {
    u64 op1 = GetOperand(state, operands[0]);
    u64 op2 = GetOperand(state, operands[1]);
    u64 result = op1 - op2;

    u8 size = operands[0].size;
    SetZero(state, result, size);
    SetParity(state, result);
    SetSign(state, result, size);
    SetCarrySub(state, op1, op2);
    SetOverflowSub(state, op1, op2, result, size);
    SetAuxiliarySub(state, op1, op2);
}

INTR_HANDLE(OR) {
    u64 op1 = GetOperand(state, operands[0]);
    u64 op2 = GetOperand(state, operands[1]);
    u64 result = op1 | op2;
    SetOperand(state, operands[0], result);

    u8 size = operands[0].size;
    ClearCF(state);
    ClearOF(state);
    SetZero(state, result, size);
    SetParity(state, result);
    SetSign(state, result, size);
}

INTR_HANDLE(XOR) {
    u64 op1 = GetOperand(state, operands[0]);
    u64 op2 = GetOperand(state, operands[1]);
    u64 result = op1 ^ op2;
    SetOperand(state, operands[0], result);

    u8 size = operands[0].size;
    ClearCF(state);
    ClearOF(state);
    SetZero(state, result, size);
    SetParity(state, result);
    SetSign(state, result, size);
}

INTR_HANDLE(AND) {
    u64 op1 = GetOperand(state, operands[0]);
    u64 op2 = GetOperand(state, operands[1]);
    u64 result = op1 & op2;
    SetOperand(state, operands[0], result);

    u8 size = operands[0].size;
    ClearCF(state);
    ClearOF(state);
    SetZero(state, result, size);
    SetParity(state, result);
    SetSign(state, result, size);
}

INTR_HANDLE(TEST) {
    u64 op1 = GetOperand(state, operands[0]);
    u64 op2 = GetOperand(state, operands[1]);
    u64 result = op1 & op2;

    u8 size = operands[0].size;
    ClearCF(state);
    ClearOF(state);
    SetZero(state, result, size);
    SetParity(state, result);
    SetSign(state, result, size);
}

INTR_HANDLE(HLT) {
    state->exit_reason = EXIT_REASON_HLT;
}

INTR_HANDLE(PAND) {
    ElementOperation<u64>(state, operands, [](u64* lhs, u64* rhs) {
        lhs[0] &= rhs[0];
        lhs[1] &= rhs[1];
    });
}

INTR_HANDLE(ANDPS) {
    RUN(PAND);
}

INTR_HANDLE(ANDPD) {
    RUN(PAND);
}

INTR_HANDLE(PANDN) {
    ElementOperation<u64>(state, operands, [](u64* lhs, u64* rhs) {
        lhs[0] = ~lhs[0] & rhs[0];
        lhs[1] = ~lhs[1] & rhs[1];
    });
}

INTR_HANDLE(ANDNPS) {
    RUN(PANDN);
}

INTR_HANDLE(ANDNPD) {
    RUN(PANDN);
}

INTR_HANDLE(POR) {
    ElementOperation<u64>(state, operands, [](u64* lhs, u64* rhs) {
        lhs[0] |= rhs[0];
        lhs[1] |= rhs[1];
    });
}

INTR_HANDLE(ORPS) {
    RUN(POR);
}

INTR_HANDLE(ORPD) {
    RUN(POR);
}

INTR_HANDLE(PXOR) {
    ElementOperation<u64>(state, operands, [](u64* lhs, u64* rhs) {
        lhs[0] ^= rhs[0];
        lhs[1] ^= rhs[1];
    });
}

INTR_HANDLE(XORPS) {
    RUN(PXOR);
}

INTR_HANDLE(XORPD) {
    RUN(PXOR);
}

INTR_HANDLE(PADDB) {
    ElementOperation<u8>(state, operands, [](u8* lhs, u8* rhs) {
        lhs[0] += rhs[0];
        lhs[1] += rhs[1];
        lhs[2] += rhs[2];
        lhs[3] += rhs[3];
        lhs[4] += rhs[4];
        lhs[5] += rhs[5];
        lhs[6] += rhs[6];
        lhs[7] += rhs[7];
        lhs[8] += rhs[8];
        lhs[9] += rhs[9];
        lhs[10] += rhs[10];
        lhs[11] += rhs[11];
        lhs[12] += rhs[12];
        lhs[13] += rhs[13];
        lhs[14] += rhs[14];
        lhs[15] += rhs[15];
    });
}

INTR_HANDLE(PADDW) {
    ElementOperation<u16>(state, operands, [](u16* lhs, u16* rhs) {
        lhs[0] += rhs[0];
        lhs[1] += rhs[1];
        lhs[2] += rhs[2];
        lhs[3] += rhs[3];
        lhs[4] += rhs[4];
        lhs[5] += rhs[5];
        lhs[6] += rhs[6];
        lhs[7] += rhs[7];
    });
}

INTR_HANDLE(PADDD) {
    ElementOperation<u32>(state, operands, [](u32* lhs, u32* rhs) {
        lhs[0] += rhs[0];
        lhs[1] += rhs[1];
        lhs[2] += rhs[2];
        lhs[3] += rhs[3];
    });
}

INTR_HANDLE(PADDQ) {
    ElementOperation<u64>(state, operands, [](u64* lhs, u64* rhs) {
        lhs[0] += rhs[0];
        lhs[1] += rhs[1];
    });
}

INTR_HANDLE(PSUBB) {
    ElementOperation<u8>(state, operands, [](u8* lhs, u8* rhs) {
        lhs[0] -= rhs[0];
        lhs[1] -= rhs[1];
        lhs[2] -= rhs[2];
        lhs[3] -= rhs[3];
        lhs[4] -= rhs[4];
        lhs[5] -= rhs[5];
        lhs[6] -= rhs[6];
        lhs[7] -= rhs[7];
        lhs[8] -= rhs[8];
        lhs[9] -= rhs[9];
        lhs[10] -= rhs[10];
        lhs[11] -= rhs[11];
        lhs[12] -= rhs[12];
        lhs[13] -= rhs[13];
        lhs[14] -= rhs[14];
        lhs[15] -= rhs[15];
    });
}

INTR_HANDLE(PSUBW) {
    ElementOperation<u16>(state, operands, [](u16* lhs, u16* rhs) {
        lhs[0] -= rhs[0];
        lhs[1] -= rhs[1];
        lhs[2] -= rhs[2];
        lhs[3] -= rhs[3];
        lhs[4] -= rhs[4];
        lhs[5] -= rhs[5];
        lhs[6] -= rhs[6];
        lhs[7] -= rhs[7];
    });
}

INTR_HANDLE(PSUBD) {
    ElementOperation<u32>(state, operands, [](u32* lhs, u32* rhs) {
        lhs[0] -= rhs[0];
        lhs[1] -= rhs[1];
        lhs[2] -= rhs[2];
        lhs[3] -= rhs[3];
    });
}

INTR_HANDLE(PSUBQ) {
    ElementOperation<u64>(state, operands, [](u64* lhs, u64* rhs) {
        lhs[0] -= rhs[0];
        lhs[1] -= rhs[1];
    });
}

INTR_HANDLE(ADDPS) {
    ElementOperation<float>(state, operands, [](float* lhs, float* rhs) {
        lhs[0] += rhs[0];
        lhs[1] += rhs[1];
        lhs[2] += rhs[2];
        lhs[3] += rhs[3];
    });
}

INTR_HANDLE(ADDPD) {
    ElementOperation<double>(state, operands, [](double* lhs, double* rhs) {
        lhs[0] += rhs[0];
        lhs[1] += rhs[1];
    });
}

INTR_HANDLE(SUBPS) {
    ElementOperation<float>(state, operands, [](float* lhs, float* rhs) {
        lhs[0] -= rhs[0];
        lhs[1] -= rhs[1];
        lhs[2] -= rhs[2];
        lhs[3] -= rhs[3];
    });
}

INTR_HANDLE(SUBPD) {
    ElementOperation<double>(state, operands, [](double* lhs, double* rhs) {
        lhs[0] -= rhs[0];
        lhs[1] -= rhs[1];
    });
}

INTR_HANDLE(MULPS) {
    ElementOperation<float>(state, operands, [](float* lhs, float* rhs) {
        lhs[0] *= rhs[0];
        lhs[1] *= rhs[1];
        lhs[2] *= rhs[2];
        lhs[3] *= rhs[3];
    });
}

INTR_HANDLE(MULPD) {
    ElementOperation<double>(state, operands, [](double* lhs, double* rhs) {
        lhs[0] *= rhs[0];
        lhs[1] *= rhs[1];
    });
}

INTR_HANDLE(DIVPS) {
    ElementOperation<float>(state, operands, [](float* lhs, float* rhs) {
        lhs[0] /= rhs[0];
        lhs[1] /= rhs[1];
        lhs[2] /= rhs[2];
        lhs[3] /= rhs[3];
    });
}

INTR_HANDLE(DIVPD) {
    ElementOperation<double>(state, operands, [](double* lhs, double* rhs) {
        lhs[0] /= rhs[0];
        lhs[1] /= rhs[1];
    });
}

INTR_HANDLE(MINPS) {
    ElementOperation<float>(state, operands, [](float* lhs, float* rhs) {
        lhs[0] = lhs[0] < rhs[0] ? lhs[0] : rhs[0];
        lhs[1] = lhs[1] < rhs[1] ? lhs[1] : rhs[1];
        lhs[2] = lhs[2] < rhs[2] ? lhs[2] : rhs[2];
        lhs[3] = lhs[3] < rhs[3] ? lhs[3] : rhs[3];
    });
}

INTR_HANDLE(MINPD) {
    ElementOperation<double>(state, operands, [](double* lhs, double* rhs) {
        lhs[0] = lhs[0] < rhs[0] ? lhs[0] : rhs[0];
        lhs[1] = lhs[1] < rhs[1] ? lhs[1] : rhs[1];
    });
}

INTR_HANDLE(MAXPS) {
    ElementOperation<float>(state, operands, [](float* lhs, float* rhs) {
        lhs[0] = lhs[0] > rhs[0] ? lhs[0] : rhs[0];
        lhs[1] = lhs[1] > rhs[1] ? lhs[1] : rhs[1];
        lhs[2] = lhs[2] > rhs[2] ? lhs[2] : rhs[2];
        lhs[3] = lhs[3] > rhs[3] ? lhs[3] : rhs[3];
    });
}

INTR_HANDLE(MAXPD) {
    ElementOperation<double>(state, operands, [](double* lhs, double* rhs) {
        lhs[0] = lhs[0] > rhs[0] ? lhs[0] : rhs[0];
        lhs[1] = lhs[1] > rhs[1] ? lhs[1] : rhs[1];
    });
}
