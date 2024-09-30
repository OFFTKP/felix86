#include "felix86/common/log.hpp"
#include "felix86/ir/instruction.hpp"

bool IRInstruction::IsSameExpression(const IRInstruction& other) const {
    if (expression.index() != other.expression.index()) {
        return false;
    }

    switch (expression.index()) {
    case 0: {
        const Operands& operands = std::get<Operands>(expression);
        const Operands& other_operands = std::get<Operands>(other.expression);

        for (u8 i = 0; i < 6; i++) {
            if (operands.operands[i] != other_operands.operands[i]) {
                return false;
            }
        }

        if (operands.extra_data != other_operands.extra_data) {
            return false;
        }

        return true;
    }
    case 1: {
        const Immediate& immediate = std::get<Immediate>(expression);
        const Immediate& other_immediate = std::get<Immediate>(other.expression);

        return immediate.immediate == other_immediate.immediate;
    }
    case 2: {
        const GetGuest& get_guest = std::get<GetGuest>(expression);
        const GetGuest& other_get_guest = std::get<GetGuest>(other.expression);

        return get_guest.ref == other_get_guest.ref;
    }
    case 3: {
        const SetGuest& set_guest = std::get<SetGuest>(expression);
        const SetGuest& other_set_guest = std::get<SetGuest>(other.expression);

        return set_guest.ref == other_set_guest.ref && set_guest.source == other_set_guest.source;
    }
    case 4: {
        const Phi& phi = std::get<Phi>(expression);
        const Phi& other_phi = std::get<Phi>(other.expression);

        if (phi.ref != other_phi.ref) {
            return false;
        }

        if (phi.nodes.size() != other_phi.nodes.size()) {
            return false;
        }

        for (u8 i = 0; i < phi.nodes.size(); i++) {
            if (phi.nodes[i].block != other_phi.nodes[i].block || phi.nodes[i].value != other_phi.nodes[i].value) {
                return false;
            }
        }

        return true;
    }
    case 5: {
        const Comment& comment = std::get<Comment>(expression);
        const Comment& other_comment = std::get<Comment>(other.expression);

        return comment.comment == other_comment.comment;
    }
    case 6: {
        const TupleAccess& tuple_get = std::get<TupleAccess>(expression);
        const TupleAccess& other_tuple_get = std::get<TupleAccess>(other.expression);

        return tuple_get.tuple == other_tuple_get.tuple && tuple_get.index == other_tuple_get.index;
    }
    default:
        ERROR("Invalid expression type");
        return false;
    }
}

