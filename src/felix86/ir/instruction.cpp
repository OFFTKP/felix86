#include <fmt/format.h>
#include "felix86/common/log.hpp"
#include "felix86/common/print.hpp"
#include "felix86/ir/block.hpp"
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
        ERROR("Unreachable");
        return false;
    }
}

IRType IRInstruction::getTypeFromOpcode(IROpcode opcode, x86_ref_e ref) {
    switch (opcode) {
    case IROpcode::TupleExtract:
    case IROpcode::Mov: {
        ERROR("Should not be used in GetTypeFromOpcode: %d", (int)opcode);
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
    if (locked) {
        ERROR("Tried to invalidate locked instruction");
    }

    switch (expression.index()) {
    case 0: {
        Operands& operands = std::get<Operands>(expression);
        for (IRInstruction* operand : operands.operands) {
            if (operand != nullptr) {
                operand->RemoveUse();
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
            set_guest.source->RemoveUse();
        } else {
            ERROR("Source is null");
        }
        break;
    }
    case 4: {
        Phi& phi = std::get<Phi>(expression);
        for (const PhiNode& node : phi.nodes) {
            if (node.value != nullptr) {
                node.value->RemoveUse();
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
            tuple_access.tuple->RemoveUse();
        } else {
            ERROR("Tuple is null");
        }
        break;
    }
    default:
        ERROR("Unreachable");
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

std::string IRInstruction::GetNameString() const {
    switch (return_type) {
    case IRType::Integer64: {
        return fmt::format("i{}", GetName());
    }
    case IRType::Vector128: {
        return fmt::format("v{}", GetName());
    }
    case IRType::Float80: {
        return fmt::format("f{}", GetName());
    }
    case IRType::TupleTwoInteger64: {
        return fmt::format("t{}<int, int>", GetName());
    }
    case IRType::TupleFourInteger64: {
        return fmt::format("t{}<i64, i64, i64, i64>", GetName());
    }
    case IRType::Void: {
        return "void";
    }
    default: {
        ERROR("Unreachable");
        return "";
    }
    }
}

#define OP2(op) fmt::format("{} ← {} {} {}", GetNameString(), GetOperandNameString(0), #op, GetOperandNameString(1))
#define SOP2(op) fmt::format("{} ← (i64){} {} (i64){}", GetNameString(), GetOperandNameString(0), #op, GetOperandNameString(1))
#define U8OP2(op) fmt::format("{} ← (u8){} {} (u8){}", GetNameString(), GetOperandNameString(0), #op, GetOperandNameString(1))
#define S8OP2(op) fmt::format("{} ← (i8){} {} (i8){}", GetNameString(), GetOperandNameString(0), #op, GetOperandNameString(1))
#define S16OP2(op) fmt::format("{} ← (i16){} {} (i16){}", GetNameString(), GetOperandNameString(0), #op, GetOperandNameString(1))
#define S32OP2(op) fmt::format("{} ← (i32){} {} (i32){}", GetNameString(), GetOperandNameString(0), #op, GetOperandNameString(1))

#define FOP(func) fmt::format("{} ← {}()", GetNameString(), #func)
#define FOP1(func, param) fmt::format("{} ← {}({}: {})", GetNameString(), #func, #param, GetOperandNameString(0))
#define FOP2(func, param1, param2)                                                                                                                   \
    fmt::format("{} ← {}({}: {}, {}: {})", GetNameString(), #func, #param1, GetOperandNameString(0), #param2, GetOperandNameString(1))
#define FOP3(func, param1, param2, param3)                                                                                                           \
    fmt::format("{} ← {}({}: {}, {}: {}, {}: {})", GetNameString(), #func, #param1, GetOperandNameString(0), #param2, GetOperandNameString(1),       \
                #param3, GetOperandNameString(2))
#define FOP7(func, param1, param2, param3, param4, param5, param6, param7)                                                                           \
    fmt::format("{} ← {}({}: {}, {}: {}, {}: {}, {}: {}, {}: {}, {}: {}, {}: {})", GetNameString(), #func, #param1, GetOperandNameString(0),         \
                #param2, GetOperandNameString(1), #param3, GetOperandNameString(2), #param4, GetOperandNameString(3), #param5,                       \
                GetOperandNameString(4), #param6, GetOperandNameString(5), #param7, GetOperandNameString(6))

std::string IRInstruction::Print() const {
    IROpcode opcode = GetOpcode();
    switch (opcode) {
    case IROpcode::Null: {
        return "Null";
    }
    case IROpcode::Phi: {
        const Phi& phi = AsPhi();
        std::string ret = fmt::format("{} ← φ<%{}>(", GetNameString(), print_guest_register(phi.ref));
        for (size_t i = 0; i < phi.nodes.size(); i++) {
            ret += fmt::format("{} @ Block {}", phi.nodes[i].value->GetNameString(), phi.nodes[i].block->GetIndex());

            if (i != phi.nodes.size() - 1) {
                ret += ", ";
            }
        }
        ret += ")";
        return ret;
    }
    case IROpcode::Comment: {
        return AsComment().comment;
    }
    case IROpcode::TupleExtract: {
        const TupleAccess& tup = AsTupleAccess();
        return fmt::format("{} ← get<{}>({})", GetNameString(), tup.index, tup.tuple->GetNameString());
    }
    case IROpcode::Select: {
        return fmt::format("{} ← {} ? {} : {}", GetNameString(), GetOperandNameString(0), GetOperandNameString(1), GetOperandNameString(2));
    }
    case IROpcode::Lea: {
        return fmt::format("{} ← [{} + {} * {} + 0x{:x}]", GetNameString(), GetOperandNameString(0), GetOperandNameString(1),
                           GetOperand(2)->AsImmediate().immediate, GetOperand(3)->AsImmediate().immediate);
    }
    case IROpcode::Mov: {
        return fmt::format("{} ← {}", GetNameString(), GetOperandNameString(0));
    }
    case IROpcode::Immediate: {
        return fmt::format("{} ← {:x}", GetNameString(), AsImmediate().immediate);
    }
    case IROpcode::Rdtsc: {
        return FOP(rdtsc);
    }
    case IROpcode::GetGuest: {
        return fmt::format("{} ← get_guest %{}", GetNameString(), print_guest_register(AsGetGuest().ref));
    }
    case IROpcode::SetGuest: {
        return fmt::format("{} ← set_guest %{}, {}", GetNameString(), print_guest_register(AsSetGuest().ref), AsSetGuest().source->GetNameString());
    }
    case IROpcode::LoadGuestFromMemory: {
        return fmt::format("{} ← load_from_vm %{}", GetNameString(), print_guest_register(AsGetGuest().ref));
    }
    case IROpcode::StoreGuestToMemory: {
        return fmt::format("{} ← store_to_vm %{}, {}", GetNameString(), print_guest_register(AsSetGuest().ref), AsSetGuest().source->GetNameString());
    }
    case IROpcode::Add: {
        return OP2(+);
    }
    case IROpcode::Sub: {
        return OP2(-);
    }
    case IROpcode::And: {
        return OP2(&);
    }
    case IROpcode::Or: {
        return OP2(|);
    }
    case IROpcode::Xor: {
        return OP2(^);
    }
    case IROpcode::ShiftLeft: {
        return OP2(<<);
    }
    case IROpcode::ShiftRight: {
        return OP2(>>);
    }
    case IROpcode::ShiftRightArithmetic: {
        return SOP2(>>);
    }
    case IROpcode::Equal: {
        return OP2(==);
    }
    case IROpcode::NotEqual: {
        return OP2(!=);
    }
    case IROpcode::UGreaterThan: {
        return OP2(>);
    }
    case IROpcode::IGreaterThan: {
        return SOP2(>);
    }
    case IROpcode::ULessThan: {
        return OP2(<);
    }
    case IROpcode::ILessThan: {
        return SOP2(<);
    }
    case IROpcode::UDiv8: {
        return U8OP2(/);
    }
    case IROpcode::IDiv8: {
        return S8OP2(/);
    }
    case IROpcode::IMul64: {
        return SOP2(*);
    }
    case IROpcode::LeftRotate8: {
        return FOP2(rol8, src, amount);
    }
    case IROpcode::LeftRotate16: {
        return FOP2(rol16, src, amount);
    }
    case IROpcode::LeftRotate32: {
        return FOP2(rol32, src, amount);
    }
    case IROpcode::LeftRotate64: {
        return FOP2(rol64, src, amount);
    }
    case IROpcode::Cpuid: {
        return FOP2(cpuid, rax, rcx);
    }
    case IROpcode::WriteByte: {
        return FOP2(write8, address, src);
    }
    case IROpcode::WriteWord: {
        return FOP2(write16, address, src);
    }
    case IROpcode::WriteDWord: {
        return FOP2(write32, address, src);
    }
    case IROpcode::WriteQWord: {
        return FOP2(write64, address, src);
    }
    case IROpcode::WriteXmmWord: {
        return FOP2(write128, address, src);
    }
    case IROpcode::Sext8: {
        return FOP1(sext8, src);
    }
    case IROpcode::Sext16: {
        return FOP1(sext16, src);
    }
    case IROpcode::Sext32: {
        return FOP1(sext32, src);
    }
    case IROpcode::CastIntegerToVector: {
        return FOP1(int_to_vec, integer);
    }
    case IROpcode::CastVectorToInteger: {
        return FOP1(vec_to_int, vector);
    }
    case IROpcode::Clz: {
        return FOP1(clz, src);
    }
    case IROpcode::Ctz: {
        return FOP1(ctz, src);
    }
    case IROpcode::Not: {
        return FOP1(not, src);
    }
    case IROpcode::Popcount: {
        return FOP1(popcount, src);
    }
    case IROpcode::ReadByte: {
        return FOP1(read8, address);
    }
    case IROpcode::ReadWord: {
        return FOP1(read16, address);
    }
    case IROpcode::ReadDWord: {
        return FOP1(read32, address);
    }
    case IROpcode::ReadQWord: {
        return FOP1(read64, address);
    }
    case IROpcode::ReadXmmWord: {
        return FOP1(read128, address);
    }
    case IROpcode::IDiv16: {
        return FOP3(idiv16, rdx, rax, divisor);
    }
    case IROpcode::IDiv32: {
        return FOP3(idiv32, rdx, rax, divisor);
    }
    case IROpcode::IDiv64: {
        return FOP3(idiv64, rdx, rax, divisor);
    }
    case IROpcode::UDiv16: {
        return FOP3(udiv16, rdx, rax, divisor);
    }
    case IROpcode::UDiv32: {
        return FOP3(udiv32, rdx, rax, divisor);
    }
    case IROpcode::UDiv64: {
        return FOP3(udiv64, rdx, rax, divisor);
    }
    case IROpcode::Syscall: {
        return FOP7(syscall, rax, rdi, rsi, rdx, r10, r8, r9);
    }
    case IROpcode::VAnd: {
        return FOP2(vand, src1, src2);
    }
    case IROpcode::VOr: {
        return FOP2(vor, src1, src2);
    }
    case IROpcode::VXor: {
        return FOP2(vxor, src1, src2);
    }
    case IROpcode::VShl: {
        return FOP2(vshl, src1, src2);
    }
    case IROpcode::VShr: {
        return FOP2(vshr, src1, src2);
    }
    case IROpcode::VZext64: {
        return FOP1(vzext64, src);
    }
    case IROpcode::VPackedAddQWord: {
        return FOP2(vpaddqword, src1, src2);
    }
    case IROpcode::VPackedEqualByte: {
        return FOP2(vpeqbyte, src1, src2);
    }
    case IROpcode::VPackedEqualWord: {
        return FOP2(vpeqword, src1, src2);
    }
    case IROpcode::VPackedEqualDWord: {
        return FOP2(vpeqdword, src1, src2);
    }
    case IROpcode::VPackedShuffleDWord: {
        return fmt::format("{} ← vpshufdword({}, {:x})", GetNameString(), GetOperandNameString(0), (u8)GetExtraData());
    }
    case IROpcode::VPackedMinByte: {
        return FOP2(vpminbyte, src1, src2);
    }
    case IROpcode::VPackedSubByte: {
        return FOP2(vpsubbyte, src1, src2);
    }
    case IROpcode::VMoveByteMask: {
        return FOP1(vmovbytemask, src);
    }
    case IROpcode::VExtractInteger: {
        return FOP1(vextractint, src);
    }
    case IROpcode::VInsertInteger: {
        return FOP2(vinsertint, vector, integer);
    }
    case IROpcode::VUnpackByteLow: {
        return FOP2(vunpackbytelow, src1, src2);
    }
    case IROpcode::VUnpackWordLow: {
        return FOP2(vunpackwordlow, src1, src2);
    }
    case IROpcode::VUnpackDWordLow: {
        return FOP2(vunpackdwordlow, src1, src2);
    }
    case IROpcode::VUnpackQWordLow: {
        return FOP2(vunpackqwordlow, src1, src2);
    }
    default: {
        ERROR("Unimplemented op: %d", (int)GetOpcode());
        return "";
    }
    }
}
