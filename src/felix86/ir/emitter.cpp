#include "felix86/ir/emitter.hpp"

namespace {
u64 ImmSext(u64 imm, x86_size_e size) {
    i64 value = imm;
    switch (size) {
    case X86_SIZE_BYTE:
        value = (i8)value;
        break;
    case X86_SIZE_WORD:
        value = (i16)value;
        break;
    case X86_SIZE_DWORD:
        value = (i32)value;
        break;
    case X86_SIZE_QWORD:
        break;
    default:
        ERROR("Invalid immediate size");
    }
    return value;
}
} // namespace

SSAInstruction* IREmitter::GetReg(x86_ref_e reg, x86_size_e size, bool high) {
    ASSERT(!high || size == X86_SIZE_BYTE);
    switch (size) {
    case X86_SIZE_BYTE: {
        if (high) {
            return getGpr8High(reg);
        } else {
            return getGpr8Low(reg);
        }
    }
    case X86_SIZE_WORD:
        return getGpr16(reg);
    case X86_SIZE_DWORD:
        return getGpr32(reg);
    case X86_SIZE_QWORD:
        return getGpr64(reg);
    case X86_SIZE_XMM:
        return getVector(reg);
    default:
        ERROR("Invalid register size");
        return nullptr;
    }
}

void IREmitter::SetReg(SSAInstruction* value, x86_ref_e reg, x86_size_e size, bool high) {
    ASSERT(!high || size == X86_SIZE_BYTE);
    switch (size) {
    case X86_SIZE_BYTE: {
        if (high) {
            setGpr8High(reg, value);
        } else {
            setGpr8Low(reg, value);
        }
        break;
    }
    case X86_SIZE_WORD:
        setGpr16(reg, value);
        break;
    case X86_SIZE_DWORD:
        setGpr32(reg, value);
        break;
    case X86_SIZE_QWORD:
        setGpr64(reg, value);
        break;
    case X86_SIZE_XMM:
        setVector(reg, value);
        break;
    default:
        ERROR("Invalid register size");
        break;
    }
}

SSAInstruction* IREmitter::GetFlag(x86_ref_e ref) {
    if (ref < X86_REF_CF || ref > X86_REF_OF) {
        ERROR("Invalid flag reference");
    }

    return getGuest(ref);
}

SSAInstruction* IREmitter::GetFlagNot(x86_ref_e ref) {
    if (ref < X86_REF_CF || ref > X86_REF_OF) {
        ERROR("Invalid flag reference");
    }

    return Xori(getGuest(ref), 1);
}

void IREmitter::SetFlag(SSAInstruction* value, x86_ref_e ref) {
    if (ref < X86_REF_CF || ref > X86_REF_OF) {
        ERROR("Invalid flag reference");
    }

    setGuest(ref, value);
}

SSAInstruction* IREmitter::GetRm(const x86_operand_t& operand) {
    if (operand.type == X86_OP_TYPE_REGISTER) {
        return GetReg(operand.reg.ref, operand.size);
    } else {
        SSAInstruction* address = Lea(operand);
        return ReadMemory(address, operand.size);
    }
}

void IREmitter::SetRm(const x86_operand_t& operand, SSAInstruction* value) {
    if (operand.type == X86_OP_TYPE_REGISTER) {
        SetReg(value, operand.reg.ref, operand.size);
    } else {
        SSAInstruction* address = Lea(operand);
        WriteMemory(address, value, operand.size);
    }
}

void IREmitter::Comment(const std::string& comment) {
    block->InsertAtEnd(SSAInstruction(comment));
}

SSAInstruction* IREmitter::Imm(u64 value) {
    SSAInstruction instruction(value);
    return block->InsertAtEnd(std::move(instruction));
}

SSAInstruction* IREmitter::Add(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::Add, {lhs, rhs});
}

SSAInstruction* IREmitter::Addi(SSAInstruction* lhs, i64 rhs) {
    ASSERT(IsValidSigned12BitImm(rhs));
    return insertInstruction(IROpcode::Addi, {lhs}, rhs);
}

SSAInstruction* IREmitter::Sub(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::Sub, {lhs, rhs});
}

SSAInstruction* IREmitter::Shl(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::Shl, {lhs, rhs});
}

SSAInstruction* IREmitter::Shli(SSAInstruction* lhs, i64 rhs) {
    return insertInstruction(IROpcode::Shli, {lhs}, rhs);
}

SSAInstruction* IREmitter::Shr(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::Shr, {lhs, rhs});
}

SSAInstruction* IREmitter::Shri(SSAInstruction* lhs, i64 rhs) {
    return insertInstruction(IROpcode::Shri, {lhs}, rhs);
}

SSAInstruction* IREmitter::Sar(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::Sar, {lhs, rhs});
}

SSAInstruction* IREmitter::Sari(SSAInstruction* lhs, i64 rhs) {
    return insertInstruction(IROpcode::Sari, {lhs}, rhs);
}

SSAInstruction* IREmitter::Rol(SSAInstruction* lhs, SSAInstruction* rhs, x86_size_e size) {
    switch (size) {
    case x86_size_e::X86_SIZE_BYTE:
        return insertInstruction(IROpcode::Rol8, {lhs, rhs});
    case x86_size_e::X86_SIZE_WORD:
        return insertInstruction(IROpcode::Rol16, {lhs, rhs});
    case x86_size_e::X86_SIZE_DWORD:
        return insertInstruction(IROpcode::Rol32, {lhs, rhs});
    case x86_size_e::X86_SIZE_QWORD:
        return insertInstruction(IROpcode::Rol64, {lhs, rhs});
    default:
        UNREACHABLE();
        return nullptr;
    }
}

SSAInstruction* IREmitter::Ror(SSAInstruction* lhs, SSAInstruction* rhs, x86_size_e size) {
    switch (size) {
    case x86_size_e::X86_SIZE_BYTE:
        return insertInstruction(IROpcode::Ror8, {lhs, rhs});
    case x86_size_e::X86_SIZE_WORD:
        return insertInstruction(IROpcode::Ror16, {lhs, rhs});
    case x86_size_e::X86_SIZE_DWORD:
        return insertInstruction(IROpcode::Ror32, {lhs, rhs});
    case x86_size_e::X86_SIZE_QWORD:
        return insertInstruction(IROpcode::Ror64, {lhs, rhs});
    default:
        UNREACHABLE();
        return nullptr;
    }
}

SSAInstruction* IREmitter::Select(SSAInstruction* cond, SSAInstruction* true_value, SSAInstruction* false_value) {
    return insertInstruction(IROpcode::Select, {cond, true_value, false_value});
}

SSAInstruction* IREmitter::Clz(SSAInstruction* value) {
    return insertInstruction(IROpcode::Clz, {value});
}

SSAInstruction* IREmitter::Ctz(SSAInstruction* value) {
    return insertInstruction(IROpcode::Ctz, {value});
}

SSAInstruction* IREmitter::Ctzh(SSAInstruction* value) {
    return insertInstruction(IROpcode::Ctzh, {value});
}

SSAInstruction* IREmitter::Ctzw(SSAInstruction* value) {
    return insertInstruction(IROpcode::Ctzw, {value});
}

SSAInstruction* IREmitter::Parity(SSAInstruction* value) {
    return insertInstruction(IROpcode::Parity, {value});
}

SSAInstruction* IREmitter::And(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::And, {lhs, rhs});
}

