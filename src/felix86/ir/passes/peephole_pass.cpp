#include "felix86/ir/passes/passes.hpp"

namespace {
bool IsImmediate(const SSAInstruction* inst, u64 imm) {
    return inst->IsImmediate() && inst->GetImmediateData() == imm;
}

bool IsInteger64(const SSAInstruction* inst) {
    return !inst->IsImmediate() && inst->GetType() == IRType::Integer64;
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
        if (IsImmediate(inst.GetOperand(i), 0) && IsInteger64(inst.GetOperand(~i))) {
            inst.ReplaceWithMov(inst.GetOperand(~i));
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

// One of the operands masks something with the same mask
// such as:
// t4 = t3 & t2
// t5 = t4 & t2
// then t5 can be replaced with t5 = t4
bool PeepholeAndTwice(SSAInstruction& inst) {
    for (u8 i = 0; i < 2; i++) {
        SSAInstruction* operand = inst.GetOperand(i);
        const SSAInstruction* other = inst.GetOperand(~i);
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
            return false;
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
}; // namespace

bool PassManager::PeepholePass(IRFunction* function) {
    bool changed = false;

    for (IRBlock* block : function->GetBlocks()) {
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
                    break;
                }
                case IROpcode::Or: {
                    CHECK(PeepholeOrImmediates);
                    break;
                }
                case IROpcode::Xor: {
                    CHECK(PeepholeXorImmediates);
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
                    break;
                }
                default:
                    break;
                }

                changed |= local_changed;
            }
        }
    }

    return changed;
}