IRType IRInstruction::getTypeFromOpcode(IROpcode opcode, x86_ref_e ref) {
    switch (opcode) {
    case IROpcode::TupleExtract:
    case IROpcode::Mov: {
        ERROR("Should not be used in GetTypeFromOpcode");
        return IRType::Void;
    }
    case IROpcode::Null:
    case IROpcode::Comment: {
        return IRType::Void;
    }
    case IROpcode::Select:
    case IROpcode::Immediate:
    case IROpcode::Popcount:
    case IROpcode::Add:
    case IROpcode::Sub:
    case IROpcode::Clz:
    case IROpcode::Ctz:
    case IROpcode::ShiftLeft:
    case IROpcode::ShiftRight:
    case IROpcode::ShiftRightArithmetic:
    case IROpcode::LeftRotate8:
    case IROpcode::LeftRotate16:
    case IROpcode::LeftRotate32:
    case IROpcode::LeftRotate64:
    case IROpcode::And:
    case IROpcode::Or:
    case IROpcode::Xor:
    case IROpcode::Not:
    case IROpcode::Lea:
    case IROpcode::Equal:
    case IROpcode::NotEqual:
    case IROpcode::IGreaterThan:
    case IROpcode::ILessThan:
    case IROpcode::UGreaterThan:
    case IROpcode::ULessThan:
    case IROpcode::ReadByte:
    case IROpcode::ReadWord:
    case IROpcode::ReadDWord:
    case IROpcode::ReadQWord:
    case IROpcode::Syscall:
    case IROpcode::CastVectorToInteger:
    case IROpcode::VExtractInteger:
    case IROpcode::Sext8:
    case IROpcode::Sext16:
    case IROpcode::Sext32: {
        return IRType::Integer64;
    }
    case IROpcode::IMul64:
    case IROpcode::IDiv8:
    case IROpcode::IDiv16:
    case IROpcode::IDiv32:
    case IROpcode::IDiv64:
    case IROpcode::UDiv8:
    case IROpcode::UDiv16:
    case IROpcode::UDiv32:
    case IROpcode::UDiv64:
    case IROpcode::Rdtsc: {
        return IRType::TupleTwoInteger64;
    }
    case IROpcode::Cpuid: {
        return IRType::TupleFourInteger64;
    }
    case IROpcode::ReadXmmWord:
    case IROpcode::CastIntegerToVector:
    case IROpcode::VUnpackByteLow:
    case IROpcode::VUnpackWordLow:
    case IROpcode::VUnpackDWordLow:
    case IROpcode::VUnpackQWordLow:
    case IROpcode::VAnd:
    case IROpcode::VOr:
    case IROpcode::VXor:
    case IROpcode::VShr:
    case IROpcode::VShl:
    case IROpcode::VPackedSubByte:
    case IROpcode::VPackedAddQWord:
    case IROpcode::VPackedEqualByte:
    case IROpcode::VPackedEqualWord:
    case IROpcode::VPackedEqualDWord:
    case IROpcode::VPackedShuffleDWord:
    case IROpcode::VMoveByteMask:
    case IROpcode::VPackedMinByte:
    case IROpcode::VZext64:
    case IROpcode::VInsertInteger: {
        return IRType::Vector128;
    }
    case IROpcode::WriteByte:
    case IROpcode::WriteWord:
    case IROpcode::WriteDWord:
    case IROpcode::WriteQWord:
    case IROpcode::WriteXmmWord: {
        return IRType::Void;
    }

    case IROpcode::Phi:
    case IROpcode::GetGuest:
    case IROpcode::SetGuest:
    case IROpcode::LoadGuestFromMemory:
    case IROpcode::StoreGuestToMemory: {
        switch (ref) {
        case X86_REF_RAX ... X86_REF_R15:
        case X86_REF_RIP:
        case X86_REF_GS:
        case X86_REF_FS:
            return IRType::Integer64;
        case X86_REF_ST0 ... X86_REF_ST7:
            return IRType::Float80;
        case X86_REF_CF ... X86_REF_OF:
            return IRType::Integer64;
        case X86_REF_XMM0 ... X86_REF_XMM15:
            return IRType::Vector128;
        default:
            ERROR("Invalid register reference: %d", static_cast<u8>(ref));
            return IRType::Void;
        }
    }

    default: {
        ERROR("Unimplemented opcode: %d", static_cast<u8>(opcode));
        return IRType::Void;
    }
    }
}

IRType IRInstruction::getTypeFromTuple(IRType type, u8 index) {
    switch (type) {
    case IRType::TupleFourInteger64: {
        if (index < 4) {
            return IRType::Integer64;
        } else {
            ERROR("Invalid index for TupleFourInteger64: %d", index);
            return IRType::Void;
        }
    }
    case IRType::TupleTwoInteger64: {
        if (index < 2) {
            return IRType::Integer64;
        } else {
            ERROR("Invalid index for TupleTwoInteger64: %d", index);
            return IRType::Void;
        }
    }
    default: {
        ERROR("Invalid type for tuple: %d", static_cast<u8>(type));
        return IRType::Void;
    }
    }
}