SSAInstruction* IREmitter::Andi(SSAInstruction* lhs, u64 rhs) {
    if (IsValidSigned12BitImm(rhs)) {
        return insertInstruction(IROpcode::Andi, {lhs}, rhs);
    } else {
        WARN("Andi reduced to And with immediate: 0x%lx", rhs);
        SSAInstruction* imm = Imm(rhs);
        return insertInstruction(IROpcode::And, {lhs, imm});
    }
}

SSAInstruction* IREmitter::Or(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::Or, {lhs, rhs});
}

SSAInstruction* IREmitter::Ori(SSAInstruction* lhs, u64 rhs) {
    if (IsValidSigned12BitImm(rhs)) {
        return insertInstruction(IROpcode::Ori, {lhs}, rhs);
    } else {
        WARN("Ori reduced to Or with immediate: 0x%lx", rhs);
        SSAInstruction* imm = Imm(rhs);
        return insertInstruction(IROpcode::Or, {lhs, imm});
    }
}

SSAInstruction* IREmitter::Xor(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::Xor, {lhs, rhs});
}

SSAInstruction* IREmitter::Xori(SSAInstruction* lhs, u64 rhs) {
    if (IsValidSigned12BitImm(rhs)) {
        return insertInstruction(IROpcode::Xori, {lhs}, rhs);
    } else {
        WARN("Xori reduced to Xor with immediate: 0x%lx", rhs);
        SSAInstruction* imm = Imm(rhs);
        return insertInstruction(IROpcode::Xor, {lhs, imm});
    }
}

SSAInstruction* IREmitter::Not(SSAInstruction* value) {
    return insertInstruction(IROpcode::Not, {value});
}

SSAInstruction* IREmitter::Neg(SSAInstruction* value) {
    return insertInstruction(IROpcode::Neg, {value});
}

SSAInstruction* IREmitter::Mul(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::Mul, {lhs, rhs});
}

SSAInstruction* IREmitter::Mulh(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::Mulh, {lhs, rhs});
}

SSAInstruction* IREmitter::Mulhu(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::Mulhu, {lhs, rhs});
}

SSAInstruction* IREmitter::Div(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::Div, {lhs, rhs});
}

SSAInstruction* IREmitter::Divu(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::Divu, {lhs, rhs});
}

SSAInstruction* IREmitter::Rem(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::Rem, {lhs, rhs});
}

SSAInstruction* IREmitter::Remu(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::Remu, {lhs, rhs});
}

SSAInstruction* IREmitter::Divw(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::Divw, {lhs, rhs});
}

SSAInstruction* IREmitter::Divuw(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::Divuw, {lhs, rhs});
}

SSAInstruction* IREmitter::Remw(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::Remw, {lhs, rhs});
}

SSAInstruction* IREmitter::Remuw(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::Remuw, {lhs, rhs});
}

SSAInstruction* IREmitter::Seqz(SSAInstruction* value) {
    return insertInstruction(IROpcode::Seqz, {value});
}

SSAInstruction* IREmitter::Snez(SSAInstruction* value) {
    return insertInstruction(IROpcode::Snez, {value});
}

SSAInstruction* IREmitter::Equal(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::Equal, {lhs, rhs});
}

SSAInstruction* IREmitter::NotEqual(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::NotEqual, {lhs, rhs});
}

SSAInstruction* IREmitter::LessThanSigned(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::SetLessThanSigned, {lhs, rhs});
}

SSAInstruction* IREmitter::LessThanUnsigned(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::SetLessThanUnsigned, {lhs, rhs});
}

SSAInstruction* IREmitter::GreaterThanSigned(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::SetLessThanSigned, {rhs, lhs});
}

SSAInstruction* IREmitter::GreaterThanUnsigned(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::SetLessThanUnsigned, {rhs, lhs});
}

SSAInstruction* IREmitter::Sext(SSAInstruction* value, x86_size_e size) {
    switch (size) {
    case x86_size_e::X86_SIZE_BYTE:
        return insertInstruction(IROpcode::Sext8, {value});
    case x86_size_e::X86_SIZE_WORD:
        return insertInstruction(IROpcode::Sext16, {value});
    case x86_size_e::X86_SIZE_DWORD:
        return insertInstruction(IROpcode::Sext32, {value});
    case x86_size_e::X86_SIZE_QWORD:
        return value;
    default:
        UNREACHABLE();
        return nullptr;
    }
}
SSAInstruction* IREmitter::Zext(SSAInstruction* value, x86_size_e size) {
    switch (size) {
    case x86_size_e::X86_SIZE_BYTE:
        return insertInstruction(IROpcode::Zext8, {value});
    case x86_size_e::X86_SIZE_WORD:
        return insertInstruction(IROpcode::Zext16, {value});
    case x86_size_e::X86_SIZE_DWORD:
        return insertInstruction(IROpcode::Zext32, {value});
    case x86_size_e::X86_SIZE_QWORD:
        return value;
    default:
        UNREACHABLE();
        return nullptr;
    }
}

SSAInstruction* IREmitter::AmoAdd(SSAInstruction* address, SSAInstruction* source, MemoryOrdering ordering, x86_size_e size) {
    switch (size) {
    case x86_size_e::X86_SIZE_BYTE:
        return insertInstruction(IROpcode::AmoAdd8, {address, source}, (u8)ordering);
    case x86_size_e::X86_SIZE_WORD:
        return insertInstruction(IROpcode::AmoAdd16, {address, source}, (u8)ordering);
    case x86_size_e::X86_SIZE_DWORD:
        return insertInstruction(IROpcode::AmoAdd32, {address, source}, (u8)ordering);
    case x86_size_e::X86_SIZE_QWORD:
        return insertInstruction(IROpcode::AmoAdd64, {address, source}, (u8)ordering);
    default:
        UNREACHABLE();
        return nullptr;
    }
}

SSAInstruction* IREmitter::AmoAnd(SSAInstruction* address, SSAInstruction* source, MemoryOrdering ordering, x86_size_e size) {
    switch (size) {
    case x86_size_e::X86_SIZE_BYTE:
        return insertInstruction(IROpcode::AmoAnd8, {address, source}, (u8)ordering);
    case x86_size_e::X86_SIZE_WORD:
        return insertInstruction(IROpcode::AmoAnd16, {address, source}, (u8)ordering);
    case x86_size_e::X86_SIZE_DWORD:
        return insertInstruction(IROpcode::AmoAnd32, {address, source}, (u8)ordering);
    case x86_size_e::X86_SIZE_QWORD:
        return insertInstruction(IROpcode::AmoAnd64, {address, source}, (u8)ordering);
    default:
        UNREACHABLE();
        return nullptr;
    }
}

SSAInstruction* IREmitter::AmoOr(SSAInstruction* address, SSAInstruction* source, MemoryOrdering ordering, x86_size_e size) {
    switch (size) {
    case x86_size_e::X86_SIZE_BYTE:
        return insertInstruction(IROpcode::AmoOr8, {address, source}, (u8)ordering);
    case x86_size_e::X86_SIZE_WORD:
        return insertInstruction(IROpcode::AmoOr16, {address, source}, (u8)ordering);
    case x86_size_e::X86_SIZE_DWORD:
        return insertInstruction(IROpcode::AmoOr32, {address, source}, (u8)ordering);
    case x86_size_e::X86_SIZE_QWORD:
        return insertInstruction(IROpcode::AmoOr64, {address, source}, (u8)ordering);
    default:
        UNREACHABLE();
        return nullptr;
    }
}

