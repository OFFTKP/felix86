#include "felix86/ir/passes/passes.hpp"

namespace {
bool IsImmediate(const SSAInstruction* inst, u64 imm) {
    return inst->IsImmediate() && inst->GetImmediateData() == imm;
}

bool IsInteger64(const SSAInstruction* inst) {
    return !inst->IsImmediate() && inst->GetType() == IRType::Integer64;
}

IROpcode ReadToRelative(IROpcode read) {
    switch (read) {
    case IROpcode::ReadByte: {
        return IROpcode::ReadByteRelative;
    }
    case IROpcode::ReadWord: {
        return IROpcode::ReadWordRelative;
    }
    case IROpcode::ReadDWord: {
        return IROpcode::ReadDWordRelative;
    }
    case IROpcode::ReadQWord: {
        return IROpcode::ReadQWordRelative;
    }
    default: {
        UNREACHABLE();
        return IROpcode::Null;
    }
    }
}

IROpcode WriteToRelative(IROpcode write) {
    switch (write) {
    case IROpcode::WriteByte: {
        return IROpcode::WriteByteRelative;
    }
    case IROpcode::WriteWord: {
        return IROpcode::WriteWordRelative;
    }
    case IROpcode::WriteDWord: {
        return IROpcode::WriteDWordRelative;
    }
    case IROpcode::WriteQWord: {
        return IROpcode::WriteQWordRelative;
    }
    default: {
        UNREACHABLE();
        return IROpcode::Null;
    }
    }
}

// t2 = imm + imm
bool PeepholeAddImmediates(SSAInstruction& inst) {
    const SSAInstruction* op1 = inst.GetOperand(0);
    const SSAInstruction* op2 = inst.GetOperand(1);

    if (op1->IsImmediate() && op2->IsImmediate()) {
        inst.ReplaceWithImmediate(op1->GetImmediateData() + op2->GetImmediateData());
        return true;
    }

    return false;
}

// t2 = t1 + 0
bool PeepholeAddZero(SSAInstruction& inst) {
    for (u8 i = 0; i < 2; i++) {
        if (IsImmediate(inst.GetOperand(i), 0) && IsInteger64(inst.GetOperand(!i))) {
            inst.ReplaceWithMov(inst.GetOperand(!i));
            return true;
        }
    }

    return false;
}

// t2 = imm & imm
bool PeepholeAndImmediates(SSAInstruction& inst) {
    const SSAInstruction* op1 = inst.GetOperand(0);
    const SSAInstruction* op2 = inst.GetOperand(1);

    if (op1->IsImmediate() && op2->IsImmediate()) {
        inst.ReplaceWithImmediate(op1->GetImmediateData() & op2->GetImmediateData());
        return true;
    }

    return false;
}

// t2 = t1 & t1
bool PeepholeAndSame(SSAInstruction& inst) {
    SSAInstruction* op1 = inst.GetOperand(0);
    SSAInstruction* op2 = inst.GetOperand(1);

    if (op1 == op2) {
        inst.ReplaceWithMov(op1);
        return true;
    }

    return false;
}

// One of the operands masks something with the same mask
// such as:
// t4 = t3 & t2
// t5 = t4 & t2
// then t5 can be replaced with t5 = t4
bool PeepholeAndTwice(SSAInstruction& inst) {
    for (u8 i = 0; i < 2; i++) {
        SSAInstruction* operand = inst.GetOperand(i);
        const SSAInstruction* other = inst.GetOperand(!i);
        if (operand->GetOpcode() == IROpcode::And) {
            for (u8 i = 0; i < 2; i++) {
                if (operand->GetOperand(i) == other) {
                    inst.ReplaceWithMov(operand);
                    return true;
                }
            }
        }
    }

    return false;
}

// t2 = t1 & 0
bool PeepholeAndZero(SSAInstruction& inst) {
    for (u8 i = 0; i < 2; i++) {
        if (IsImmediate(inst.GetOperand(i), 0)) {
            inst.ReplaceWithImmediate(0);
            return true;
        }
    }

    return false;
}

// t2 = t1 / 1
bool PeepholeDivOne(SSAInstruction& inst) {
    SSAInstruction* right = inst.GetOperand(1);

    if (IsImmediate(right, 1)) {
        inst.ReplaceWithMov(inst.GetOperand(0));
        return true;
    }

    return false;
}

// t2 = imm / imm
bool PeepholeDivImmediates(SSAInstruction& inst) {
    const SSAInstruction* op1 = inst.GetOperand(0);
    const SSAInstruction* op2 = inst.GetOperand(1);

    if (op1->IsImmediate() && op2->IsImmediate()) {
        ASSERT(op2->GetImmediateData() != 0);
        inst.ReplaceWithImmediate((i64)op1->GetImmediateData() / (i64)op2->GetImmediateData());
        return true;
    }

    return false;
}

// t2 = imm / imm
bool PeepholeDivuImmediates(SSAInstruction& inst) {
    const SSAInstruction* op1 = inst.GetOperand(0);
    const SSAInstruction* op2 = inst.GetOperand(1);

    if (op1->IsImmediate() && op2->IsImmediate()) {
        ASSERT(op2->GetImmediateData() != 0);
        inst.ReplaceWithImmediate(op1->GetImmediateData() / op2->GetImmediateData());
        return true;
    }

    return false;
}

// t2 = imm == imm
bool PeepholeEqualImmediates(SSAInstruction& inst) {
    const SSAInstruction* op1 = inst.GetOperand(0);
    const SSAInstruction* op2 = inst.GetOperand(1);

    if (op1->IsImmediate() && op2->IsImmediate()) {
        inst.ReplaceWithImmediate(op1->GetImmediateData() == op2->GetImmediateData());
        return true;
    }

    return false;
}

// t2 = t1 == t1
bool PeepholeEqualSame(SSAInstruction& inst) {
    SSAInstruction* op1 = inst.GetOperand(0);
    SSAInstruction* op2 = inst.GetOperand(1);

    if (op1 == op2) {
        inst.ReplaceWithImmediate(1);
        return true;
    }

    return false;
}

// t2 = imm < imm
bool PeepholeLessThanSignedImmediates(SSAInstruction& inst) {
    const SSAInstruction* op1 = inst.GetOperand(0);
    const SSAInstruction* op2 = inst.GetOperand(1);

    if (op1->IsImmediate() && op2->IsImmediate()) {
        inst.ReplaceWithImmediate((i64)op1->GetImmediateData() < (i64)op2->GetImmediateData());
        return true;
    }

    return false;
}

// t2 = imm < imm
bool PeepholeLessThanUnsignedImmediates(SSAInstruction& inst) {
    const SSAInstruction* op1 = inst.GetOperand(0);
    const SSAInstruction* op2 = inst.GetOperand(1);

    if (op1->IsImmediate() && op2->IsImmediate()) {
        inst.ReplaceWithImmediate(op1->GetImmediateData() < op2->GetImmediateData());
        return true;
    }

    return false;
}

// t2 = t1 < t1
bool PeepholeLessThanSame(SSAInstruction& inst) {
    SSAInstruction* op1 = inst.GetOperand(0);
    SSAInstruction* op2 = inst.GetOperand(1);

    if (op1 == op2) {
        inst.ReplaceWithImmediate(0);
        return true;
    }

    return false;
}

// t2 = imm * imm
bool PeepholeMulImmediates(SSAInstruction& inst) {
    const SSAInstruction* op1 = inst.GetOperand(0);
    const SSAInstruction* op2 = inst.GetOperand(1);

    if (op1->IsImmediate() && op2->IsImmediate()) {
        inst.ReplaceWithImmediate(op1->GetImmediateData() * op2->GetImmediateData());
        return true;
    }

    return false;
}

// t2 = -imm
bool PeepholeNegImmediate(SSAInstruction& inst) {
    const SSAInstruction* op1 = inst.GetOperand(0);

    if (op1->IsImmediate()) {
        inst.ReplaceWithImmediate(-op1->GetImmediateData());
        return true;
    }

    return false;
}

// t2 = imm != imm
bool PeepholeNotEqualImmediates(SSAInstruction& inst) {
    const SSAInstruction* op1 = inst.GetOperand(0);
    const SSAInstruction* op2 = inst.GetOperand(1);

    if (op1->IsImmediate() && op2->IsImmediate()) {
        inst.ReplaceWithImmediate(op1->GetImmediateData() != op2->GetImmediateData());
        return true;
    }

    return false;
}

// t2 = t1 != t1
bool PeepholeNotEqualSame(SSAInstruction& inst) {
    SSAInstruction* op1 = inst.GetOperand(0);
    SSAInstruction* op2 = inst.GetOperand(1);

    if (op1 == op2) {
        inst.ReplaceWithImmediate(0);
        return true;
    }

    return false;
}

// t2 = ~imm
bool PeepholeNotImmediate(SSAInstruction& inst) {
    const SSAInstruction* op1 = inst.GetOperand(0);

    if (op1->IsImmediate()) {
        inst.ReplaceWithImmediate(~op1->GetImmediateData());
        return true;
    }

    return false;
}

// t2 = imm | imm
bool PeepholeOrImmediates(SSAInstruction& inst) {
    const SSAInstruction* op1 = inst.GetOperand(0);
    const SSAInstruction* op2 = inst.GetOperand(1);

    if (op1->IsImmediate() && op2->IsImmediate()) {
        inst.ReplaceWithImmediate(op1->GetImmediateData() | op2->GetImmediateData());
        return true;
    }

    return false;
}

// t2 = t1 | t1
bool PeepholeOrSame(SSAInstruction& inst) {
    SSAInstruction* op1 = inst.GetOperand(0);
    SSAInstruction* op2 = inst.GetOperand(1);

    if (op1 == op2) {
        inst.ReplaceWithMov(op1);
        return true;
    }

    return false;
}

// t2 = t1 | 0
bool PeepholeOrZero(SSAInstruction& inst) {
    for (u8 i = 0; i < 2; i++) {
        if (IsImmediate(inst.GetOperand(i), 0) && IsInteger64(inst.GetOperand(!i))) {
            inst.ReplaceWithMov(inst.GetOperand(!i));
            return true;
        }
    }

    return false;
}

// t2 = imm % imm
bool PeepholeRemImmediates(SSAInstruction& inst) {
    const SSAInstruction* op1 = inst.GetOperand(0);
    const SSAInstruction* op2 = inst.GetOperand(1);

    if (op1->IsImmediate() && op2->IsImmediate()) {
        ASSERT(op2->GetImmediateData() != 0);
        inst.ReplaceWithImmediate((i64)op1->GetImmediateData() % (i64)op2->GetImmediateData());
        return true;
    }

    return false;
}

// t2 = imm % imm
bool PeepholeRemuImmediates(SSAInstruction& inst) {
    const SSAInstruction* op1 = inst.GetOperand(0);
    const SSAInstruction* op2 = inst.GetOperand(1);

    if (op1->IsImmediate() && op2->IsImmediate()) {
        ASSERT(op2->GetImmediateData() != 0);
        inst.ReplaceWithImmediate(op1->GetImmediateData() % op2->GetImmediateData());
        return true;
    }

    return false;
}

// t2 = imm << imm
bool PeepholeShiftLeftImmediates(SSAInstruction& inst) {
    const SSAInstruction* op1 = inst.GetOperand(0);
    const SSAInstruction* op2 = inst.GetOperand(1);

    if (op1->IsImmediate() && op2->IsImmediate()) {
        inst.ReplaceWithImmediate(op1->GetImmediateData() << op2->GetImmediateData());
        return true;
    }

    return false;
}

// t2 = imm >> imm
bool PeepholeShiftRightArithmeticImmediates(SSAInstruction& inst) {
    const SSAInstruction* op1 = inst.GetOperand(0);
    const SSAInstruction* op2 = inst.GetOperand(1);

    if (op1->IsImmediate() && op2->IsImmediate()) {
        inst.ReplaceWithImmediate((i64)op1->GetImmediateData() >> op2->GetImmediateData());
        return true;
    }

    return false;
}

// t2 = imm >> imm
bool PeepholeShiftRightImmediates(SSAInstruction& inst) {
    const SSAInstruction* op1 = inst.GetOperand(0);
    const SSAInstruction* op2 = inst.GetOperand(1);

    if (op1->IsImmediate() && op2->IsImmediate()) {
        inst.ReplaceWithImmediate(op1->GetImmediateData() >> op2->GetImmediateData());
        return true;
    }

    return false;
}

// t2 = t1 + imm
// store some_val, (t2)
bool PeepholeWriteRelative(SSAInstruction& inst) {
    SSAInstruction* address = inst.GetOperand(0);

    switch (address->GetOpcode()) {
    case IROpcode::Add: {
        for (u8 i = 0; i < 2; i++) {
            SSAInstruction* imm = address->GetOperand(i);
            SSAInstruction* base = address->GetOperand(!i);
            if (imm->IsImmediate() && IsValidSigned12BitImm(imm->GetImmediateData())) {
                Operands op;
                op.operands[0] = base;
                op.operands[1] = inst.GetOperand(1);
                op.operand_count = 2;
                op.immediate_data = imm->GetImmediateData();
                inst.Replace(op, WriteToRelative(inst.GetOpcode()));
                return true;
            }
        }
        break;
    }
    case IROpcode::Sub: {
        SSAInstruction* base = address->GetOperand(0);
        SSAInstruction* imm = address->GetOperand(1);
        if (imm->IsImmediate() && IsValidSigned12BitImm(-imm->GetImmediateData())) {
            Operands op;
            op.operands[0] = base;
            op.operands[1] = inst.GetOperand(1);
            op.operand_count = 2;
            op.immediate_data = -imm->GetImmediateData();
            inst.Replace(op, WriteToRelative(inst.GetOpcode()));
            return true;
        }
        break;
    }
    default:
        break;
    }

    return false;
}

// t2 = t1 + imm
// read (t2)
bool PeepholeReadRelative(SSAInstruction& inst) {
    SSAInstruction* address = inst.GetOperand(0);

    switch (address->GetOpcode()) {
    case IROpcode::Add: {
        for (u8 i = 0; i < 2; i++) {
            SSAInstruction* imm = address->GetOperand(i);
            SSAInstruction* base = address->GetOperand(!i);
            if (imm->IsImmediate() && IsValidSigned12BitImm(imm->GetImmediateData())) {
                Operands op;
                op.operands[0] = base;
                op.operand_count = 1;
                op.immediate_data = imm->GetImmediateData();
                inst.Replace(op, WriteToRelative(inst.GetOpcode()));
                return true;
            }
        }
        break;
    }
    case IROpcode::Sub: {
        SSAInstruction* base = address->GetOperand(0);
        SSAInstruction* imm = address->GetOperand(1);
        if (imm->IsImmediate() && IsValidSigned12BitImm(-imm->GetImmediateData())) {
            Operands op;
            op.operands[0] = base;
            op.operand_count = 1;
            op.immediate_data = -imm->GetImmediateData();
            inst.Replace(op, ReadToRelative(inst.GetOpcode()));
            return true;
        }
        break;
    }
    default:
        break;
    }

    return false;
}

// t2 = imm - imm
bool PeepholeSubImmediates(SSAInstruction& inst) {
    const SSAInstruction* op1 = inst.GetOperand(0);
    const SSAInstruction* op2 = inst.GetOperand(1);

    if (op1->IsImmediate() && op2->IsImmediate()) {
        inst.ReplaceWithImmediate(op1->GetImmediateData() - op2->GetImmediateData());
        return true;
    }

    return false;
}

// t2 = t1 - t1
bool PeepholeSubSame(SSAInstruction& inst) {
    SSAInstruction* op1 = inst.GetOperand(0);
    SSAInstruction* op2 = inst.GetOperand(1);

    if (op1 == op2) {
        inst.ReplaceWithImmediate(0);
        return true;
    }

    return false;
}

// t2 = t1 - 0
bool PeepholeSubZero(SSAInstruction& inst) {
    SSAInstruction* op1 = inst.GetOperand(0);
    SSAInstruction* op2 = inst.GetOperand(1);

    if (IsImmediate(op2, 0)) {
        inst.ReplaceWithMov(op1);
        return true;
    }

    return false;
}

// t2 = imm ^ imm
bool PeepholeXorImmediates(SSAInstruction& inst) {
    const SSAInstruction* op1 = inst.GetOperand(0);
    const SSAInstruction* op2 = inst.GetOperand(1);

    if (op1->IsImmediate() && op2->IsImmediate()) {
        inst.ReplaceWithImmediate(op1->GetImmediateData() ^ op2->GetImmediateData());
        return true;
    }

    return false;
}

// t2 = t1 ^ t1
bool PeepholeXorSame(SSAInstruction& inst) {
    SSAInstruction* op1 = inst.GetOperand(0);
    SSAInstruction* op2 = inst.GetOperand(1);

    if (op1 == op2) {
        inst.ReplaceWithImmediate(0);
        return true;
    }

    return false;
}

// t2 = t1 ^ 0
bool PeepholeXorZero(SSAInstruction& inst) {
    for (u8 i = 0; i < 2; i++) {
        if (IsImmediate(inst.GetOperand(i), 0) && IsInteger64(inst.GetOperand(!i))) {
            inst.ReplaceWithMov(inst.GetOperand(!i));
            return true;
        }
    }

    return false;
}
}; // namespace

