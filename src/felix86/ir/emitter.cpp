#include "felix86/frontend/frontend.hpp"
#include "felix86/frontend/instruction.hpp"
#include "felix86/ir/emitter.hpp"

namespace {
SSAInstruction* SecondMSB(IREmitter& ir, SSAInstruction* value, x86_size_e size) {
    switch (size) {
    case X86_SIZE_BYTE:
        return ir.Andi(ir.Shri(value, 6), 1);
    case X86_SIZE_WORD:
        return ir.Andi(ir.Shri(value, 14), 1);
    case X86_SIZE_DWORD:
        return ir.Andi(ir.Shri(value, 30), 1);
    case X86_SIZE_QWORD:
        return ir.Andi(ir.Shri(value, 62), 1);
    default:
        UNREACHABLE();
        return nullptr;
    }
}
} // namespace

u64 IREmitter::ImmSext(u64 imm, x86_size_e size) {
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

SSAInstruction* IREmitter::GetRm(const x86_operand_t& operand, VectorState vector_state) {
    if (operand.type == X86_OP_TYPE_REGISTER) {
        return GetReg(operand.reg.ref, operand.size);
    } else {
        SSAInstruction* address = Lea(operand);
        return ReadMemory(address, operand.size, vector_state);
    }
}

void IREmitter::SetRm(const x86_operand_t& operand, SSAInstruction* value, VectorState vector_state) {
    if (operand.type == X86_OP_TYPE_REGISTER) {
        SetReg(value, operand.reg.ref, operand.size);
    } else {
        SSAInstruction* address = Lea(operand);
        WriteMemory(address, value, operand.size, vector_state);
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

SSAInstruction* IREmitter::AddShifted(SSAInstruction* lhs, SSAInstruction* rhs, u8 shift) {
    ASSERT(shift <= 3);
    return insertInstruction(IROpcode::AddShifted, {lhs, rhs}, shift);
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
    case x86_size_e::X86_SIZE_BYTE: {
        SSAInstruction* left_shift = Shl(lhs, rhs);
        SSAInstruction* right_shift = Shr(lhs, Andi(Neg(rhs), 7));
        return Zext(Or(left_shift, right_shift), X86_SIZE_BYTE);
    }
    case x86_size_e::X86_SIZE_WORD: {
        SSAInstruction* left_shift = Shl(lhs, Andi(rhs, 15));
        SSAInstruction* right_shift = Shr(lhs, Andi(Neg(rhs), 15));
        return Zext(Or(left_shift, right_shift), X86_SIZE_WORD);
    }
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
    case x86_size_e::X86_SIZE_BYTE: {
        SSAInstruction* left_shift = Shl(lhs, Andi(Neg(rhs), 7));
        SSAInstruction* right_shift = Shr(lhs, Andi(rhs, 7));
        return Zext(Or(left_shift, right_shift), X86_SIZE_BYTE);
    }
    case x86_size_e::X86_SIZE_WORD: {
        SSAInstruction* left_shift = Shl(lhs, Andi(Neg(rhs), 15));
        SSAInstruction* right_shift = Shr(lhs, Andi(rhs, 15));
        return Zext(Or(left_shift, right_shift), X86_SIZE_WORD);
    }
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

SSAInstruction* IREmitter::LoadReserved32(SSAInstruction* address, biscuit::Ordering ordering) {
    SSAInstruction* load = insertInstruction(IROpcode::LoadReserved32, {address}, (u8)ordering);
    load->Lock();
    return load;
}

SSAInstruction* IREmitter::LoadReserved64(SSAInstruction* address, biscuit::Ordering ordering) {
    SSAInstruction* load = insertInstruction(IROpcode::LoadReserved64, {address}, (u8)ordering);
    load->Lock();
    return load;
}

SSAInstruction* IREmitter::StoreConditional32(SSAInstruction* address, SSAInstruction* value, biscuit::Ordering ordering) {
    SSAInstruction* store = insertInstruction(IROpcode::StoreConditional32, {address, value}, (u8)ordering);
    store->Lock();
    return store;
}

SSAInstruction* IREmitter::StoreConditional64(SSAInstruction* address, SSAInstruction* value, biscuit::Ordering ordering) {
    SSAInstruction* store = insertInstruction(IROpcode::StoreConditional64, {address, value}, (u8)ordering);
    store->Lock();
    return store;
}

SSAInstruction* IREmitter::atomic8(SSAInstruction* address, SSAInstruction* source, IROpcode opcode) {
    if (Extensions::Zabha) {
        SSAInstruction* instruction = insertInstruction(opcode, {address, source}, (u8)biscuit::Ordering::AQRL);
        instruction->Lock();
        return instruction;
    }

    ASSERT(current_address != 0);
    u64 next_address = GetNextAddress();

    IRBlock* header = CreateBlock();
    IRBlock* loop = CreateBlock();
    IRBlock* conclusion = CreateBlock();
    TerminateJump(header);
    SetBlock(header);

    // Mask the address to grab a whole word
    SSAInstruction* masked_address = Andi(address, (i64)-4);

    // Creates a shift amount based on the 3 lowest bits of address, which indicates
    // where in the 32-bit word the byte is.
    // Thus shift amount is 8 * (address & 3) (0, 8, 16, ..., 56)
    SSAInstruction* bits = Andi(address, 3);
    SSAInstruction* shift_amount = Shli(bits, 3);
    SSAInstruction* mask = Imm(0xFF);
    SSAInstruction* shifted_mask = Shl(mask, shift_amount);

    // Shift the amount we are operating on too
    SSAInstruction* shifted_source = Shl(source, shift_amount);
    shifted_source->Lock();
    SSAInstruction* not_shifted_mask = Not(shifted_mask);
    not_shifted_mask->Lock();

    TerminateJump(loop);
    SetBlock(loop);

    // We don't want anything in here to be spilled as it could cause issues
    // so we lock every instruction inside the loop
    SSAInstruction* load = LoadReserved32(masked_address, biscuit::Ordering::AQRL);
    SSAInstruction* result;
    switch (opcode) {
    case IROpcode::AmoAdd8: {
        result = Add(load, shifted_source);
        break;
    }
    case IROpcode::AmoAnd8: {
        result = And(load, shifted_source);
        break;
    }
    case IROpcode::AmoOr8: {
        result = Or(load, shifted_source);
        break;
    }
    case IROpcode::AmoXor8: {
        result = Xor(load, shifted_source);
        break;
    }
    case IROpcode::AmoSwap8: {
        result = shifted_source;
        break;
    }
    default: {
        UNREACHABLE();
        return nullptr;
    }
    }
    result->Lock();
    SSAInstruction* masked_load = And(load, not_shifted_mask);
    masked_load->Lock();
    SSAInstruction* masked_result = And(result, shifted_mask);
    masked_result->Lock();
    SSAInstruction* new_value = Or(masked_load, masked_result);
    new_value->Lock();
    SSAInstruction* success = StoreConditional32(masked_address, new_value, biscuit::Ordering::RL);

    // Jump to loop if store didn't return zero
    SSAInstruction* condition = Snez(success);
    TerminateJumpConditional(condition, loop, conclusion);

    SetBlock(conclusion);

    // Return the original loaded value
    SSAInstruction* load_shifted = Shr(load, shift_amount);
    SSAInstruction* load_masked = Andi(load_shifted, 0xFF);

    IRBlock* next_block = CreateBlockAt(next_address);
    TerminateJump(next_block);
    Exit();

    frontend_compile_block(GetFunction(), next_block);

    return load_masked;
}

SSAInstruction* IREmitter::cas64(SSAInstruction* address, SSAInstruction* expected, SSAInstruction* source) {
    if (Extensions::Zacas) {
        return insertInstruction(IROpcode::AmoCAS64, {address, expected, source}, (u8)biscuit::Ordering::AQRL);
    }

    ASSERT(current_address != 0);
    u64 next_address = GetNextAddress();
    IRBlock* loop = CreateBlock();
    IRBlock* cmp_true = CreateBlock();
    IRBlock* next_block = CreateBlockAt(next_address);

    TerminateJump(loop);
    SetBlock(loop);

    SSAInstruction* expected_mov = Xori(expected, 0);
    expected_mov->Lock();
    SSAInstruction* load = LoadReserved64(address, biscuit::Ordering::AQRL);
    SSAInstruction* cmp = NotEqual(load, expected_mov);

    TerminateJumpConditional(cmp, next_block, cmp_true);
    SetBlock(cmp_true);

    SSAInstruction* success = StoreConditional64(address, source, biscuit::Ordering::RL);

    SSAInstruction* condition = Snez(success);
    TerminateJumpConditional(condition, loop, next_block);
    Exit();

    frontend_compile_block(GetFunction(), next_block);

    return load;
}

SSAInstruction* IREmitter::AmoAdd(SSAInstruction* address, SSAInstruction* source, x86_size_e size) {
    SSAInstruction* instruction;
    switch (size) {
    case x86_size_e::X86_SIZE_BYTE:
        instruction = atomic8(address, source, IROpcode::AmoAdd8);
        break;
    case x86_size_e::X86_SIZE_WORD:
        instruction = insertInstruction(IROpcode::AmoAdd16, {address, source}, (u8)biscuit::Ordering::AQRL);
        instruction->Lock();
        break;
    case x86_size_e::X86_SIZE_DWORD:
        instruction = insertInstruction(IROpcode::AmoAdd32, {address, source}, (u8)biscuit::Ordering::AQRL);
        instruction->Lock();
        break;
    case x86_size_e::X86_SIZE_QWORD:
        instruction = insertInstruction(IROpcode::AmoAdd64, {address, source}, (u8)biscuit::Ordering::AQRL);
        instruction->Lock();
        break;
    default:
        UNREACHABLE();
        return nullptr;
    }

    return instruction;
}

SSAInstruction* IREmitter::AmoAnd(SSAInstruction* address, SSAInstruction* source, x86_size_e size) {
    SSAInstruction* instruction;
    switch (size) {
    case x86_size_e::X86_SIZE_BYTE:
        instruction = atomic8(address, source, IROpcode::AmoAnd8);
        break;
    case x86_size_e::X86_SIZE_WORD:
        instruction = insertInstruction(IROpcode::AmoAnd16, {address, source}, (u8)biscuit::Ordering::AQRL);
        instruction->Lock();
        break;
    case x86_size_e::X86_SIZE_DWORD:
        instruction = insertInstruction(IROpcode::AmoAnd32, {address, source}, (u8)biscuit::Ordering::AQRL);
        instruction->Lock();
        break;
    case x86_size_e::X86_SIZE_QWORD:
        instruction = insertInstruction(IROpcode::AmoAnd64, {address, source}, (u8)biscuit::Ordering::AQRL);
        instruction->Lock();
        break;
    default:
        UNREACHABLE();
        return nullptr;
    }

    return instruction;
}

SSAInstruction* IREmitter::AmoOr(SSAInstruction* address, SSAInstruction* source, x86_size_e size) {
    SSAInstruction* instruction;
    switch (size) {
    case x86_size_e::X86_SIZE_BYTE:
        instruction = atomic8(address, source, IROpcode::AmoOr8);
        break;
    case x86_size_e::X86_SIZE_WORD:
        instruction = insertInstruction(IROpcode::AmoOr16, {address, source}, (u8)biscuit::Ordering::AQRL);
        instruction->Lock();
        break;
    case x86_size_e::X86_SIZE_DWORD:
        instruction = insertInstruction(IROpcode::AmoOr32, {address, source}, (u8)biscuit::Ordering::AQRL);
        instruction->Lock();
        break;
    case x86_size_e::X86_SIZE_QWORD:
        instruction = insertInstruction(IROpcode::AmoOr64, {address, source}, (u8)biscuit::Ordering::AQRL);
        instruction->Lock();
        break;
    default:
        UNREACHABLE();
        return nullptr;
    }

    return instruction;
}

SSAInstruction* IREmitter::AmoXor(SSAInstruction* address, SSAInstruction* source, x86_size_e size) {
    SSAInstruction* instruction;
    switch (size) {
    case x86_size_e::X86_SIZE_BYTE:
        instruction = atomic8(address, source, IROpcode::AmoXor8);
        break;
    case x86_size_e::X86_SIZE_WORD:
        instruction = insertInstruction(IROpcode::AmoXor16, {address, source}, (u8)biscuit::Ordering::AQRL);
        instruction->Lock();
        break;
    case x86_size_e::X86_SIZE_DWORD:
        instruction = insertInstruction(IROpcode::AmoXor32, {address, source}, (u8)biscuit::Ordering::AQRL);
        instruction->Lock();
        break;
    case x86_size_e::X86_SIZE_QWORD:
        instruction = insertInstruction(IROpcode::AmoXor64, {address, source}, (u8)biscuit::Ordering::AQRL);
        instruction->Lock();
        break;
    default:
        UNREACHABLE();
        return nullptr;
    }

    return instruction;
}

SSAInstruction* IREmitter::AmoSwap(SSAInstruction* address, SSAInstruction* source, x86_size_e size) {
    SSAInstruction* instruction;
    switch (size) {
    case x86_size_e::X86_SIZE_BYTE:
        instruction = atomic8(address, source, IROpcode::AmoSwap8);
        break;
    case x86_size_e::X86_SIZE_WORD:
        instruction = insertInstruction(IROpcode::AmoSwap16, {address, source}, (u8)biscuit::Ordering::AQRL);
        instruction->Lock();
        break;
    case x86_size_e::X86_SIZE_DWORD:
        instruction = insertInstruction(IROpcode::AmoSwap32, {address, source}, (u8)biscuit::Ordering::AQRL);
        instruction->Lock();
        break;
    case x86_size_e::X86_SIZE_QWORD:
        instruction = insertInstruction(IROpcode::AmoSwap64, {address, source}, (u8)biscuit::Ordering::AQRL);
        instruction->Lock();
        break;
    default:
        UNREACHABLE();
        return nullptr;
    }

    return instruction;
}

SSAInstruction* IREmitter::AmoCAS(SSAInstruction* address, SSAInstruction* expected, SSAInstruction* source, x86_size_e size) {
    SSAInstruction* instruction;
    switch (size) {
    case x86_size_e::X86_SIZE_BYTE:
        instruction = insertInstruction(IROpcode::AmoCAS8, {address, expected, source}, (u8)biscuit::Ordering::AQRL);
        break;
    case x86_size_e::X86_SIZE_WORD:
        instruction = insertInstruction(IROpcode::AmoCAS16, {address, expected, source}, (u8)biscuit::Ordering::AQRL);
        break;
    case x86_size_e::X86_SIZE_DWORD:
        instruction = insertInstruction(IROpcode::AmoCAS32, {address, expected, source}, (u8)biscuit::Ordering::AQRL);
        break;
    case x86_size_e::X86_SIZE_QWORD:
        instruction = cas64(address, expected, source);
        break;
    default:
        UNREACHABLE();
        return nullptr;
    }

    instruction->Lock();
    return instruction;
}

SSAInstruction* IREmitter::CZeroEqz(SSAInstruction* value, SSAInstruction* cond) {
    return insertInstruction(IROpcode::CZeroEqz, {value, cond});
}

SSAInstruction* IREmitter::CZeroNez(SSAInstruction* value, SSAInstruction* cond) {
    return insertInstruction(IROpcode::CZeroNez, {value, cond});
}

void IREmitter::Punpckl(x86_instruction_t* inst, VectorState state) {
    SSAInstruction* rm = GetRm(inst->operand_rm, state);
    SSAInstruction* reg = GetReg(inst->operand_reg);
    // Essentially two "vdecompress" (viota + vrgather) instructions
    // If an element index is out of range ( vs1[i] >= VLMAX ) then zero is returned for the element value.
    // This means we don't care to reduce the splat to only the first two elements
    // Doing iota with these masks essentially creates something like
    // [3 3 2 2 1 1 0 0] and [4 3 3 2 2 1 1 0]
    // And the gather itself is also masked
    // So for the reg it picks:
    // [h g f e d c b a]
    // [4 3 3 2 2 1 1 0]
    // [0 1 0 1 0 1 0 1]
    // [x d x c x b x a]
    // And for the rm it picks:
    // [p o n m l k j i]
    // [3 3 2 2 1 1 0 0]
    // [1 0 1 0 1 0 1 0]
    // [l x k x j x i x]
    // Which is the correct interleaving of the two vectors
    // [h g f e d c b a]
    // [p o n m l k j i]
    // -----------------
    // [l d k c j b i a]
    SSAInstruction* rm_mask = VSplat(Imm(0b10101010), state);
    SetVMask(rm_mask);
    SSAInstruction* rm_iota = VIota(rm_mask, state);
    SSAInstruction* zero = VZero(state);
    SSAInstruction* rm_gathered = VGather(zero, rm, rm_iota, state, VecMask::Yes);
    SSAInstruction* reg_mask = VSplat(Imm(0b01010101), state);
    SetVMask(reg_mask);
    SSAInstruction* reg_iota = VIota(reg_mask, state);
    SSAInstruction* result = VGather(rm_gathered, reg, reg_iota, state, VecMask::Yes);
    SetReg(inst->operand_reg, result);
}

void IREmitter::Punpckh(x86_instruction_t* inst, VectorState state) {
    int num;
    switch (state) {
    case VectorState::PackedByte: {
        num = 8;
        break;
    }
    case VectorState::PackedWord: {
        num = 4;
        break;
    }
    case VectorState::PackedDWord: {
        num = 2;
        break;
    }
    case VectorState::PackedQWord: {
        num = 1;
        break;
    }
    default: {
        UNREACHABLE();
        return;
    }
    }

    SSAInstruction* rm = GetRm(inst->operand_rm, state);
    SSAInstruction* reg = GetReg(inst->operand_reg);
    // Like punpckl but we add a number to pick the high elements
    SSAInstruction* rm_mask = VSplat(Imm(0b10101010), state);
    SetVMask(rm_mask);
    SSAInstruction* rm_iota = VIota(rm_mask, state);
    SSAInstruction* rm_iota_added = VAddi(rm_iota, num, state);
    SSAInstruction* zero = VZero(state);
    SSAInstruction* rm_gathered = VGather(zero, rm, rm_iota_added, state, VecMask::Yes);
    SSAInstruction* reg_mask = VSplat(Imm(0b01010101), state);
    SetVMask(reg_mask);
    SSAInstruction* reg_iota = VIota(reg_mask, state);
    SSAInstruction* reg_iota_added = VAddi(reg_iota, num, state);
    SSAInstruction* result = VGather(rm_gathered, reg, reg_iota_added, state, VecMask::Yes);
    SetReg(inst->operand_reg, result);
}

void IREmitter::Pcmpeq(x86_instruction_t* inst, VectorState state) {
    SSAInstruction* rm = GetRm(inst->operand_rm, state);
    SSAInstruction* reg = GetReg(inst->operand_reg);
    SSAInstruction* mask = VEqual(reg, rm, state);
    // Splat 0xFF or 0 based on the mask
    SetVMask(mask);
    SSAInstruction* result = VMergei(-1ull, VZero(state), state);
    SetReg(inst->operand_reg, result);
}

void IREmitter::ScalarRegRm(x86_instruction_t* inst, IROpcode opcode, VectorState state) {
    SSAInstruction *rm, *reg;
    if (inst->operand_rm.type == X86_OP_TYPE_MEMORY) {
        inst->operand_rm.size = state == VectorState::Float ? X86_SIZE_DWORD : X86_SIZE_QWORD;
        SSAInstruction* mem = GetRm(inst->operand_rm);
        rm = IToV(mem, state);
        reg = GetReg(inst->operand_reg);
    } else {
        rm = GetRm(inst->operand_rm, state);
        reg = GetReg(inst->operand_reg);
    }

    SSAInstruction* result_almost = insertInstruction(opcode, state, {reg, rm});

    // Preserve the top bits of the destination register
    VectorState packed_state;
    switch (state) {
    case VectorState::Float:
        packed_state = VectorState::PackedDWord;
        break;
    case VectorState::Double:
        packed_state = VectorState::PackedQWord;
        break;
    default:
        UNREACHABLE();
        return;
    }

    SetVMask(VSplati(0b000000001 /* mask of elements */, state));
    SSAInstruction* result = VMerge(result_almost /* 1's value */, reg /* 0's value */, packed_state);
    SetReg(inst->operand_reg, result);
}

void IREmitter::PackedRegRm(x86_instruction_t* inst, IROpcode opcode, VectorState state) {
    SSAInstruction* rm = GetRm(inst->operand_rm, state);
    SSAInstruction* reg = GetReg(inst->operand_reg);
    SSAInstruction* result = insertInstruction(opcode, state, {reg, rm});
    SetReg(inst->operand_reg, result);
}

void IREmitter::ScalarRegRm(x86_instruction_t* inst, VectorFunc func, VectorState state) {
    SSAInstruction *rm, *reg;
    if (inst->operand_rm.type == X86_OP_TYPE_MEMORY) {
        inst->operand_rm.size = state == VectorState::Float ? X86_SIZE_DWORD : X86_SIZE_QWORD;
        SSAInstruction* mem = GetRm(inst->operand_rm);
        rm = IToV(mem, state);
        reg = GetReg(inst->operand_reg);
    } else {
        rm = GetRm(inst->operand_rm, state);
        reg = GetReg(inst->operand_reg);
    }

    SSAInstruction* result_almost = func(*this, reg, rm, state);

    // Preserve the top bits of the destination register
    VectorState packed_state;
    switch (state) {
    case VectorState::Float:
        packed_state = VectorState::PackedDWord;
        break;
    case VectorState::Double:
        packed_state = VectorState::PackedQWord;
        break;
    default:
        UNREACHABLE();
        return;
    }

    SetVMask(VSplati(0b000000001 /* mask of elements */, state));
    SSAInstruction* result = VMerge(result_almost /* 1's value */, reg /* 0's value */, packed_state);
    SetReg(inst->operand_reg, result);
}

void IREmitter::PackedRegRm(x86_instruction_t* inst, VectorFunc func, VectorState state) {
    SSAInstruction* rm = GetRm(inst->operand_rm, state);
    SSAInstruction* reg = GetReg(inst->operand_reg);
    SSAInstruction* result = func(*this, reg, rm, state);
    SetReg(inst->operand_reg, result);
}

void IREmitter::SetVMask(SSAInstruction* mask) {
    SSAInstruction* instruction = insertInstruction(IROpcode::SetVMask, {mask});
    instruction->Lock();
}

SSAInstruction* IREmitter::VIota(SSAInstruction* mask, VectorState state) {
    return insertInstruction(IROpcode::VIota, state, {mask});
}

SSAInstruction* IREmitter::VId(VectorState state) {
    return insertInstruction(IROpcode::VId, state, {});
}

SSAInstruction* IREmitter::VGather(SSAInstruction* dest, SSAInstruction* source, SSAInstruction* iota, VectorState state, VecMask masked) {
    SSAInstruction* instruction = insertInstruction(IROpcode::VGather, state, {dest, source, iota});
    if (masked == VecMask::Yes) {
        instruction->SetMasked();
    }
    return instruction;
}

SSAInstruction* IREmitter::VZero(VectorState state) {
    return VSplati(0, state);
}

SSAInstruction* IREmitter::VSlli(SSAInstruction* value, u8 shift, VectorState state) {
    return insertInstruction(IROpcode::VSlli, state, {value}, shift);
}

SSAInstruction* IREmitter::VSll(SSAInstruction* lhs, SSAInstruction* rhs, VectorState state) {
    return insertInstruction(IROpcode::VSll, state, {lhs, rhs});
}

SSAInstruction* IREmitter::VSrli(SSAInstruction* value, u8 shift, VectorState state) {
    return insertInstruction(IROpcode::VSrli, state, {value}, shift);
}

SSAInstruction* IREmitter::VSrl(SSAInstruction* lhs, SSAInstruction* rhs, VectorState state) {
    return insertInstruction(IROpcode::VSrl, state, {lhs, rhs});
}

SSAInstruction* IREmitter::VSrai(SSAInstruction* value, u8 shift, VectorState state) {
    return insertInstruction(IROpcode::VSrai, state, {value}, shift);
}

SSAInstruction* IREmitter::VMSeqi(SSAInstruction* value, VectorState state, u64 imm) {
    return insertInstruction(IROpcode::VMSeqi, state, {value}, imm);
}

SSAInstruction* IREmitter::VSlideDowni(SSAInstruction* value, u8 shift, VectorState state) {
    return insertInstruction(IROpcode::VSlideDowni, state, {value}, shift);
}

SSAInstruction* IREmitter::VSlideUpi(SSAInstruction* value, u8 shift, VectorState state) {
    return insertInstruction(IROpcode::VSlideUpi, state, {value}, shift);
}

SSAInstruction* IREmitter::VSlideUpZeroesi(SSAInstruction* value, u8 shift, VectorState state) {
    return insertInstruction(IROpcode::VSlideUpZeroesi, state, {value}, shift);
}

SSAInstruction* IREmitter::VSlide1Up(SSAInstruction* integer, SSAInstruction* vector, VectorState state) {
    return insertInstruction(IROpcode::VSlide1Up, state, {integer, vector});
}

SSAInstruction* IREmitter::VSlide1Down(SSAInstruction* integer, SSAInstruction* vector, VectorState state) {
    return insertInstruction(IROpcode::VSlide1Down, state, {integer, vector});
}

SSAInstruction* IREmitter::VFSqrt(SSAInstruction* value, VectorState state) {
    return insertInstruction(IROpcode::VFSqrt, state, {value});
}

SSAInstruction* IREmitter::VFRcp(SSAInstruction* value, VectorState state) {
    return insertInstruction(IROpcode::VFRcp, state, {value});
}

SSAInstruction* IREmitter::VFRcpSqrt(SSAInstruction* value, VectorState state) {
    return insertInstruction(IROpcode::VFRcpSqrt, state, {value});
}

SSAInstruction* IREmitter::VFMul(SSAInstruction* lhs, SSAInstruction* rhs, VectorState state) {
    return insertInstruction(IROpcode::VFMul, state, {lhs, rhs});
}

SSAInstruction* IREmitter::VFSub(SSAInstruction* lhs, SSAInstruction* rhs, VectorState state) {
    return insertInstruction(IROpcode::VFSub, state, {lhs, rhs});
}

SSAInstruction* IREmitter::VFDiv(SSAInstruction* lhs, SSAInstruction* rhs, VectorState state) {
    return insertInstruction(IROpcode::VFDiv, state, {lhs, rhs});
}

SSAInstruction* IREmitter::VZext(SSAInstruction* value, x86_size_e size) {
    switch (size) {
    case X86_SIZE_DWORD: {
        u8 element_count = SUPPORTED_VLEN / 32;
        u8 shift_count = element_count - 1;
        SSAInstruction* upped = VSlideUpi(value, shift_count, VectorState::PackedDWord);
        return VSlideDowni(upped, shift_count, VectorState::PackedDWord);
    }
    case X86_SIZE_QWORD: {
        u8 element_count = SUPPORTED_VLEN / 64;
        u8 shift_count = element_count - 1;
        SSAInstruction* upped = VSlideUpi(value, shift_count, VectorState::PackedQWord);
        return VSlideDowni(upped, shift_count, VectorState::PackedQWord);
    }
    default:
        UNREACHABLE();
        return nullptr;
    }
}

SSAInstruction* IREmitter::VEqual(SSAInstruction* lhs, SSAInstruction* rhs, VectorState state) {
    return insertInstruction(IROpcode::VEqual, state, {lhs, rhs});
}

SSAInstruction* IREmitter::VAdd(SSAInstruction* lhs, SSAInstruction* rhs, VectorState state) {
    return insertInstruction(IROpcode::VAdd, state, {lhs, rhs});
}

SSAInstruction* IREmitter::VAddi(SSAInstruction* lhs, i64 rhs, VectorState state) {
    return insertInstruction(IROpcode::VAddi, state, {lhs}, rhs);
}

SSAInstruction* IREmitter::VSub(SSAInstruction* lhs, SSAInstruction* rhs, VectorState state) {
    return insertInstruction(IROpcode::VSub, state, {lhs, rhs});
}

SSAInstruction* IREmitter::VAnd(SSAInstruction* lhs, SSAInstruction* rhs, VectorState state) {
    return insertInstruction(IROpcode::VAnd, state, {lhs, rhs});
}

SSAInstruction* IREmitter::VOr(SSAInstruction* lhs, SSAInstruction* rhs, VectorState state) {
    return insertInstruction(IROpcode::VOr, state, {lhs, rhs});
}

SSAInstruction* IREmitter::VXor(SSAInstruction* lhs, SSAInstruction* rhs, VectorState state) {
    return insertInstruction(IROpcode::VXor, state, {lhs, rhs});
}

SSAInstruction* IREmitter::VXori(SSAInstruction* lhs, i64 rhs, VectorState state) {
    return insertInstruction(IROpcode::VXori, state, {lhs}, rhs);
}

SSAInstruction* IREmitter::VSplat(SSAInstruction* value, VectorState state) {
    return insertInstruction(IROpcode::VSplat, state, {value});
}

SSAInstruction* IREmitter::VSplati(u64 imm, VectorState state) {
    return insertInstruction(IROpcode::VSplati, state, {}, imm);
}

SSAInstruction* IREmitter::VMerge(SSAInstruction* true_value, SSAInstruction* false_value, VectorState state) {
    SSAInstruction* instruction = insertInstruction(IROpcode::VMerge, state, {true_value, false_value});
    instruction->SetMasked();
    return instruction;
}

SSAInstruction* IREmitter::VMergei(u64 true_imm, SSAInstruction* false_value, VectorState state) {
    SSAInstruction* instruction = insertInstruction(IROpcode::VMergei, state, {false_value}, true_imm);
    instruction->SetMasked();
    return instruction;
}

SSAInstruction* IREmitter::IToV(SSAInstruction* value, VectorState state) {
    return insertInstruction(IROpcode::IToV, state, {value});
}

SSAInstruction* IREmitter::VToI(SSAInstruction* value, VectorState state) {
    return insertInstruction(IROpcode::VToI, state, {value});
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
        WARN("Accessing GS base register");
        SSAInstruction* gs = getGuest(X86_REF_GS);
        base_final = Add(base, gs);
    }

    SSAInstruction* address = base_final;
    if (index) {
        ASSERT(operand.memory.scale >= 0 && operand.memory.scale <= 3);
        if (Extensions::B || Extensions::Xtheadba) {
            address = AddShifted(base_final, index, operand.memory.scale);
        } else {
            SSAInstruction* scaled_index = Shli(index, operand.memory.scale);
            address = Add(base_final, scaled_index);
        }
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

SSAInstruction* IREmitter::IsNotZero(SSAInstruction* value, x86_size_e size) {
    return Snez(Zext(value, size));
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
        return Shri(value, 63);
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

SSAInstruction* IREmitter::insertInstruction(IROpcode opcode, VectorState state, std::initializer_list<SSAInstruction*> operands) {
    SSAInstruction instruction(opcode, operands);
    instruction.SetVectorState(state);
    return block->InsertAtEnd(std::move(instruction));
}

SSAInstruction* IREmitter::insertInstruction(IROpcode opcode, VectorState state, std::initializer_list<SSAInstruction*> operands, u64 imm) {
    SSAInstruction instruction(opcode, operands, imm);
    instruction.SetVectorState(state);
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
    SSAInstruction* new_value = Set8Low(old, value);
    setGuest(ref, new_value);
}

void IREmitter::setGpr8High(x86_ref_e ref, SSAInstruction* value) {
    if (ref < X86_REF_RAX || ref > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    SSAInstruction* old = getGpr64(ref);
    SSAInstruction* new_value = Set8High(old, value);
    setGuest(ref, new_value);
}

void IREmitter::setGpr16(x86_ref_e ref, SSAInstruction* value) {
    if (ref < X86_REF_RAX || ref > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    SSAInstruction* old = getGpr64(ref);
    SSAInstruction* new_value = Set16(old, value);
    setGuest(ref, new_value);
}

void IREmitter::setGpr32(x86_ref_e ref, SSAInstruction* value) {
    if (ref < X86_REF_RAX || ref > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    setGuest(ref, Set32(value));
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

SSAInstruction* IREmitter::readXmmWord(SSAInstruction* address, VectorState state) {
    if (state == VectorState::AnyPacked) {
        state = VectorState::PackedDWord;
    }

    // If our vlen and supported (target) vlen match, we can just do full load/stores
    if (Extensions::VLEN == SUPPORTED_VLEN && state != VectorState::Float && state != VectorState::Double) {
        // TODO: Needs testing with 256-bit vectors to make sure it doesn't break anything
        // state = VectorState::Null;
    }

    return insertInstruction(IROpcode::ReadXmmWord, state, {address});
}

SSAInstruction* IREmitter::ReadMemory(SSAInstruction* address, x86_size_e size, VectorState vector_state) {
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
        ASSERT(vector_state != VectorState::Null);
        return readXmmWord(address, vector_state);
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

void IREmitter::writeXmmWord(SSAInstruction* address, SSAInstruction* value, VectorState state) {
    if (state == VectorState::AnyPacked) {
        state = VectorState::PackedDWord;
    }

    // If our vlen and supported (target) vlen match, we can just do full load/stores
    if (Extensions::VLEN == SUPPORTED_VLEN && state != VectorState::Float && state != VectorState::Double) {
        // TODO: Needs testing with 256-bit vectors to make sure it doesn't break anything
        // state = VectorState::Null;
    }

    SSAInstruction* instruction = insertInstruction(IROpcode::WriteXmmWord, state, {address, value});
    instruction->Lock();
}

void IREmitter::WriteMemory(SSAInstruction* address, SSAInstruction* value, x86_size_e size, VectorState vector_state) {
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
        ASSERT(vector_state != VectorState::Null);
        return writeXmmWord(address, value, vector_state);
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

SSAInstruction* IREmitter::IsOverflowSbb(SSAInstruction* lhs, SSAInstruction* rhs, SSAInstruction* carry, SSAInstruction* result, x86_size_e size_e) {
    SSAInstruction* sum = Sub(lhs, rhs);
    SSAInstruction* of1 = IsOverflowSub(lhs, rhs, sum, size_e);
    SSAInstruction* of2 = IsOverflowSub(sum, carry, result, size_e);
    return Or(of1, of2);
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

SSAInstruction* IREmitter::IsAuxSbb(SSAInstruction* lhs, SSAInstruction* rhs, SSAInstruction* carry) {
    SSAInstruction* sum = Sub(lhs, rhs);
    SSAInstruction* and1 = IsAuxSub(lhs, rhs);
    SSAInstruction* and2 = IsAuxSub(sum, carry);

    return Or(and1, and2);
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
    ::Group1 opcode = (::Group1)((inst->operand_reg.reg.ref & 0x7) - X86_REF_RAX);

    x86_size_e size_e = inst->operand_rm.size;
    SSAInstruction* imm = Imm(ImmSext(inst->operand_imm.immediate.data, inst->operand_imm.size));
    SSAInstruction* result = nullptr;
    SSAInstruction* zero = Imm(0);
    SSAInstruction* c = zero;
    SSAInstruction* o = zero;
    SSAInstruction* a = nullptr;

    bool is_lock = inst->operand_rm.type == X86_OP_TYPE_MEMORY && inst->operand_rm.memory.lock;

    switch (opcode) {
    case Group1::Add: {
        SSAInstruction* rm;
        if (is_lock) {
            SSAInstruction* address = Lea(inst->operand_rm);
            rm = AmoAdd(address, imm, size_e);
            result = Add(rm, imm);
        } else {
            rm = GetRm(inst->operand_rm);
            result = Add(rm, imm);
        }
        c = IsCarryAdd(rm, result, size_e);
        o = IsOverflowAdd(rm, imm, result, size_e);
        a = IsAuxAdd(rm, imm);
        break;
    }
    case Group1::Adc: {
        SSAInstruction* carry_in = GetFlag(X86_REF_CF);
        SSAInstruction* imm_carry = Add(imm, carry_in);
        SSAInstruction* rm;
        if (is_lock) {
            SSAInstruction* address = Lea(inst->operand_rm);
            rm = AmoAdd(address, imm_carry, size_e);
            result = Add(rm, imm_carry);
        } else {
            rm = GetRm(inst->operand_rm);
            result = Add(rm, imm_carry);
        }
        c = IsCarryAdc(rm, imm, carry_in, size_e);
        o = IsOverflowAdd(rm, imm_carry, result, size_e);
        a = IsAuxAdd(rm, imm_carry);
        break;
    }
    case Group1::Sbb: {
        SSAInstruction* carry_in = GetFlag(X86_REF_CF);
        SSAInstruction* imm_carry = Add(imm, carry_in);
        SSAInstruction* rm;
        if (is_lock) {
            SSAInstruction* address = Lea(inst->operand_rm);
            rm = AmoAdd(address, Neg(imm_carry), size_e);
            result = Sub(rm, imm_carry);
        } else {
            rm = GetRm(inst->operand_rm);
            result = Sub(rm, imm_carry);
        }
        c = IsCarrySbb(rm, imm, carry_in, size_e);
        o = IsOverflowSub(rm, imm_carry, result, size_e);
        a = IsAuxSub(rm, imm_carry);
        break;
    }
    case Group1::Or: {
        SSAInstruction* rm;
        if (is_lock) {
            SSAInstruction* address = Lea(inst->operand_rm);
            rm = AmoOr(address, imm, size_e);
            result = Or(rm, imm);
        } else {
            rm = GetRm(inst->operand_rm);
            result = Or(rm, imm);
        }
        break;
    }
    case Group1::And: {
        SSAInstruction* rm;
        if (is_lock) {
            SSAInstruction* address = Lea(inst->operand_rm);
            rm = AmoAnd(address, imm, size_e);
            result = And(rm, imm);
        } else {
            rm = GetRm(inst->operand_rm);
            result = And(rm, imm);
        }
        break;
    }
    case Group1::Sub: {
        SSAInstruction* rm;
        if (is_lock) {
            SSAInstruction* address = Lea(inst->operand_rm);
            rm = AmoAdd(address, Neg(imm), size_e);
            result = Sub(rm, imm);
        } else {
            rm = GetRm(inst->operand_rm);
            result = Sub(rm, imm);
        }
        c = IsCarrySub(rm, imm);
        o = IsOverflowSub(rm, imm, result, size_e);
        a = IsAuxSub(rm, imm);
        break;
    }
    case Group1::Xor: {
        SSAInstruction* rm;
        if (is_lock) {
            SSAInstruction* address = Lea(inst->operand_rm);
            rm = AmoXor(address, imm, size_e);
            result = Xor(rm, imm);
        } else {
            rm = GetRm(inst->operand_rm);
            result = Xor(rm, imm);
        }
        break;
    }
    case Group1::Cmp: {
        SSAInstruction* rm = GetRm(inst->operand_rm);
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

    if (opcode != Group1::Cmp) {
        SetRm(inst->operand_rm, result);
    }
}

void IREmitter::Group2(x86_instruction_t* inst, SSAInstruction* shift_amount) {
    ::Group2 opcode = (::Group2)((inst->operand_reg.reg.ref & 0x7) - X86_REF_RAX);

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
    case Group2::Rol: {
        result = Rol(rm, shift_value, size_e);
        SSAInstruction* msb = IsNegative(result, size_e);
        c = Select(Seqz(shift_value), GetFlag(X86_REF_CF), Andi(result, 1));
        o = Select(Seqz(shift_value), GetFlag(X86_REF_OF), Xor(c, msb));
        break;
    }
    case Group2::Ror: {
        result = Ror(rm, shift_value, size_e);
        c = Select(Seqz(shift_value), GetFlag(X86_REF_CF), IsNegative(result, size_e));
        o = Select(Seqz(shift_value), GetFlag(X86_REF_OF), Xor(c, SecondMSB(*this, result, size_e)));
        break;
    }
    case Group2::Rcl: {
        ERROR("Why? :(");
        break;
    }
    case Group2::Rcr: {
        ERROR("Why? :(");
        break;
    }
    case Group2::Sal:
    case Group2::Shl: {
        SSAInstruction* shift = Sub(Imm(GetBitSize(size_e)), shift_value);
        SSAInstruction* msb_mask = Shl(Imm(1), shift);
        result = Shl(rm, shift_value);
        c = Equal(And(rm, msb_mask), msb_mask);
        SSAInstruction* sign = IsNegative(result, size_e);
        o = Xor(c, sign);
        break;
    }
    case Group2::Shr: {
        SSAInstruction* is_zero = Seqz(shift_value);
        SSAInstruction* shift = Addi(shift_value, -1);
        SSAInstruction* mask = Shl(Imm(1), shift);
        result = Shr(rm, shift_value);
        c = Select(is_zero, Imm(0), Equal(And(rm, mask), mask));
        o = IsNegative(rm, size_e);
        break;
    }
    case Group2::Sar: {
        // Shift left to place MSB to bit 63
        u8 anti_shift = 64 - GetBitSize(size_e);
        SSAInstruction* shifted_left = Shli(rm, anti_shift);
        SSAInstruction* shift_right = Addi(shift_value, anti_shift);
        SSAInstruction* is_zero = Seqz(shift_value);
        SSAInstruction* shift = Addi(shift_value, -1);
        SSAInstruction* mask = Shl(Imm(1), shift);
        result = Sar(shifted_left, shift_right);
        o = Imm(0);
        c = Select(is_zero, Imm(0), Equal(And(rm, mask), mask));
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
    ::Group3 opcode = (::Group3)((inst->operand_reg.reg.ref & 0x7) - X86_REF_RAX);

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
    case Group3::Test:
    case Group3::Test_: {
        SSAInstruction* imm = Imm(ImmSext(inst->operand_imm.immediate.data, inst->operand_imm.size));
        SSAInstruction* masked = And(rm, imm);
        s = IsNegative(masked, size_e);
        z = IsZero(masked, size_e);
        p = Parity(masked);
        break;
    }
    case Group3::Not: {
        result = Not(rm);
        break;
    }
    case Group3::Neg: {
        result = Neg(rm);
        z = IsZero(result, size_e);
        c = Seqz(z);
        s = IsNegative(result, size_e);
        o = IsOverflowSub(Imm(0), rm, result, size_e);
        a = IsAuxSub(Imm(0), rm);
        p = Parity(result);
        break;
    }
    case Group3::Mul: {
        switch (size_e) {
        case X86_SIZE_BYTE: {
            SSAInstruction* al = Zext(GetReg(X86_REF_RAX, X86_SIZE_BYTE), X86_SIZE_BYTE);
            SSAInstruction* se_rm = Zext(rm, X86_SIZE_BYTE);
            SSAInstruction* mul = Mul(al, se_rm);
            SetReg(mul, X86_REF_RAX, X86_SIZE_WORD);
            break;
        }
        case X86_SIZE_WORD: {
            SSAInstruction* ax = Zext(GetReg(X86_REF_RAX, X86_SIZE_WORD), X86_SIZE_WORD);
            SSAInstruction* se_rm = Zext(rm, X86_SIZE_WORD);
            SSAInstruction* mul = Mul(ax, se_rm);
            SSAInstruction* mul_high = Shri(mul, 16);
            SetReg(mul, X86_REF_RAX, X86_SIZE_WORD);
            SetReg(mul_high, X86_REF_RDX, X86_SIZE_WORD);
            break;
        }
        case X86_SIZE_DWORD: {
            SSAInstruction* eax = Zext(GetReg(X86_REF_RAX, X86_SIZE_DWORD), X86_SIZE_DWORD);
            SSAInstruction* se_rm = Zext(rm, X86_SIZE_DWORD);
            SSAInstruction* mul = Mul(eax, se_rm);
            SSAInstruction* mul_high = Shri(mul, 32);
            SetReg(mul, X86_REF_RAX, X86_SIZE_DWORD);
            SetReg(mul_high, X86_REF_RDX, X86_SIZE_DWORD);
            break;
        }
        case X86_SIZE_QWORD: {
            SSAInstruction* rax = GetReg(X86_REF_RAX, X86_SIZE_QWORD);
            SSAInstruction* mul = Mul(rax, rm);
            SSAInstruction* mul_high = Mulhu(rax, rm);
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
    case Group3::IMul: {
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
    case Group3::Div: {
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

            SSAInstruction* instruction = insertInstruction(IROpcode::Divu128, {rm});
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
    case Group3::IDiv: {
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

void IREmitter::Group14(x86_instruction_t* inst) {
    ::Group14 opcode = (::Group14)((inst->operand_reg.reg.ref & 0x7) - X86_REF_RAX);
    ASSERT(inst->operand_rm.type == X86_OP_TYPE_REGISTER);
    switch (opcode) {
    case Group14::PSrlQ: {
        u8 shift = inst->operand_imm.immediate.data & 0x3F;
        SSAInstruction* reg = GetRm(inst->operand_rm);
        SSAInstruction* shifted;
        if (shift > 31) {
            shifted = VSrl(reg, Imm(shift), VectorState::PackedQWord);
        } else {
            shifted = VSrli(reg, shift, VectorState::PackedQWord);
        }
        SetRm(inst->operand_rm, shifted);
        break;
    }
    case Group14::PSllQ: {
        u8 shift = inst->operand_imm.immediate.data & 0x3F;
        SSAInstruction* reg = GetRm(inst->operand_rm);
        SSAInstruction* shifted;
        if (shift > 31) {
            shifted = VSll(reg, Imm(shift), VectorState::PackedQWord);
        } else {
            shifted = VSlli(reg, shift, VectorState::PackedQWord);
        }
        SetRm(inst->operand_rm, shifted);
        break;
    }
    case Group14::PSrlDQ: {
        u8 shift = inst->operand_imm.immediate.data & 0x3F;
        if (shift > 15)
            shift = 16;
        SSAInstruction* reg = GetRm(inst->operand_rm);
        SSAInstruction* shifted = VSlideDowni(reg, shift, VectorState::PackedByte);
        SetRm(inst->operand_rm, shifted);
        break;
    }
    case Group14::PSllDQ: {
        u8 shift = inst->operand_imm.immediate.data & 0x3F;
        if (shift > 15)
            shift = 16;
        SSAInstruction* reg = GetRm(inst->operand_rm);
        SSAInstruction* shifted = VSlideUpZeroesi(reg, shift, VectorState::PackedByte);
        SetRm(inst->operand_rm, shifted);
        break;
    }
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

    SSAInstruction* final_condition = Snez(Or(rcx_zero, condition));
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

SSAInstruction* IREmitter::Set8High(SSAInstruction* old, SSAInstruction* value) {
    SSAInstruction* masked_old = And(old, Imm(0xFFFFFFFFFFFF00FF));
    SSAInstruction* masked_value = Zext(value, X86_SIZE_BYTE);
    SSAInstruction* shifted_value = Shli(masked_value, 8);
    SSAInstruction* new_value = Or(masked_old, shifted_value);
    return new_value;
}

SSAInstruction* IREmitter::Set8Low(SSAInstruction* old, SSAInstruction* value) {
    SSAInstruction* masked_old = Andi(old, 0xFFFFFFFFFFFFFF00);
    SSAInstruction* masked_value = Zext(value, X86_SIZE_BYTE);
    SSAInstruction* new_value = Or(masked_old, masked_value);
    return new_value;
}

SSAInstruction* IREmitter::Set16(SSAInstruction* old, SSAInstruction* value) {
    SSAInstruction* masked_old = And(old, Imm(0xFFFFFFFFFFFF0000));
    SSAInstruction* masked_value = Zext(value, X86_SIZE_WORD);
    SSAInstruction* new_value = Or(masked_old, masked_value);
    return new_value;
}

SSAInstruction* IREmitter::Set32(SSAInstruction* value) {
    return Zext(value, X86_SIZE_DWORD);
}

SSAInstruction* IREmitter::Set(SSAInstruction* old, SSAInstruction* value, x86_size_e size_e, bool high) {
    switch (size_e) {
    case X86_SIZE_BYTE:
        return high ? Set8High(old, value) : Set8Low(old, value);
    case X86_SIZE_WORD:
        return Set16(old, value);
    case X86_SIZE_DWORD:
        return Set32(value);
    case X86_SIZE_QWORD:
        return value;
    default:
        ERROR("Invalid size");
        return nullptr;
    }
}