SSAInstruction* IREmitter::AmoXor(SSAInstruction* address, SSAInstruction* source, MemoryOrdering ordering, x86_size_e size) {
    switch (size) {
    case x86_size_e::X86_SIZE_BYTE:
        return insertInstruction(IROpcode::AmoXor8, {address, source}, (u8)ordering);
    case x86_size_e::X86_SIZE_WORD:
        return insertInstruction(IROpcode::AmoXor16, {address, source}, (u8)ordering);
    case x86_size_e::X86_SIZE_DWORD:
        return insertInstruction(IROpcode::AmoXor32, {address, source}, (u8)ordering);
    case x86_size_e::X86_SIZE_QWORD:
        return insertInstruction(IROpcode::AmoXor64, {address, source}, (u8)ordering);
    default:
        UNREACHABLE();
        return nullptr;
    }
}

SSAInstruction* IREmitter::AmoSwap(SSAInstruction* address, SSAInstruction* source, MemoryOrdering ordering, x86_size_e size) {
    switch (size) {
    case x86_size_e::X86_SIZE_BYTE:
        return insertInstruction(IROpcode::AmoSwap8, {address, source}, (u8)ordering);
    case x86_size_e::X86_SIZE_WORD:
        return insertInstruction(IROpcode::AmoSwap16, {address, source}, (u8)ordering);
    case x86_size_e::X86_SIZE_DWORD:
        return insertInstruction(IROpcode::AmoSwap32, {address, source}, (u8)ordering);
    case x86_size_e::X86_SIZE_QWORD:
        return insertInstruction(IROpcode::AmoSwap64, {address, source}, (u8)ordering);
    default:
        UNREACHABLE();
        return nullptr;
    }
}

SSAInstruction* IREmitter::AmoCAS(SSAInstruction* address, SSAInstruction* expected, SSAInstruction* source, MemoryOrdering ordering,
                                  x86_size_e size) {
    switch (size) {
    case x86_size_e::X86_SIZE_BYTE:
        return insertInstruction(IROpcode::AmoSwap8, {address, expected, source}, (u8)ordering);
    case x86_size_e::X86_SIZE_WORD:
        return insertInstruction(IROpcode::AmoSwap16, {address, expected, source}, (u8)ordering);
    case x86_size_e::X86_SIZE_DWORD:
        return insertInstruction(IROpcode::AmoSwap32, {address, expected, source}, (u8)ordering);
    case x86_size_e::X86_SIZE_QWORD:
        return insertInstruction(IROpcode::AmoSwap64, {address, expected, source}, (u8)ordering);
    default:
        UNREACHABLE();
        return nullptr;
    }
}

SSAInstruction* IREmitter::VUnpackByteLow(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::VUnpackByteLow, {lhs, rhs});
}

SSAInstruction* IREmitter::VUnpackWordLow(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::VUnpackWordLow, {lhs, rhs});
}

SSAInstruction* IREmitter::VUnpackDWordLow(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::VUnpackDWordLow, {lhs, rhs});
}

SSAInstruction* IREmitter::VUnpackQWordLow(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::VUnpackQWordLow, {lhs, rhs});
}

SSAInstruction* IREmitter::VPackedEqualByte(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::VPackedEqualByte, {lhs, rhs});
}

SSAInstruction* IREmitter::VPackedEqualWord(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::VPackedEqualWord, {lhs, rhs});
}

SSAInstruction* IREmitter::VPackedEqualDWord(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::VPackedEqualDWord, {lhs, rhs});
}

SSAInstruction* IREmitter::VPackedEqualQWord(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::VPackedEqualQWord, {lhs, rhs});
}

SSAInstruction* IREmitter::VPackedAddByte(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::VPackedAddByte, {lhs, rhs});
}

SSAInstruction* IREmitter::VPackedAddWord(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::VPackedAddWord, {lhs, rhs});
}

SSAInstruction* IREmitter::VPackedAddDWord(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::VPackedAddDWord, {lhs, rhs});
}

SSAInstruction* IREmitter::VPackedAddQWord(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::VPackedAddQWord, {lhs, rhs});
}

SSAInstruction* IREmitter::VPackedShuffleDWord(SSAInstruction* value, u8 shuffle) {
    return insertInstruction(IROpcode::VPackedShuffleDWord, {value}, shuffle);
}

SSAInstruction* IREmitter::VPackedShr(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::VPackedShr, {lhs, rhs});
}

SSAInstruction* IREmitter::VPackedMinByte(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::VPackedMinByte, {lhs, rhs});
}

SSAInstruction* IREmitter::VPackedSubByte(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::VPackedSubByte, {lhs, rhs});
}

SSAInstruction* IREmitter::VMoveByteMask(SSAInstruction* value) {
    return insertInstruction(IROpcode::VMoveByteMask, {value});
}

SSAInstruction* IREmitter::VAnd(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::VAnd, {lhs, rhs});
}

SSAInstruction* IREmitter::VOr(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::VOr, {lhs, rhs});
}

SSAInstruction* IREmitter::VXor(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::VXor, {lhs, rhs});
}

SSAInstruction* IREmitter::IToV(SSAInstruction* value) {
    return insertInstruction(IROpcode::IToV, {value});
}

SSAInstruction* IREmitter::VToI(SSAInstruction* value) {
    return insertInstruction(IROpcode::VToI, {value});
}

SSAInstruction* IREmitter::Lea(const x86_operand_t& operand) {
    x86_size_e address_size = operand.memory.address_override ? X86_SIZE_DWORD : X86_SIZE_QWORD;
    SSAInstruction *base, *index;
    if (operand.memory.base != X86_REF_COUNT) {
        base = GetReg(operand.memory.base, address_size);
    } else {
        base = Imm(0);
    }

    if (operand.memory.index != X86_REF_COUNT) {
        index = GetReg(operand.memory.index, address_size);
    } else {
        index = nullptr;
    }

    SSAInstruction* base_final = base;
    if (operand.memory.fs_override) {
        SSAInstruction* fs = getGuest(X86_REF_FS);
        base_final = Add(base, fs);
    } else if (operand.memory.gs_override) {
        SSAInstruction* gs = getGuest(X86_REF_GS);
        base_final = Add(base, gs);
    }

    SSAInstruction* address = base_final;
    if (index) {
        ASSERT(operand.memory.scale >= 0 && operand.memory.scale <= 3);
        SSAInstruction* scaled_index = Shli(index, operand.memory.scale);
        address = Add(base_final, scaled_index);
    }

    SSAInstruction* displaced_address = address;
    if (operand.memory.displacement) {
        if (IsValidSigned12BitImm(operand.memory.displacement)) {
            displaced_address = Addi(address, operand.memory.displacement);
        } else {
            displaced_address = Add(address, Imm(operand.memory.displacement));
        }
    }

    SSAInstruction* final_address = displaced_address;
    if (operand.memory.address_override) {
        final_address = Zext(displaced_address, X86_SIZE_DWORD);
    }

    return final_address;
}

