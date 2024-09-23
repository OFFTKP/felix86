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

        if (operands.control_byte != other_operands.control_byte) {
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
        const TupleGet& tuple_get = std::get<TupleGet>(expression);
        const TupleGet& other_tuple_get = std::get<TupleGet>(other.expression);

        return tuple_get.tuple == other_tuple_get.tuple && tuple_get.index == other_tuple_get.index;
    }
    default:
        ERROR("Invalid expression type");
        return false;
    }
}

IRType GetTypeFromOpcode(IROpcode opcode) {
    switch (opcode) {
        case IROpcode::IR_TUPLE_GET:
        case IROpcode::IR_MOV: 
        case IROpcode::IR_LOAD_GUEST_FROM_MEMORY:
        case IROpcode::IR_STORE_GUEST_TO_MEMORY:
        case IROpcode::IR_PHI:
        case IROpcode::IR_GET_GUEST:
        case IROpcode::IR_SET_GUEST: {
            ERROR("Should not be used in GetTypeFromOpcode");
            return IRType::Void;
        }
        case IROpcode::IR_NULL:
        case IROpcode::IR_START_OF_BLOCK:
        case IROpcode::IR_COMMENT: {
            return IRType::Void;
        }
        case IROpcode::IR_SELECT:
        case IROpcode::IR_IMMEDIATE:
        case IROpcode::IR_POPCOUNT:
        case IROpcode::IR_ADD:
        case IROpcode::IR_SUB:
        case IROpcode::IR_CLZ:
        case IROpcode::IR_CTZ:
        case IROpcode::IR_SHIFT_LEFT:
        case IROpcode::IR_SHIFT_RIGHT:
        case IROpcode::IR_SHIFT_RIGHT_ARITHMETIC:
        case IROpcode::IR_LEFT_ROTATE8:
        case IROpcode::IR_LEFT_ROTATE16:
        case IROpcode::IR_LEFT_ROTATE32:
        case IROpcode::IR_LEFT_ROTATE64:
        case IROpcode::IR_AND:
        case IROpcode::IR_OR:
        case IROpcode::IR_XOR:
        case IROpcode::IR_NOT:
        case IROpcode::IR_LEA:
        case IROpcode::IR_EQUAL:
        case IROpcode::IR_NOT_EQUAL:
        case IROpcode::IR_GREATER_THAN_SIGNED:
        case IROpcode::IR_LESS_THAN_SIGNED:
        case IROpcode::IR_GREATER_THAN_UNSIGNED:
        case IROpcode::IR_LESS_THAN_UNSIGNED:
        case IROpcode::IR_READ_BYTE:
        case IROpcode::IR_READ_WORD:
        case IROpcode::IR_READ_DWORD:
        case IROpcode::IR_READ_QWORD:
        case IROpcode::IR_SYSCALL:
        case IROpcode::IR_INTEGER_FROM_VECTOR:
        case IROpcode::IR_EXTRACT_INTEGER_FROM_VECTOR:
        case IROpcode::IR_SEXT8:
        case IROpcode::IR_SEXT16:
        case IROpcode::IR_SEXT32: {
            return IRType::Integer64;
        }
        case IROpcode::IR_IMUL64:
        case IROpcode::IR_IDIV8:
        case IROpcode::IR_IDIV16:
        case IROpcode::IR_IDIV32:
        case IROpcode::IR_IDIV64:
        case IROpcode::IR_UDIV8:
        case IROpcode::IR_UDIV16:
        case IROpcode::IR_UDIV32:
        case IROpcode::IR_UDIV64:
        case IROpcode::IR_RDTSC: {
            return IRType::TupleTwoInteger64;
        }
        case IROpcode::IR_CPUID: {
            return IRType::TupleFourInteger64;
        }
        case IROpcode::IR_READ_XMMWORD:
        case IROpcode::IR_VECTOR_FROM_INTEGER:
        case IROpcode::IR_VECTOR_UNPACK_BYTE_LOW:
        case IROpcode::IR_VECTOR_UNPACK_WORD_LOW:
        case IROpcode::IR_VECTOR_UNPACK_DWORD_LOW:
        case IROpcode::IR_VECTOR_UNPACK_QWORD_LOW:
        case IROpcode::IR_VECTOR_PACKED_AND:
        case IROpcode::IR_VECTOR_PACKED_OR:
        case IROpcode::IR_VECTOR_PACKED_XOR:
        case IROpcode::IR_VECTOR_PACKED_SHIFT_RIGHT:
        case IROpcode::IR_VECTOR_PACKED_SHIFT_LEFT:
        case IROpcode::IR_VECTOR_PACKED_SUB_BYTE:
        case IROpcode::IR_VECTOR_PACKED_ADD_QWORD:
        case IROpcode::IR_VECTOR_PACKED_COMPARE_EQ_BYTE:
        case IROpcode::IR_VECTOR_PACKED_COMPARE_EQ_WORD:
        case IROpcode::IR_VECTOR_PACKED_COMPARE_EQ_DWORD:
        case IROpcode::IR_VECTOR_PACKED_SHUFFLE_DWORD:
        case IROpcode::IR_VECTOR_PACKED_MOVE_BYTE_MASK:
        case IROpcode::IR_VECTOR_PACKED_MIN_BYTE:
        case IROpcode::IR_VECTOR_PACKED_COMPARE_IMPLICIT_STRING_INDEX: 
        case IROpcode::IR_VECTOR_ZEXT64:
        case IROpcode::IR_INSERT_INTEGER_TO_VECTOR: {
            return IRType::Vector128;
        }
        case IROpcode::IR_WRITE_BYTE:
        case IROpcode::IR_WRITE_WORD:
        case IROpcode::IR_WRITE_DWORD:
        case IROpcode::IR_WRITE_QWORD:
        case IROpcode::IR_WRITE_XMMWORD: {
            return IRType::Void;
        }
        default: {
            ERROR("Unimplemented opcode: %d", static_cast<u8>(opcode));
            return IRType::Void;
        }
    }
}

IRType GetTypeFromTuple(IRType type, u8 index) {
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

IRType GetTypeFromPhi(const Phi& phi) {
    if (phi.nodes.empty()) {
        ERROR("Phi node has no nodes");
        return IRType::Void;
    }

    if (phi.nodes[0].value == nullptr) {
        ERROR("Phi node 0 has no value");
        return IRType::Void;
    }

    IRType firstType = phi.nodes[0].value->GetType();
    for (const PhiNode& node : phi.nodes) {
        if (node.value == nullptr) {
            ERROR("Phi node has no value");
            return IRType::Void;
        }

        if (node.value->GetType() != firstType) {
            ERROR("Phi node has different types");
            return IRType::Void;
        }
    }

    return firstType;
}

void IRInstruction::UndoUses() {
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
        break;
    }
    default:
        ERROR("Invalid expression type");
        break;
    }
}