bool PassManager::peepholePassBlock(IRBlock* block) {
    bool changed = false;
    for (SSAInstruction& inst : block->GetInstructions()) {
        if (!inst.IsLocked()) {
            bool local_changed = false;
#define CHECK(x)                                                                                                                                     \
    if (!local_changed)                                                                                                                              \
    local_changed |= x(inst)
            switch (inst.GetOpcode()) {
            case IROpcode::Add: {
                CHECK(PeepholeAddImmediates);
                CHECK(PeepholeAddZero);
                break;
            }
            case IROpcode::And: {
                CHECK(PeepholeAndImmediates);
                CHECK(PeepholeAndZero);
                CHECK(PeepholeAndTwice);
                CHECK(PeepholeAndSame);
                break;
            }
            case IROpcode::Or: {
                CHECK(PeepholeOrImmediates);
                CHECK(PeepholeOrZero);
                CHECK(PeepholeOrSame);
                break;
            }
            case IROpcode::Xor: {
                CHECK(PeepholeXorImmediates);
                CHECK(PeepholeXorZero);
                CHECK(PeepholeXorSame);
                break;
            }
            case IROpcode::Div: {
                CHECK(PeepholeDivOne);
                CHECK(PeepholeDivImmediates);
                break;
            }
            case IROpcode::Divu: {
                CHECK(PeepholeDivOne);
                CHECK(PeepholeDivuImmediates);
                break;
            }
            case IROpcode::Equal: {
                CHECK(PeepholeEqualImmediates);
                CHECK(PeepholeEqualSame);
                break;
            }
            case IROpcode::SetLessThanSigned: {
                CHECK(PeepholeLessThanSignedImmediates);
                CHECK(PeepholeLessThanSame);
                break;
            }
            case IROpcode::SetLessThanUnsigned: {
                CHECK(PeepholeLessThanUnsignedImmediates);
                CHECK(PeepholeLessThanSame);
                break;
            }
            case IROpcode::Mul: {
                CHECK(PeepholeMulImmediates);
                break;
            }
            case IROpcode::Neg: {
                CHECK(PeepholeNegImmediate);
                break;
            }
            case IROpcode::Not: {
                CHECK(PeepholeNotImmediate);
                break;
            }
            case IROpcode::NotEqual: {
                CHECK(PeepholeNotEqualImmediates);
                CHECK(PeepholeNotEqualSame);
                break;
            }
            case IROpcode::ReadByte:
            case IROpcode::ReadWord:
            case IROpcode::ReadDWord:
            case IROpcode::ReadQWord: {
                CHECK(PeepholeReadRelative);
                break;
            }
            case IROpcode::Rem: {
                CHECK(PeepholeRemImmediates);
                break;
            }
            case IROpcode::Remu: {
                CHECK(PeepholeRemuImmediates);
                break;
            }
            case IROpcode::ShiftLeft: {
                CHECK(PeepholeShiftLeftImmediates);
                break;
            }
            case IROpcode::ShiftRight: {
                CHECK(PeepholeShiftRightImmediates);
                break;
            }
            case IROpcode::ShiftRightArithmetic: {
                CHECK(PeepholeShiftRightArithmeticImmediates);
                break;
            }
            case IROpcode::Sub: {
                CHECK(PeepholeSubImmediates);
                CHECK(PeepholeSubSame);
                CHECK(PeepholeSubZero);
                break;
            }
            case IROpcode::WriteByte:
            case IROpcode::WriteWord:
            case IROpcode::WriteDWord:
            case IROpcode::WriteQWord: {
                CHECK(PeepholeWriteRelative);
                break;
            }
            default:
                break;
            }

            changed |= local_changed;
        }
    }

    return changed;
}

bool PassManager::PeepholePass(IRFunction* function) {
    bool changed = false;

    for (IRBlock* block : function->GetBlocks()) {
        changed |= peepholePassBlock(block);
    }

    return changed;
}