void IRInstruction::Invalidate() {
    switch (expression.index()) {
    case 0: {
        Operands& operands = std::get<Operands>(expression);
        for (IRInstruction* operand : operands.operands) {
            if (operand != nullptr) {
                operand->uses--;
            } else {
                ERROR("Operand is null");
            }
        }
        break;
    }
    case 1: {
        break;
    }
    case 2: {
        break;
    }
    case 3: {
        SetGuest& set_guest = std::get<SetGuest>(expression);
        if (set_guest.source != nullptr) {
            set_guest.source->uses--;
        } else {
            ERROR("Source is null");
        }
        break;
    }
    case 4: {
        Phi& phi = std::get<Phi>(expression);
        for (const PhiNode& node : phi.nodes) {
            if (node.value != nullptr) {
                node.value->uses--;
            } else {
                ERROR("Value is null");
            }
        }
        break;
    }
    case 5: {
        break;
    }
    case 6: {
        TupleAccess& tuple_access = std::get<TupleAccess>(expression);
        if (tuple_access.tuple != nullptr) {
            tuple_access.tuple->uses--;
        } else {
            ERROR("Tuple is null");
        }
        break;
    }
    default:
        ERROR("Invalid expression type");
        break;
    }
}

#define VALIDATE_0OP(opcode)                                                                                                                         \
    case IROpcode::opcode:                                                                                                                           \
        if (operands.operands.size() != 0) {                                                                                                         \
            ERROR("Invalid operands for opcode %d", static_cast<u8>(IROpcode::opcode));                                                              \
        }                                                                                                                                            \
        break

#define VALIDATE_OPS_INT(opcode, num_ops)                                                                                                            \
    case IROpcode::opcode:                                                                                                                           \
        if (operands.operands.size() != num_ops) {                                                                                                   \
            ERROR("Invalid operands for opcode %d", static_cast<u8>(IROpcode::opcode));                                                              \
        }                                                                                                                                            \
        for (IRInstruction * operand : operands.operands) {                                                                                          \
            if (operand->GetType() != IRType::Integer64) {                                                                                           \
                ERROR("Invalid operand type for opcode %d", static_cast<u8>(IROpcode::opcode));                                                      \
            }                                                                                                                                        \
        }                                                                                                                                            \
        break

#define VALIDATE_OPS_VECTOR(opcode, num_ops)                                                                                                         \
    case IROpcode::opcode:                                                                                                                           \
        if (operands.operands.size() != num_ops) {                                                                                                   \
            ERROR("Invalid operands for opcode %d", static_cast<u8>(IROpcode::opcode));                                                              \
        }                                                                                                                                            \
        for (IRInstruction * operand : operands.operands) {                                                                                          \
            if (operand->GetType() != IRType::Vector128) {                                                                                           \
                ERROR("Invalid operand type for opcode %d", static_cast<u8>(IROpcode::opcode));                                                      \
            }                                                                                                                                        \
        }                                                                                                                                            \
        break

#define BAD(opcode)                                                                                                                                  \
    case IROpcode::opcode:                                                                                                                           \
        ERROR("Invalid opcode %d", static_cast<u8>(IROpcode::opcode));                                                                               \
        break

