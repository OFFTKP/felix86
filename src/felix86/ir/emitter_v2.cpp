#include "felix86/ir/emitter_v2.hpp"

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

SSAInstruction* IREmitter::Imm(u64 value) {
    SSAInstruction instruction(value);
    return block.InsertAtEnd(std::move(instruction));
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
    ASSERT(IsValidSigned12BitImm(rhs));
    return insertInstruction(IROpcode::Andi, {lhs}, rhs);
}

SSAInstruction* IREmitter::Or(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::Or, {lhs, rhs});
}

SSAInstruction* IREmitter::Ori(SSAInstruction* lhs, u64 rhs) {
    ASSERT(IsValidSigned12BitImm(rhs));
    return insertInstruction(IROpcode::Ori, {lhs}, rhs);
}

SSAInstruction* IREmitter::Xor(SSAInstruction* lhs, SSAInstruction* rhs) {
    return insertInstruction(IROpcode::Xor, {lhs, rhs});
}

SSAInstruction* IREmitter::Xori(SSAInstruction* lhs, u64 rhs) {
    ASSERT(IsValidSigned12BitImm(rhs));
    return insertInstruction(IROpcode::Xori, {lhs}, rhs);
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

SSAInstruction* IREmitter::Lea(const x86_operand_t& operand) {
    x86_size_e address_size = operand.memory.address_override ? X86_SIZE_DWORD : X86_SIZE_QWORD;
    SSAInstruction *base, *index;
    if (operand.memory.base != X86_REF_COUNT) {
        base = GetReg(operand.memory.base, address_size);
    } else {
        base = Imm(0);
    }

    if (operand.memory.index != X86_REF_COUNT) {
        index = GetReg(operand.memory.base, address_size);
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
        ASSERT(operand.memory.scale == 1 || operand.memory.scale == 2 || operand.memory.scale == 4 || operand.memory.scale == 8);
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

SSAInstruction* IREmitter::insertInstruction(IROpcode opcode, std::initializer_list<SSAInstruction*> operands) {
    SSAInstruction instruction(opcode, operands);
    return block.InsertAtEnd(std::move(instruction));
}

SSAInstruction* IREmitter::getGuest(x86_ref_e ref) {
    if (ref == X86_REF_COUNT) {
        ERROR("Invalid register reference");
    }

    SSAInstruction instruction(IROpcode::GetGuest, ref);
    return block.InsertAtEnd(std::move(instruction));
}

SSAInstruction* IREmitter::setGuest(x86_ref_e ref, SSAInstruction* value) {
    if (ref == X86_REF_COUNT) {
        ERROR("Invalid register reference");
    }

    SSAInstruction instruction(IROpcode::SetGuest, ref, value);
    return block.InsertAtEnd(std::move(instruction));
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
    SSAInstruction* masked_value = Andi(value, 0xFF);
    SSAInstruction* new_value = Or(masked_old, masked_value);
    setGuest(ref, new_value);
}

void IREmitter::setGpr8High(x86_ref_e ref, SSAInstruction* value) {
    if (ref < X86_REF_RAX || ref > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    SSAInstruction* old = getGpr64(ref);
    SSAInstruction* masked_old = Andi(old, 0xFFFFFFFFFFFF00FF);
    SSAInstruction* masked_value = Andi(value, 0xFF);
    SSAInstruction* shifted_value = Shli(masked_value, 8);
    SSAInstruction* new_value = Or(masked_old, shifted_value);
    setGuest(ref, new_value);
}

void IREmitter::setGpr16(x86_ref_e ref, SSAInstruction* value) {
    if (ref < X86_REF_RAX || ref > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    SSAInstruction* old = getGpr64(ref);
    SSAInstruction* masked_old = Andi(old, 0xFFFFFFFFFFFF0000);
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