SSAInstruction* IREmitter::GetFlags() {
    SSAInstruction* c = GetFlag(X86_REF_CF);
    SSAInstruction* p = GetFlag(X86_REF_PF);
    SSAInstruction* a = GetFlag(X86_REF_AF);
    SSAInstruction* z = GetFlag(X86_REF_ZF);
    SSAInstruction* s = GetFlag(X86_REF_SF);
    SSAInstruction* o = GetFlag(X86_REF_OF);
    SSAInstruction* d = GetFlag(X86_REF_DF);

    SSAInstruction* p_shifted = Shli(p, 2);
    SSAInstruction* a_shifted = Shli(a, 4);
    SSAInstruction* z_shifted = Shli(z, 6);
    SSAInstruction* s_shifted = Shli(s, 7);
    SSAInstruction* d_shifted = Shli(d, 10);
    SSAInstruction* o_shifted = Shli(o, 11);

    SSAInstruction* c_p = Or(c, p_shifted);
    SSAInstruction* a_z = Or(a_shifted, z_shifted);
    SSAInstruction* a_z_o = Or(a_z, o_shifted);
    SSAInstruction* c_p_s = Or(c_p, s_shifted);
    SSAInstruction* c_p_s_d = Or(c_p_s, d_shifted);
    SSAInstruction* result_almost = Or(c_p_s_d, a_z_o);
    SSAInstruction* result = Ori(result_almost, 0b10); // always set bit 1

    return result;
}

SSAInstruction* IREmitter::IsZero(SSAInstruction* value, x86_size_e size) {
    return Seqz(Zext(value, size));
}

SSAInstruction* IREmitter::IsNegative(SSAInstruction* value, x86_size_e size) {
    switch (size) {
    case X86_SIZE_BYTE:
        return Andi(Shri(value, 7), 1);
    case X86_SIZE_WORD:
        return Andi(Shri(value, 15), 1);
    case X86_SIZE_DWORD:
        return Andi(Shri(value, 31), 1);
    case X86_SIZE_QWORD:
        return Andi(Shri(value, 63), 1);
    default:
        UNREACHABLE();
        return nullptr;
    }
}

void IREmitter::Syscall() {
    // The kernel clobbers registers rcx and r11 but preserves all other registers except rax which is the result.
    // We don't have to clobber rcx and r11 as we are not a kernel.
    // First, get_guest and writeback rax, rdi, rsi, rdx, r10, r8, r9 because they may be used by the syscall.
    // Then emit the syscall instruction.
    // Finally, load rax so it's not propagated from before the syscall.
    constexpr static std::array in_regs = {X86_REF_RAX, X86_REF_RDI, X86_REF_RSI, X86_REF_RDX, X86_REF_R10, X86_REF_R8, X86_REF_R9};
    constexpr static std::array out_regs = {X86_REF_RAX};

    storePartialState(in_regs);

    SSAInstruction* syscall = insertInstruction(IROpcode::Syscall, {});
    syscall->Lock();

    loadPartialState(out_regs);
}

void IREmitter::Cpuid() {
    constexpr static std::array in_regs = {X86_REF_RAX, X86_REF_RCX};
    constexpr static std::array out_regs = {X86_REF_RAX, X86_REF_RBX, X86_REF_RCX, X86_REF_RDX};

    storePartialState(in_regs);

    SSAInstruction* cpuid = insertInstruction(IROpcode::Cpuid, {});
    cpuid->Lock();

    loadPartialState(out_regs);
}

void IREmitter::Rdtsc() {
    // Has no inputs but writes to EDX:EAX
    constexpr static std::array out_regs = {X86_REF_RAX, X86_REF_RDX};

    SSAInstruction* instruction = insertInstruction(IROpcode::Rdtsc, {});
    instruction->Lock();

    loadPartialState(out_regs);
}

void IREmitter::SetCC(x86_instruction_t* inst) {
    SetRm(inst->operand_rm, GetCC(inst->opcode));
}

SSAInstruction* IREmitter::GetCC(u8 opcode) {
    switch (opcode & 0xF) {
    case 0:
        return GetFlag(X86_REF_OF);
    case 1:
        return GetFlagNot(X86_REF_OF);
    case 2:
        return GetFlag(X86_REF_CF);
    case 3:
        return GetFlagNot(X86_REF_CF);
    case 4:
        return GetFlag(X86_REF_ZF);
    case 5:
        return GetFlagNot(X86_REF_ZF);
    case 6:
        return Or(GetFlag(X86_REF_CF), GetFlag(X86_REF_ZF));
    case 7:
        return And(GetFlagNot(X86_REF_CF), GetFlagNot(X86_REF_ZF));
    case 8:
        return GetFlag(X86_REF_SF);
    case 9:
        return GetFlagNot(X86_REF_SF);
    case 10:
        return GetFlag(X86_REF_PF);
    case 11:
        return GetFlagNot(X86_REF_PF);
    case 12:
        return NotEqual(GetFlag(X86_REF_SF), GetFlag(X86_REF_OF));
    case 13:
        return Equal(GetFlag(X86_REF_SF), GetFlag(X86_REF_OF));
    case 14:
        return Or(Equal(GetFlag(X86_REF_ZF), Imm(1)), NotEqual(GetFlag(X86_REF_SF), GetFlag(X86_REF_OF)));
    case 15:
        return And(Equal(GetFlag(X86_REF_ZF), Imm(0)), Equal(GetFlag(X86_REF_SF), GetFlag(X86_REF_OF)));
    }

    ERROR("Invalid condition code");
    return nullptr;
}

void IREmitter::SetCPAZSO(SSAInstruction* c, SSAInstruction* p, SSAInstruction* a, SSAInstruction* z, SSAInstruction* s, SSAInstruction* o) {
    if (c)
        SetFlag(c, X86_REF_CF);
    if (p)
        SetFlag(p, X86_REF_PF);
    if (a)
        SetFlag(a, X86_REF_AF);
    if (z)
        SetFlag(z, X86_REF_ZF);
    if (s)
        SetFlag(s, X86_REF_SF);
    if (o)
        SetFlag(o, X86_REF_OF);
}

void IREmitter::SetExitReason(ExitReason reason) {
    SSAInstruction* set_exit_reason = insertInstruction(IROpcode::SetExitReason, {}, reason);
    set_exit_reason->Lock();
}

void IREmitter::SetFlags(SSAInstruction* flags) {
    SSAInstruction* c = Andi(flags, 1);
    SSAInstruction* p = Andi(Shri(flags, 2), 1);
    SSAInstruction* a = Andi(Shri(flags, 4), 1);
    SSAInstruction* z = Andi(Shri(flags, 6), 1);
    SSAInstruction* s = Andi(Shri(flags, 7), 1);
    SSAInstruction* d = Andi(Shri(flags, 10), 1);
    SSAInstruction* o = Andi(Shri(flags, 11), 1);

    SetFlag(c, X86_REF_CF);
    SetFlag(p, X86_REF_PF);
    SetFlag(a, X86_REF_AF);
    SetFlag(z, X86_REF_ZF);
    SetFlag(s, X86_REF_SF);
    SetFlag(d, X86_REF_DF);
    SetFlag(o, X86_REF_OF);
}

SSAInstruction* IREmitter::insertInstruction(IROpcode opcode, std::initializer_list<SSAInstruction*> operands) {
    SSAInstruction instruction(opcode, operands);
    return block->InsertAtEnd(std::move(instruction));
}

SSAInstruction* IREmitter::insertInstruction(IROpcode opcode, std::initializer_list<SSAInstruction*> operands, u64 imm) {
    SSAInstruction instruction(opcode, operands, imm);
    return block->InsertAtEnd(std::move(instruction));
}

SSAInstruction* IREmitter::getGuest(x86_ref_e ref) {
    if (ref == X86_REF_COUNT) {
        ERROR("Invalid register reference");
    }

    SSAInstruction instruction(IROpcode::GetGuest, ref);
    return block->InsertAtEnd(std::move(instruction));
}