void IRInstruction::checkValidity(IROpcode opcode, const Operands& operands) {
    switch (opcode) {
    case IROpcode::Null: {
        ERROR("Null should not be used");
        break;
    }

        BAD(Mov);
        BAD(Phi);
        BAD(GetGuest);
        BAD(SetGuest);
        BAD(LoadGuestFromMemory);
        BAD(StoreGuestToMemory);
        BAD(TupleExtract);
        BAD(Comment);
        BAD(Immediate);

        VALIDATE_0OP(Rdtsc);

        VALIDATE_OPS_INT(Sext8, 1);
        VALIDATE_OPS_INT(Sext16, 1);
        VALIDATE_OPS_INT(Sext32, 1);
        VALIDATE_OPS_INT(CastIntegerToVector, 1);
        VALIDATE_OPS_INT(Clz, 1);
        VALIDATE_OPS_INT(Ctz, 1);
        VALIDATE_OPS_INT(Not, 1);
        VALIDATE_OPS_INT(Popcount, 1);
        VALIDATE_OPS_INT(ReadByte, 1);
        VALIDATE_OPS_INT(ReadWord, 1);
        VALIDATE_OPS_INT(ReadDWord, 1);
        VALIDATE_OPS_INT(ReadQWord, 1);
        VALIDATE_OPS_INT(ReadXmmWord, 1);

        VALIDATE_OPS_INT(WriteByte, 2);
        VALIDATE_OPS_INT(WriteWord, 2);
        VALIDATE_OPS_INT(WriteDWord, 2);
        VALIDATE_OPS_INT(WriteQWord, 2);
        VALIDATE_OPS_INT(Add, 2);
        VALIDATE_OPS_INT(Sub, 2);
        VALIDATE_OPS_INT(ShiftLeft, 2);
        VALIDATE_OPS_INT(ShiftRight, 2);
        VALIDATE_OPS_INT(ShiftRightArithmetic, 2);
        VALIDATE_OPS_INT(And, 2);
        VALIDATE_OPS_INT(Or, 2);
        VALIDATE_OPS_INT(Xor, 2);
        VALIDATE_OPS_INT(Equal, 2);
        VALIDATE_OPS_INT(NotEqual, 2);
        VALIDATE_OPS_INT(IGreaterThan, 2);
        VALIDATE_OPS_INT(ILessThan, 2);
        VALIDATE_OPS_INT(UGreaterThan, 2);
        VALIDATE_OPS_INT(ULessThan, 2);
        VALIDATE_OPS_INT(LeftRotate8, 2);
        VALIDATE_OPS_INT(LeftRotate16, 2);
        VALIDATE_OPS_INT(LeftRotate32, 2);
        VALIDATE_OPS_INT(LeftRotate64, 2);
        VALIDATE_OPS_INT(IDiv8, 2);
        VALIDATE_OPS_INT(UDiv8, 2);
        VALIDATE_OPS_INT(Cpuid, 2);
        VALIDATE_OPS_INT(IMul64, 2);

        VALIDATE_OPS_INT(IDiv16, 3);
        VALIDATE_OPS_INT(UDiv16, 3);
        VALIDATE_OPS_INT(IDiv32, 3);
        VALIDATE_OPS_INT(UDiv32, 3);
        VALIDATE_OPS_INT(IDiv64, 3);
        VALIDATE_OPS_INT(UDiv64, 3);
        VALIDATE_OPS_INT(Select, 3);

        VALIDATE_OPS_INT(Lea, 4);

        VALIDATE_OPS_INT(Syscall, 7);

        VALIDATE_OPS_VECTOR(CastVectorToInteger, 1);
        VALIDATE_OPS_VECTOR(VExtractInteger, 1);
        VALIDATE_OPS_VECTOR(VPackedShuffleDWord, 1);
        VALIDATE_OPS_VECTOR(VMoveByteMask, 1);
        VALIDATE_OPS_VECTOR(VZext64, 1);

        VALIDATE_OPS_VECTOR(VUnpackByteLow, 2);
        VALIDATE_OPS_VECTOR(VUnpackWordLow, 2);
        VALIDATE_OPS_VECTOR(VUnpackDWordLow, 2);
        VALIDATE_OPS_VECTOR(VUnpackQWordLow, 2);
        VALIDATE_OPS_VECTOR(VAnd, 2);
        VALIDATE_OPS_VECTOR(VOr, 2);
        VALIDATE_OPS_VECTOR(VXor, 2);
        VALIDATE_OPS_VECTOR(VShr, 2);
        VALIDATE_OPS_VECTOR(VShl, 2);
        VALIDATE_OPS_VECTOR(VPackedSubByte, 2);
        VALIDATE_OPS_VECTOR(VPackedAddQWord, 2);
        VALIDATE_OPS_VECTOR(VPackedEqualByte, 2);
        VALIDATE_OPS_VECTOR(VPackedEqualWord, 2);
        VALIDATE_OPS_VECTOR(VPackedEqualDWord, 2);
        VALIDATE_OPS_VECTOR(VPackedMinByte, 2);

    case IROpcode::WriteXmmWord:
    case IROpcode::VInsertInteger: {
        if (operands.operands.size() != 2) {
            ERROR("Invalid operands for opcode %d", static_cast<u8>(opcode));
        }

        if (operands.operands[0]->GetType() != IRType::Integer64) {
            ERROR("Invalid operand type for opcode %d", static_cast<u8>(opcode));
        }

        if (operands.operands[1]->GetType() != IRType::Vector128) {
            ERROR("Invalid operand type for opcode %d", static_cast<u8>(opcode));
        }
    }
    }
}