SSAInstruction* IREmitter::setGuest(x86_ref_e ref, SSAInstruction* value) {
    if (ref == X86_REF_COUNT) {
        ERROR("Invalid register reference");
    }

    SSAInstruction instruction(IROpcode::SetGuest, ref, value);
    return block->InsertAtEnd(std::move(instruction));
}

SSAInstruction* IREmitter::getGpr8Low(x86_ref_e ref) {
    if (ref < X86_REF_RAX || ref > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    return Zext(getGuest(ref), X86_SIZE_BYTE);
}

SSAInstruction* IREmitter::getGpr8High(x86_ref_e ref) {
    if (ref < X86_REF_RAX || ref > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    return Zext(Shri(getGuest(ref), 8), X86_SIZE_BYTE);
}

SSAInstruction* IREmitter::getGpr16(x86_ref_e ref) {
    if (ref < X86_REF_RAX || ref > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    return Zext(getGuest(ref), X86_SIZE_WORD);
}

SSAInstruction* IREmitter::getGpr32(x86_ref_e ref) {
    if (ref < X86_REF_RAX || ref > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    return Zext(getGuest(ref), X86_SIZE_DWORD);
}

SSAInstruction* IREmitter::getGpr64(x86_ref_e ref) {
    if (ref < X86_REF_RAX || ref > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    return getGuest(ref);
}

SSAInstruction* IREmitter::getVector(x86_ref_e ref) {
    if (ref < X86_REF_XMM0 || ref > X86_REF_XMM15) {
        ERROR("Invalid register reference");
    }

    return getGuest(ref);
}

void IREmitter::setGpr8Low(x86_ref_e ref, SSAInstruction* value) {
    if (ref < X86_REF_RAX || ref > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    SSAInstruction* old = getGpr64(ref);
    SSAInstruction* masked_old = Andi(old, 0xFFFFFFFFFFFFFF00);
    SSAInstruction* masked_value = Zext(value, X86_SIZE_BYTE);
    SSAInstruction* new_value = Or(masked_old, masked_value);
    setGuest(ref, new_value);
}

void IREmitter::setGpr8High(x86_ref_e ref, SSAInstruction* value) {
    if (ref < X86_REF_RAX || ref > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    SSAInstruction* old = getGpr64(ref);
    SSAInstruction* masked_old = And(old, Imm(0xFFFFFFFFFFFF00FF));
    SSAInstruction* masked_value = Zext(value, X86_SIZE_BYTE);
    SSAInstruction* shifted_value = Shli(masked_value, 8);
    SSAInstruction* new_value = Or(masked_old, shifted_value);
    setGuest(ref, new_value);
}

void IREmitter::setGpr16(x86_ref_e ref, SSAInstruction* value) {
    if (ref < X86_REF_RAX || ref > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    SSAInstruction* old = getGpr64(ref);
    SSAInstruction* masked_old = And(old, Imm(0xFFFFFFFFFFFF0000));
    SSAInstruction* masked_value = Zext(value, X86_SIZE_WORD);
    SSAInstruction* new_value = Or(masked_old, masked_value);
    setGuest(ref, new_value);
}

void IREmitter::setGpr32(x86_ref_e ref, SSAInstruction* value) {
    if (ref < X86_REF_RAX || ref > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    setGuest(ref, Zext(value, X86_SIZE_DWORD));
}

void IREmitter::setGpr64(x86_ref_e ref, SSAInstruction* value) {
    if (ref < X86_REF_RAX || ref > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    setGuest(ref, value);
}

void IREmitter::setVector(x86_ref_e ref, SSAInstruction* value) {
    if (ref < X86_REF_XMM0 || ref > X86_REF_XMM15) {
        ERROR("Invalid register reference");
    }

    setGuest(ref, value);
}

SSAInstruction* IREmitter::readByte(SSAInstruction* address) {
    return insertInstruction(IROpcode::ReadByte, {address});
}

SSAInstruction* IREmitter::readWord(SSAInstruction* address) {
    return insertInstruction(IROpcode::ReadWord, {address});
}

SSAInstruction* IREmitter::readDWord(SSAInstruction* address) {
    return insertInstruction(IROpcode::ReadDWord, {address});
}

SSAInstruction* IREmitter::readQWord(SSAInstruction* address) {
    return insertInstruction(IROpcode::ReadQWord, {address});
}

SSAInstruction* IREmitter::readXmmWord(SSAInstruction* address) {
    return insertInstruction(IROpcode::ReadXmmWord, {address});
}

SSAInstruction* IREmitter::ReadMemory(SSAInstruction* address, x86_size_e size) {
    switch (size) {
    case X86_SIZE_BYTE:
        return readByte(address);
    case X86_SIZE_WORD:
        return readWord(address);
    case X86_SIZE_DWORD:
        return readDWord(address);
    case X86_SIZE_QWORD:
        return readQWord(address);
    case X86_SIZE_XMM:
        return readXmmWord(address);
    default:
        ERROR("Invalid memory size");
        return nullptr;
    }
}

void IREmitter::writeByte(SSAInstruction* address, SSAInstruction* value) {
    SSAInstruction* instruction = insertInstruction(IROpcode::WriteByte, {address, value});
    instruction->Lock();
}

void IREmitter::writeWord(SSAInstruction* address, SSAInstruction* value) {
    SSAInstruction* instruction = insertInstruction(IROpcode::WriteWord, {address, value});
    instruction->Lock();
}

void IREmitter::writeDWord(SSAInstruction* address, SSAInstruction* value) {
    SSAInstruction* instruction = insertInstruction(IROpcode::WriteDWord, {address, value});
    instruction->Lock();
}

void IREmitter::writeQWord(SSAInstruction* address, SSAInstruction* value) {
    SSAInstruction* instruction = insertInstruction(IROpcode::WriteQWord, {address, value});
    instruction->Lock();
}

void IREmitter::writeXmmWord(SSAInstruction* address, SSAInstruction* value) {
    SSAInstruction* instruction = insertInstruction(IROpcode::WriteXmmWord, {address, value});
    instruction->Lock();
}

void IREmitter::WriteMemory(SSAInstruction* address, SSAInstruction* value, x86_size_e size) {
    switch (size) {
    case X86_SIZE_BYTE:
        return writeByte(address, value);
    case X86_SIZE_WORD:
        return writeWord(address, value);
    case X86_SIZE_DWORD:
        return writeDWord(address, value);
    case X86_SIZE_QWORD:
        return writeQWord(address, value);
    case X86_SIZE_XMM:
        return writeXmmWord(address, value);
    default:
        ERROR("Invalid memory size");
        return;
    }
}

SSAInstruction* IREmitter::IsCarryAdd(SSAInstruction* source, SSAInstruction* result, x86_size_e size_e) {
    SSAInstruction* zext_result = Zext(result, size_e);
    return LessThanUnsigned(zext_result, source);
}

SSAInstruction* IREmitter::IsCarryAdc(SSAInstruction* lhs, SSAInstruction* rhs, SSAInstruction* carry, x86_size_e size_e) {
    SSAInstruction* sum = Add(lhs, rhs);
    SSAInstruction* sum2 = Add(sum, carry);
    SSAInstruction* carry1 = IsCarryAdd(lhs, sum, size_e);
    SSAInstruction* carry2 = IsCarryAdd(sum, sum2, size_e);
    return Or(carry1, carry2);
}

SSAInstruction* IREmitter::IsCarrySbb(SSAInstruction* lhs, SSAInstruction* rhs, SSAInstruction* carry, x86_size_e size_e) {
    SSAInstruction* sum = Sub(lhs, rhs);
    SSAInstruction* carry2 = IsCarrySub(sum, carry);
    SSAInstruction* carry1 = IsCarrySub(lhs, rhs);
    return Or(carry1, carry2);
}

SSAInstruction* IREmitter::IsAuxAdd(SSAInstruction* lhs, SSAInstruction* rhs) {
    SSAInstruction* and1 = Andi(lhs, 0xF);
    SSAInstruction* and2 = Andi(rhs, 0xF);
    SSAInstruction* result = Add(and1, and2);

    return GreaterThanUnsigned(result, Imm(0xF));
}

SSAInstruction* IREmitter::IsOverflowAdd(SSAInstruction* lhs, SSAInstruction* rhs, SSAInstruction* result, x86_size_e size_e) {
    SSAInstruction* mask = getSignMask(size_e);

    // for x + y = z, overflow occurs if ((z ^ x) & (z ^ y) & mask) == mask
    // which essentially checks if the sign bits of x and y are equal, but the
    // sign bit of z is different
    SSAInstruction* xor1 = Xor(result, lhs);
    SSAInstruction* xor2 = Xor(result, rhs);
    SSAInstruction* masked1 = And(xor1, xor2);
    SSAInstruction* masked2 = And(masked1, mask);

    return Equal(masked2, mask);
}

SSAInstruction* IREmitter::IsCarrySub(SSAInstruction* lhs, SSAInstruction* rhs) {
    return LessThanUnsigned(lhs, rhs);
}

SSAInstruction* IREmitter::IsAuxSub(SSAInstruction* lhs, SSAInstruction* rhs) {
    SSAInstruction* and1 = Andi(lhs, 0xF);
    SSAInstruction* and2 = Andi(rhs, 0xF);

    return LessThanUnsigned(and1, and2);
}

SSAInstruction* IREmitter::IsOverflowSub(SSAInstruction* lhs, SSAInstruction* rhs, SSAInstruction* result, x86_size_e size_e) {
    SSAInstruction* mask = getSignMask(size_e);

    // for x - y = z, overflow occurs if ((x ^ y) & (x ^ z) & mask) == mask
    SSAInstruction* xor1 = Xor(lhs, rhs);
    SSAInstruction* xor2 = Xor(lhs, result);
    SSAInstruction* masked1 = And(xor1, xor2);
    SSAInstruction* masked2 = And(masked1, mask);

    return Equal(masked2, mask);
}

SSAInstruction* IREmitter::LoadGuestFromMemory(x86_ref_e ref) {
    if (ref == X86_REF_COUNT) {
        ERROR("Invalid register reference");
    }

    SSAInstruction instruction(IROpcode::LoadGuestFromMemory, ref);
    return block->InsertAtEnd(std::move(instruction));
}

void IREmitter::StoreGuestToMemory(SSAInstruction* value, x86_ref_e ref) {
    if (ref == X86_REF_COUNT) {
        ERROR("Invalid register reference");
    }

    SSAInstruction instruction(IROpcode::StoreGuestToMemory, ref, value);
    instruction.Lock();
    block->InsertAtEnd(std::move(instruction));
}

void IREmitter::loadPartialState(std::span<const x86_ref_e> refs) {
    for (x86_ref_e reg : refs) {
        SSAInstruction* guest = LoadGuestFromMemory(reg);
        setGuest(reg, guest);
    }
}

void IREmitter::storePartialState(std::span<const x86_ref_e> refs) {
    for (x86_ref_e reg : refs) {
        SSAInstruction* guest = getGuest(reg);
        StoreGuestToMemory(guest, reg);
    }
}

SSAInstruction* IREmitter::getSignMask(x86_size_e size_e) {
    u16 size = GetBitSize(size_e);
    return Imm(1ull << (size - 1));
}

u16 IREmitter::GetBitSize(x86_size_e size) {
    switch (size) {
    case X86_SIZE_BYTE:
        return 8;
    case X86_SIZE_WORD:
        return 16;
    case X86_SIZE_DWORD:
        return 32;
    case X86_SIZE_QWORD:
        return 64;
    case X86_SIZE_MM:
        return 64;
    case X86_SIZE_XMM:
        return 128;
    }

    ERROR("Invalid register size");
    return 0;
}

SSAInstruction* IREmitter::VInsertInteger(SSAInstruction* integer, SSAInstruction* vector, u8 index, x86_size_e size) {
    u64 immediate_data = (u64)size << 8 | index;
    return insertInstruction(IROpcode::VInsertInteger, {integer, vector}, immediate_data);
}

void IREmitter::Group1(x86_instruction_t* inst) {
    x86_group1_e opcode = (x86_group1_e)((inst->operand_reg.reg.ref & 0x7) - X86_REF_RAX);

    x86_size_e size_e = inst->operand_rm.size;
    SSAInstruction* rm = GetRm(inst->operand_rm);
    SSAInstruction* imm = Imm(ImmSext(inst->operand_imm.immediate.data, inst->operand_imm.size));
    SSAInstruction* result = nullptr;
    SSAInstruction* zero = Imm(0);
    SSAInstruction* c = zero;
    SSAInstruction* o = zero;
    SSAInstruction* a = nullptr;

    switch (opcode) {
    case X86_GROUP1_ADD: {
        result = Add(rm, imm);
        c = IsCarryAdd(rm, result, size_e);
        o = IsOverflowAdd(rm, imm, result, size_e);
        a = IsAuxAdd(rm, imm);
        break;
    }
    case X86_GROUP1_ADC: {
        SSAInstruction* carry_in = GetFlag(X86_REF_CF);
        SSAInstruction* imm_carry = Add(imm, carry_in);
        result = Add(rm, imm_carry);
        c = IsCarryAdc(rm, imm, carry_in, size_e);
        o = IsOverflowAdd(rm, imm_carry, result, size_e);
        a = IsAuxAdd(rm, imm_carry);
        break;
    }
    case X86_GROUP1_SBB: {
        SSAInstruction* carry_in = GetFlag(X86_REF_CF);
        SSAInstruction* imm_carry = Add(imm, carry_in);
        result = Sub(rm, imm_carry);
        c = IsCarrySbb(rm, imm, carry_in, size_e);
        o = IsOverflowSub(rm, imm_carry, result, size_e);
        a = IsAuxSub(rm, imm_carry);
        break;
    }
    case X86_GROUP1_OR: {
        result = Or(rm, imm);
        break;
    }
    case X86_GROUP1_AND: {
        result = And(rm, imm);
        break;
    }
    case X86_GROUP1_SUB: {
        result = Sub(rm, imm);
        c = IsCarrySub(rm, imm);
        o = IsOverflowSub(rm, imm, result, size_e);
        a = IsAuxSub(rm, imm);
        break;
    }
    case X86_GROUP1_XOR: {
        result = Xor(rm, imm);
        break;
    }
    case X86_GROUP1_CMP: {
        result = Sub(rm, imm);
        c = IsCarrySub(rm, imm);
        o = IsOverflowSub(rm, imm, result, size_e);
        a = IsAuxSub(rm, imm);
        break;
    }
    }

    SSAInstruction* p = Parity(result);
    SSAInstruction* z = IsZero(result, size_e);
    SSAInstruction* s = IsNegative(result, size_e);

    SetCPAZSO(c, p, a, z, s, o);

    if (opcode != X86_GROUP1_CMP) {
        SetRm(inst->operand_rm, result);
    }
}

void IREmitter::Group2(x86_instruction_t* inst, SSAInstruction* shift_amount) {
    x86_group2_e opcode = (x86_group2_e)((inst->operand_reg.reg.ref & 0x7) - X86_REF_RAX);

    x86_size_e size_e = inst->operand_rm.size;
    u8 shift_mask = size_e == X86_SIZE_QWORD ? 0x3F : 0x1F;
    SSAInstruction* rm = GetRm(inst->operand_rm);
    SSAInstruction* shift_value = Andi(shift_amount, shift_mask);
    SSAInstruction* result = nullptr;
    SSAInstruction* c = nullptr;
    SSAInstruction* p = nullptr;
    SSAInstruction* a = nullptr;
    SSAInstruction* z = nullptr;
    SSAInstruction* s = nullptr;
    SSAInstruction* o = nullptr;

    switch (opcode) {
    case X86_GROUP2_ROL: {
        result = Rol(rm, shift_value, size_e);
        SSAInstruction* msb = IsNegative(result, size_e);
        c = Andi(result, 1);
        o = Xor(c, msb);
        break;
    }
    case X86_GROUP2_ROR: {
        result = Ror(rm, shift_value, size_e);
        c = IsNegative(result, size_e);
        WARN("ROR OF unimplemented");
        break;
    }
    case X86_GROUP2_RCL: {
        ERROR("Why? :(");
        break;
    }
    case X86_GROUP2_RCR: {
        ERROR("Why? :(");
        break;
    }
    case X86_GROUP2_SAL:
    case X86_GROUP2_SHL: {
        SSAInstruction* shift = Sub(Imm(GetBitSize(size_e)), shift_value);
        SSAInstruction* msb_mask = Shl(Imm(1), shift);
        result = Shl(rm, shift_value);
        c = Equal(And(rm, msb_mask), msb_mask);
        SSAInstruction* sign = IsNegative(result, size_e);
        o = Xor(c, sign);
        break;
    }
    case X86_GROUP2_SHR: {
        SSAInstruction* is_zero = Seqz(shift_value);
        SSAInstruction* shift = Addi(shift_value, 1);
        SSAInstruction* mask = Shl(Imm(1), shift);
        SSAInstruction* msb_mask = Select(is_zero, Imm(0), mask);
        result = Shr(rm, shift_value);
        c = Equal(And(rm, msb_mask), msb_mask);
        o = IsNegative(rm, size_e);
        break;
    }
    case X86_GROUP2_SAR: {
        // Shift left to place MSB to bit 63
        u8 anti_shift = 64 - GetBitSize(size_e);
        SSAInstruction* shifted_left = Shli(rm, anti_shift);
        SSAInstruction* shift_right = Addi(shift_value, anti_shift);
        SSAInstruction* is_zero = Seqz(shift_value);
        SSAInstruction* shift = Addi(shift_value, 1);
        SSAInstruction* mask = Shl(Imm(1), shift);
        SSAInstruction* msb_mask = Select(is_zero, Imm(0), mask);
        result = Sar(shifted_left, shift_right);
        o = Imm(0);
        c = Equal(And(rm, msb_mask), msb_mask);
        break;
    }
    }

    p = Parity(result);
    z = IsZero(result, size_e);
    s = IsNegative(result, size_e);

    SetCPAZSO(c, p, a, z, s, o);

    SetRm(inst->operand_rm, result);
}

void IREmitter::Group3(x86_instruction_t* inst) {
    x86_group3_e opcode = (x86_group3_e)((inst->operand_reg.reg.ref & 0x7) - X86_REF_RAX);

    x86_size_e size_e = inst->operand_rm.size;
    SSAInstruction* rm = GetRm(inst->operand_rm);
    SSAInstruction* result = nullptr;
    SSAInstruction* c = nullptr;
    SSAInstruction* p = nullptr;
    SSAInstruction* a = nullptr;
    SSAInstruction* z = nullptr;
    SSAInstruction* s = nullptr;
    SSAInstruction* o = nullptr;

    switch (opcode) {
    case X86_GROUP3_TEST:
    case X86_GROUP3_TEST_: {
        SSAInstruction* imm = Imm(ImmSext(inst->operand_imm.immediate.data, inst->operand_imm.size));
        SSAInstruction* masked = And(rm, imm);
        s = IsNegative(masked, size_e);
        z = IsZero(masked, size_e);
        p = Parity(masked);
        break;
    }
    case X86_GROUP3_NOT: {
        result = Not(rm);
        break;
    }
    case X86_GROUP3_NEG: {
        result = Neg(rm);
        z = IsZero(result, size_e);
        c = Seqz(z);
        s = IsNegative(result, size_e);
        o = IsOverflowSub(Imm(0), rm, result, size_e);
        a = IsAuxSub(Imm(0), rm);
        p = Parity(result);
        break;
    }
    case X86_GROUP3_MUL: {
        UNIMPLEMENTED();
        break;
    }
    case X86_GROUP3_IMUL: {
        switch (size_e) {
        case X86_SIZE_BYTE: {
            SSAInstruction* al = Sext(GetReg(X86_REF_RAX, X86_SIZE_BYTE), X86_SIZE_BYTE);
            SSAInstruction* se_rm = Sext(rm, X86_SIZE_BYTE);
            SSAInstruction* mul = Mul(al, se_rm);
            SetReg(mul, X86_REF_RAX, X86_SIZE_WORD);
            break;
        }
        case X86_SIZE_WORD: {
            SSAInstruction* ax = Sext(GetReg(X86_REF_RAX, X86_SIZE_WORD), X86_SIZE_WORD);
            SSAInstruction* se_rm = Sext(rm, X86_SIZE_WORD);
            SSAInstruction* mul = Mul(ax, se_rm);
            SSAInstruction* mul_high = Shri(mul, 16);
            SetReg(mul, X86_REF_RAX, X86_SIZE_WORD);
            SetReg(mul_high, X86_REF_RDX, X86_SIZE_WORD);
            break;
        }
        case X86_SIZE_DWORD: {
            SSAInstruction* eax = Sext(GetReg(X86_REF_RAX, X86_SIZE_DWORD), X86_SIZE_DWORD);
            SSAInstruction* se_rm = Sext(rm, X86_SIZE_DWORD);
            SSAInstruction* mul = Mul(eax, se_rm);
            SSAInstruction* mul_high = Shri(mul, 32);
            SetReg(mul, X86_REF_RAX, X86_SIZE_DWORD);
            SetReg(mul_high, X86_REF_RDX, X86_SIZE_DWORD);
            break;
        }
        case X86_SIZE_QWORD: {
            SSAInstruction* rax = GetReg(X86_REF_RAX, X86_SIZE_QWORD);
            SSAInstruction* mul = Mul(rax, rm);
            SSAInstruction* mul_high = Mulh(rax, rm);
            SetReg(mul, X86_REF_RAX, X86_SIZE_QWORD);
            SetReg(mul_high, X86_REF_RDX, X86_SIZE_QWORD);
            break;
        }
        default: {
            UNREACHABLE();
        }
        }
        break;
    }
    case X86_GROUP3_DIV: {
        switch (size_e) {
        case X86_SIZE_BYTE: {
            // ax / rm, al := quotient, ah := remainder
            SSAInstruction* ax = GetReg(X86_REF_RAX, X86_SIZE_WORD);
            SSAInstruction* quotient = Divuw(ax, rm);
            SSAInstruction* remainder = Remuw(ax, rm);
            SetReg(quotient, X86_REF_RAX, X86_SIZE_BYTE);
            SetReg(remainder, X86_REF_RAX, X86_SIZE_BYTE, true);
            break;
        }
        case X86_SIZE_WORD: {
            // dx:ax / rm, ax := quotient, dx := remainder
            SSAInstruction* ax = GetReg(X86_REF_RAX, X86_SIZE_WORD);
            SSAInstruction* dx = GetReg(X86_REF_RDX, X86_SIZE_WORD);
            SSAInstruction* dx_shifted = Shli(dx, 16);
            SSAInstruction* dx_ax = Or(dx_shifted, ax);
            SSAInstruction* quotient = Divuw(dx_ax, rm);
            SetReg(quotient, X86_REF_RAX, X86_SIZE_WORD);
            SSAInstruction* remainder = Remuw(dx_ax, rm);
            SetReg(remainder, X86_REF_RDX, X86_SIZE_WORD);
            break;
        }
        case X86_SIZE_DWORD: {
            // edx:eax / rm, eax := quotient, edx := remainder
            SSAInstruction* eax = GetReg(X86_REF_RAX, X86_SIZE_DWORD);
            SSAInstruction* edx = GetReg(X86_REF_RDX, X86_SIZE_DWORD);
            SSAInstruction* edx_shifted = Shli(edx, 32);
            SSAInstruction* edx_eax = Or(edx_shifted, eax);
            SSAInstruction* quotient = Divu(edx_eax, rm);
            SetReg(quotient, X86_REF_RAX, X86_SIZE_DWORD);
            SSAInstruction* remainder = Remu(edx_eax, rm);
            SetReg(remainder, X86_REF_RDX, X86_SIZE_DWORD);
            break;
        }
        case X86_SIZE_QWORD: {
            // rdx:rax / rm, rax := quotient, rdx := remainder
            constexpr static std::array reg_refs = {X86_REF_RAX, X86_REF_RDX};

            storePartialState(reg_refs);

            SSAInstruction* instruction = insertInstruction(IROpcode::Div128, {rm});
            instruction->Lock();

            loadPartialState(reg_refs);
            break;
        }
        default: {
            UNREACHABLE();
        }
        }
        break;
    }
    case X86_GROUP3_IDIV: {
        switch (size_e) {
        case X86_SIZE_BYTE: {
            // ax / rm, al := quotient, ah := remainder
            SSAInstruction* ax = Sext(GetReg(X86_REF_RAX), X86_SIZE_WORD);
            SSAInstruction* se_rm = Sext(rm, X86_SIZE_BYTE);
            SSAInstruction* quotient = Divw(ax, se_rm);
            SetReg(quotient, X86_REF_RAX, X86_SIZE_BYTE);
            SSAInstruction* remainder = Remw(ax, se_rm);
            SetReg(remainder, X86_REF_RAX, X86_SIZE_BYTE, true);
            break;
        }
        case X86_SIZE_WORD: {
            // dx:ax / rm, ax := quotient, dx := remainder
            SSAInstruction* ax = GetReg(X86_REF_RAX, X86_SIZE_WORD);
            SSAInstruction* dx = GetReg(X86_REF_RDX, X86_SIZE_WORD);
            SSAInstruction* dx_shifted = Shli(dx, 16);
            SSAInstruction* dx_ax = Or(dx_shifted, ax);
            SSAInstruction* se_rm = Sext(rm, X86_SIZE_WORD);
            SSAInstruction* quotient = Divw(dx_ax, se_rm);
            SetReg(quotient, X86_REF_RAX, X86_SIZE_WORD);
            SSAInstruction* remainder = Remw(dx_ax, se_rm);
            SetReg(remainder, X86_REF_RDX, X86_SIZE_WORD);
            break;
        }
        case X86_SIZE_DWORD: {
            // edx:eax / rm, eax := quotient, edx := remainder
            SSAInstruction* eax = GetReg(X86_REF_RAX, X86_SIZE_DWORD);
            SSAInstruction* edx = GetReg(X86_REF_RDX, X86_SIZE_DWORD);
            SSAInstruction* edx_shifted = Shli(edx, 32);
            SSAInstruction* edx_eax = Or(edx_shifted, eax);
            SSAInstruction* se_rm = Sext(rm, X86_SIZE_DWORD);
            SSAInstruction* quotient = Div(edx_eax, se_rm);
            SetReg(quotient, X86_REF_RAX, X86_SIZE_DWORD);
            SSAInstruction* remainder = Rem(edx_eax, se_rm);
            SetReg(remainder, X86_REF_RDX, X86_SIZE_DWORD);
            break;
        }
        case X86_SIZE_QWORD: {
            // rdx:rax / rm, rax := quotient, rdx := remainder
            constexpr static std::array reg_refs = {X86_REF_RAX, X86_REF_RDX};

            storePartialState(reg_refs);

            SSAInstruction* instruction = insertInstruction(IROpcode::Div128, {rm});
            instruction->Lock();

            loadPartialState(reg_refs);
            break;
        }
        default: {
            UNREACHABLE();
        }
        }
        break;
    }
    }

    SetCPAZSO(c, p, a, z, s, o);

    if (result) {
        SetRm(inst->operand_rm, result);
    }
}

SSAInstruction* IREmitter::GetThreadStatePointer() {
    return insertInstruction(IROpcode::GetThreadStatePointer, {});
}

void IREmitter::RepStart(IRBlock* loop_block, IRBlock* exit_block) {
    SSAInstruction* rcx = GetReg(X86_REF_RCX);
    SSAInstruction* condition = Seqz(rcx);
    TerminateJumpConditional(condition, exit_block, loop_block);
}

void IREmitter::RepEnd(x86_rep_e rep_type, IRBlock* loop_block, IRBlock* exit_block) {
    SSAInstruction* rcx = GetReg(X86_REF_RCX);
    SSAInstruction* zero = Imm(0);
    SSAInstruction* sub = Addi(rcx, -1ll);
    SetReg(sub, X86_REF_RCX);
    SSAInstruction* rcx_zero = Seqz(sub);
    SSAInstruction* condition;
    SSAInstruction* zf = GetFlag(X86_REF_ZF);
    if (rep_type == REP) { // Some instructions don't check the ZF flag
        condition = zero;
    } else if (rep_type == REP_NZ) {
        condition = Snez(zf);
    } else if (rep_type == REP_Z) {
        condition = Seqz(zf);
    } else {
        UNREACHABLE();
        return;
    }

    SSAInstruction* final_condition = Or(rcx_zero, condition);
    TerminateJumpConditional(final_condition, exit_block, loop_block);
    Exit();
}

void IREmitter::TerminateJump(IRBlock* target) {
    ASSERT_MSG(block->GetTermination() == Termination::Null, "Block %s already has a termination", block->GetName().c_str());
    block->TerminateJump(target);
}

void IREmitter::TerminateJumpConditional(SSAInstruction* condition, IRBlock* target_true, IRBlock* target_false) {
    ASSERT_MSG(block->GetTermination() == Termination::Null, "Block %s already has a termination", block->GetName().c_str());
    block->TerminateJumpConditional(condition, target_true, target_false);
}

void IREmitter::CallHostFunction(u64 function_address) {
    SSAInstruction* instruction = insertInstruction(IROpcode::CallHostFunction, {}, function_address);
    instruction->Lock();
}