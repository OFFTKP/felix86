#include <fmt/format.h>
#include "felix86/common/log.hpp"
#include "felix86/common/print.hpp"
#include "felix86/ir/block.hpp"
#include "felix86/ir/instruction.hpp"

namespace {
std::string GetBlockName(u32 name) {
    u32 block_index = name >> 20;
    if (block_index == 0) {
        return "Entry";
    } else if (block_index == 1) {
        return "Exit";
    } else {
        return fmt::format("{}", block_index - 2);
    }
}

std::string ToVarName(u32 name) {
    return std::to_string(name & ((1 << 20) - 1));
}
} // namespace

std::string GetNameString(u32 name) {
    return fmt::format("%{}@{}", ToVarName(name), GetBlockName(name));
}

bool SSAInstruction::IsSameExpression(const SSAInstruction& other) const {
    if (expression_type != other.expression_type) {
        return false;
    }

    switch (expression_type) {
    case ExpressionType::Operands: {
        const Operands& operands = AsOperands();
        const Operands& other_operands = other.AsOperands();

        if (operands.operand_count != other_operands.operand_count) {
            return false;
        }

        for (u8 i = 0; i < operands.operand_count; i++) {
            if (operands.operands[i] != other_operands.operands[i]) {
                return false;
            }
        }

        if (operands.immediate_data != other_operands.immediate_data) {
            return false;
        }

        return true;
    }
    case ExpressionType::GetGuest: {
        const GetGuest& get_guest = AsGetGuest();
        const GetGuest& other_get_guest = other.AsGetGuest();

        return get_guest.ref == other_get_guest.ref;
    }
    case ExpressionType::SetGuest: {
        const SetGuest& set_guest = AsSetGuest();
        const SetGuest& other_set_guest = other.AsSetGuest();

        return set_guest.ref == other_set_guest.ref && set_guest.source == other_set_guest.source;
    }
    case ExpressionType::Phi: {
        const Phi& phi = AsPhi();
        const Phi& other_phi = other.AsPhi();

        if (phi.ref != other_phi.ref) {
            return false;
        }

        if (phi.values.size() != other_phi.values.size()) {
            return false;
        }

        for (u16 i = 0; i < phi.values.size(); i++) {
            if (phi.values[i] != other_phi.values[i] || phi.blocks[i] != other_phi.blocks[i]) {
                return false;
            }
        }

        return true;
    }
    case ExpressionType::Comment: {
        const Comment& comment = AsComment();
        const Comment& other_comment = other.AsComment();

        return comment.comment == other_comment.comment;
    }
    default:
        UNREACHABLE();
        return false;
    }
}

IRType SSAInstruction::getTypeFromOpcode(IROpcode opcode, x86_ref_e ref) {
    switch (opcode) {
    case IROpcode::Mov: {
        ERROR("Should not be used in GetTypeFromOpcode: %d", (int)opcode);
        return IRType::Void;
    }
    case IROpcode::Null:
    case IROpcode::Comment:
    case IROpcode::Syscall:
    case IROpcode::Cpuid:
    case IROpcode::Rdtsc:
    case IROpcode::Div128:
    case IROpcode::Divu128: {
        return IRType::Void;
    }
    case IROpcode::Select:
    case IROpcode::Immediate:
    case IROpcode::Parity:
    case IROpcode::Add:
    case IROpcode::Addi:
    case IROpcode::Sub:
    case IROpcode::Clz:
    case IROpcode::Ctzh:
    case IROpcode::Ctzw:
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
    case IROpcode::CastVectorToInteger:
    case IROpcode::VExtractInteger:
    case IROpcode::Sext8:
    case IROpcode::Sext16:
    case IROpcode::Sext32:
    case IROpcode::Div:
    case IROpcode::Divu:
    case IROpcode::Divw:
    case IROpcode::Divuw:
    case IROpcode::Rem:
    case IROpcode::Remu:
    case IROpcode::Remw:
    case IROpcode::Remuw: {
        return IRType::Integer64;
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
    case IROpcode::VShiftRight:
    case IROpcode::VShiftLeft:
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
    case IROpcode::WriteXmmWord:
    case IROpcode::StoreGuestToMemory: {
        return IRType::Void;
    }

    case IROpcode::Phi:
    case IROpcode::GetGuest:
    case IROpcode::SetGuest:
    case IROpcode::LoadGuestFromMemory: {
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

void SSAInstruction::Invalidate() {
    if (locked) {
        ERROR("Tried to invalidate locked instruction");
    }

    for (SSAInstruction* used : GetUsedInstructions()) {
        used->RemoveUse();
    }
}

#define VALIDATE_0OP(opcode)                                                                                                                         \
    case IROpcode::opcode:                                                                                                                           \
        if (operands.operand_count != 0) {                                                                                                           \
            ERROR("Invalid operands for opcode %d", static_cast<u8>(IROpcode::opcode));                                                              \
        }                                                                                                                                            \
        break

#define VALIDATE_OPS_INT(opcode, num_ops)                                                                                                            \
    case IROpcode::opcode:                                                                                                                           \
        if (operands.operand_count != num_ops) {                                                                                                     \
            ERROR("Invalid operands for opcode %d", static_cast<u8>(IROpcode::opcode));                                                              \
        }                                                                                                                                            \
        for (u8 i = 0; i < operands.operand_count; i++) {                                                                                            \
            SSAInstruction* operand = operands.operands[i];                                                                                          \
            if (operand->GetType() != IRType::Integer64) {                                                                                           \
                ERROR("Invalid operand type for opcode %d", static_cast<u8>(IROpcode::opcode));                                                      \
            }                                                                                                                                        \
        }                                                                                                                                            \
        break

#define VALIDATE_OPS_VECTOR(opcode, num_ops)                                                                                                         \
    case IROpcode::opcode:                                                                                                                           \
        if (operands.operand_count != num_ops) {                                                                                                     \
            ERROR("Invalid operands for opcode %d", static_cast<u8>(IROpcode::opcode));                                                              \
        }                                                                                                                                            \
        for (u8 i = 0; i < operands.operand_count; i++) {                                                                                            \
            SSAInstruction* operand = operands.operands[i];                                                                                          \
            if (operand->GetType() != IRType::Vector128) {                                                                                           \
                ERROR("Invalid operand type for opcode %d", static_cast<u8>(IROpcode::opcode));                                                      \
            }                                                                                                                                        \
        }                                                                                                                                            \
        break

#define BAD(opcode)                                                                                                                                  \
    case IROpcode::opcode:                                                                                                                           \
        ERROR("Invalid opcode %d", static_cast<u8>(IROpcode::opcode));                                                                               \
        break

void SSAInstruction::checkValidity(IROpcode opcode, const Operands& operands) {
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
        BAD(Comment);
        BAD(Immediate);

        VALIDATE_0OP(Rdtsc);
        VALIDATE_0OP(Syscall);
        VALIDATE_0OP(Cpuid);

        VALIDATE_OPS_INT(Addi, 1);
        VALIDATE_OPS_INT(Sext8, 1);
        VALIDATE_OPS_INT(Sext16, 1);
        VALIDATE_OPS_INT(Sext32, 1);
        VALIDATE_OPS_INT(CastIntegerToVector, 1);
        VALIDATE_OPS_INT(Clz, 1);
        VALIDATE_OPS_INT(Ctzh, 1);
        VALIDATE_OPS_INT(Ctzw, 1);
        VALIDATE_OPS_INT(Ctz, 1);
        VALIDATE_OPS_INT(Not, 1);
        VALIDATE_OPS_INT(Parity, 1);
        VALIDATE_OPS_INT(ReadByte, 1);
        VALIDATE_OPS_INT(ReadWord, 1);
        VALIDATE_OPS_INT(ReadDWord, 1);
        VALIDATE_OPS_INT(ReadQWord, 1);
        VALIDATE_OPS_INT(ReadXmmWord, 1);
        VALIDATE_OPS_INT(Div128, 1);
        VALIDATE_OPS_INT(Divu128, 1);

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
        VALIDATE_OPS_INT(Div, 2);
        VALIDATE_OPS_INT(Divu, 2);
        VALIDATE_OPS_INT(Divw, 2);
        VALIDATE_OPS_INT(Divuw, 2);
        VALIDATE_OPS_INT(Rem, 2);
        VALIDATE_OPS_INT(Remu, 2);
        VALIDATE_OPS_INT(Remw, 2);
        VALIDATE_OPS_INT(Remuw, 2);
        VALIDATE_OPS_INT(Mul, 2);
        VALIDATE_OPS_INT(Mulh, 2);
        VALIDATE_OPS_INT(Mulhu, 2);

        VALIDATE_OPS_INT(Select, 3);

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
        VALIDATE_OPS_VECTOR(VShiftRight, 2);
        VALIDATE_OPS_VECTOR(VShiftLeft, 2);
        VALIDATE_OPS_VECTOR(VPackedSubByte, 2);
        VALIDATE_OPS_VECTOR(VPackedAddQWord, 2);
        VALIDATE_OPS_VECTOR(VPackedEqualByte, 2);
        VALIDATE_OPS_VECTOR(VPackedEqualWord, 2);
        VALIDATE_OPS_VECTOR(VPackedEqualDWord, 2);
        VALIDATE_OPS_VECTOR(VPackedMinByte, 2);

    case IROpcode::WriteXmmWord:
    case IROpcode::VInsertInteger: {
        if (operands.operand_count != 2) {
            ERROR("Invalid operands for opcode %d", static_cast<u8>(opcode));
        }

        if (operands.operands[0]->GetType() != IRType::Integer64) {
            ERROR("Invalid operand type for opcode %d", static_cast<u8>(opcode));
        }

        if (operands.operands[1]->GetType() != IRType::Vector128) {
            ERROR("Invalid operand type for opcode %d", static_cast<u8>(opcode));
        }
        break;
    }

    default: {
        UNREACHABLE();
    }
    }
}

std::string SSAInstruction::GetTypeString() const {
    switch (GetType()) {
    case IRType::Integer64: {
        return "Int64";
    }
    case IRType::Vector128: {
        return "Vec128";
    }
    case IRType::Float64: {
        return "Float64";
    }
    case IRType::Float80: {
        return "Float80";
    }
    case IRType::Void: {
        return "Void";
    }
    default: {
        UNREACHABLE();
        return "";
    }
    }
}

#define OP2(op)                                                                                                                                      \
    fmt::format("{} {} <- {} {} {}", GetTypeString(), GetNameString(GetName()), GetNameString(GetOperandName(0)), #op,                               \
                GetNameString(GetOperandName(1)))
#define OP2I(op)                                                                                                                                     \
    fmt::format("{} {} <- {} {} {}", GetTypeString(), GetNameString(GetName()), GetNameString(GetOperandName(0)), #op, (i64)GetImmediateData())
#define FOP(func) fmt::format("{} {} <- {}()", GetTypeString(), GetNameString(GetName()), #func)
#define FOP1(func, param)                                                                                                                            \
    fmt::format("{} {} <- {}({}: {})", GetTypeString(), GetNameString(GetName()), #func, #param, GetNameString(GetOperandName(0)))
#define FOP2(func, param1, param2)                                                                                                                   \
    fmt::format("{} {} <- {}({}: {}, {}: {})", GetTypeString(), GetNameString(GetName()), #func, #param1, GetNameString(GetOperandName(0)), #param2, \
                GetNameString(GetOperandName(1)))
#define VOP2(func, param1, param2)                                                                                                                   \
    fmt::format("{}({}: {}, {}: {})", #func, #param1, GetNameString(GetOperandName(0)), #param2, GetNameString(GetOperandName(1)))

std::string SSAInstruction::Print(const std::function<std::string(const SSAInstruction*)>& callback) const {
    IROpcode opcode = GetOpcode();
    std::string ret;
    switch (opcode) {
    case IROpcode::Null: {
        return "Null";
    }
    case IROpcode::Phi: {
        const Phi& phi = AsPhi();
        ret = fmt::format("{} {} <- φ<{}>(", GetTypeString(), GetNameString(GetName()), print_guest_register(phi.ref));
        for (size_t i = 0; i < phi.values.size(); i++) {
            if (!phi.blocks[i]) {
                ERROR("Block is null");
            }

            if (!phi.values[i]) {
                ERROR("Value is null");
            }

            ret += fmt::format("{} @ Block {}", GetNameString(phi.values[i]->GetName()), phi.blocks[i]->GetName());

            if (i != phi.values.size() - 1) {
                ret += ", ";
            }
        }
        ret += ")";
        break;
    }
    case IROpcode::Comment: {
        return AsComment().comment;
    }
    case IROpcode::GetGuest: {
        ret += fmt::format("{} <- get_guest {}", GetNameString(GetName()), print_guest_register(AsGetGuest().ref));
        break;
    }
    case IROpcode::SetGuest: {
        ret += fmt::format("{} <- set_guest {}, {}", GetNameString(GetName()), print_guest_register(AsSetGuest().ref),
                           GetNameString(AsSetGuest().source->GetName()));
        break;
    }
    case IROpcode::LoadGuestFromMemory: {
        ret += fmt::format("{} <- load_from_vm {}", GetNameString(GetName()), print_guest_register(AsGetGuest().ref));
        break;
    }
    case IROpcode::StoreGuestToMemory: {
        ret += fmt::format("store_to_vm {}, {}", print_guest_register(AsSetGuest().ref), GetNameString(AsSetGuest().source->GetName()));
        break;
    }
    case IROpcode::Immediate: {
        ret += fmt::format("{} {} <- 0x{:x}", GetTypeString(), GetNameString(GetName()), GetImmediateData());
        break;
    }
    case IROpcode::Select: {
        ret += fmt::format("{} {} <- {} ? {} : {}", GetTypeString(), GetNameString(GetName()), GetNameString(GetOperandName(0)),
                           GetNameString(GetOperandName(1)), GetOperandName(2));
        break;
    }
    case IROpcode::Mov: {
        ret += fmt::format("{} {} <- {}", GetTypeString(), GetNameString(GetName()), GetNameString(GetOperandName(0)));
        break;
    }
    case IROpcode::Rdtsc: {
        ret += FOP(rdtsc);
        break;
    }
    case IROpcode::Add: {
        ret += OP2(+);
        break;
    }
    case IROpcode::Addi: {
        ret += OP2I(+);
        break;
    }
    case IROpcode::Sub: {
        ret += OP2(-);
        break;
    }
    case IROpcode::And: {
        ret += OP2(&);
        break;
    }
    case IROpcode::Or: {
        ret += OP2(|);
        break;
    }
    case IROpcode::Xor: {
        ret += OP2(^);
        break;
    }
    case IROpcode::ShiftLeft: {
        ret += OP2(<<);
        break;
    }
    case IROpcode::ShiftRight: {
        ret += OP2(>>);
        break;
    }
    case IROpcode::ShiftRightArithmetic: {
        ret += OP2(>>);
        break;
    }
    case IROpcode::Equal: {
        ret += OP2(==);
        break;
    }
    case IROpcode::NotEqual: {
        ret += OP2(!=);
        break;
    }
    case IROpcode::UGreaterThan: {
        ret += OP2(>);
        break;
    }
    case IROpcode::IGreaterThan: {
        ret += OP2(>);
        break;
    }
    case IROpcode::ULessThan: {
        ret += OP2(<);
        break;
    }
    case IROpcode::ILessThan: {
        ret += OP2(<);
        break;
    }
    case IROpcode::Mul:
    case IROpcode::Mulh:
    case IROpcode::Mulhu: {
        ret += OP2(*);
        break;
    }
    case IROpcode::LeftRotate8: {
        ret += FOP2(rol8, src, amount);
        break;
    }
    case IROpcode::LeftRotate16: {
        ret += FOP2(rol16, src, amount);
        break;
    }
    case IROpcode::LeftRotate32: {
        ret += FOP2(rol32, src, amount);
        break;
    }
    case IROpcode::LeftRotate64: {
        ret += FOP2(rol64, src, amount);
        break;
    }
    case IROpcode::Cpuid: {
        ret += FOP(cpuid);
        break;
    }
    case IROpcode::WriteByte: {
        ret += VOP2(write8, address, src);
        break;
    }
    case IROpcode::WriteWord: {
        ret += VOP2(write16, address, src);
        break;
    }
    case IROpcode::WriteDWord: {
        ret += VOP2(write32, address, src);
        break;
    }
    case IROpcode::WriteQWord: {
        ret += VOP2(write64, address, src);
        break;
    }
    case IROpcode::WriteXmmWord: {
        ret += VOP2(write128, address, src);
        break;
    }
    case IROpcode::Sext8: {
        ret += FOP1(sext8, src);
        break;
    }
    case IROpcode::Sext16: {
        ret += FOP1(sext16, src);
        break;
    }
    case IROpcode::Sext32: {
        ret += FOP1(sext32, src);
        break;
    }
    case IROpcode::CastIntegerToVector: {
        ret += FOP1(int_to_vec, integer);
        break;
    }
    case IROpcode::CastVectorToInteger: {
        ret += FOP1(vec_to_int, vector);
        break;
    }
    case IROpcode::Clz: {
        ret += FOP1(clz, src);
        break;
    }
    case IROpcode::Ctzh: {
        ret += FOP1(ctzh, src);
        break;
    }
    case IROpcode::Ctzw: {
        ret += FOP1(ctzw, src);
        break;
    }
    case IROpcode::Ctz: {
        ret += FOP1(ctz, src);
        break;
    }
    case IROpcode::Not: {
        ret += FOP1(not, src);
        break;
    }
    case IROpcode::Parity: {
        ret += FOP1(parity, src);
        break;
    }
    case IROpcode::ReadByte: {
        ret += FOP1(read8, address);
        break;
    }
    case IROpcode::ReadWord: {
        ret += FOP1(read16, address);
        break;
    }
    case IROpcode::ReadDWord: {
        ret += FOP1(read32, address);
        break;
    }
    case IROpcode::ReadQWord: {
        ret += FOP1(read64, address);
        break;
    }
    case IROpcode::ReadXmmWord: {
        ret += FOP1(read128, address);
        break;
    }
    case IROpcode::Div: {
        ret += FOP2(div, dividend, divisor);
        break;
    }
    case IROpcode::Divu: {
        ret += FOP2(divu, dividend, divisor);
        break;
    }
    case IROpcode::Divw: {
        ret += FOP2(divw, dividend, divisor);
        break;
    }
    case IROpcode::Divuw: {
        ret += FOP2(divuw, dividend, divisor);
        break;
    }
    case IROpcode::Rem: {
        ret += FOP2(rem, dividend, divisor);
        break;
    }
    case IROpcode::Remu: {
        ret += FOP2(remu, dividend, divisor);
        break;
    }
    case IROpcode::Remw: {
        ret += FOP2(remw, dividend, divisor);
        break;
    }
    case IROpcode::Remuw: {
        ret += FOP2(remuw, dividend, divisor);
        break;
    }
    case IROpcode::Div128: {
        ret += FOP1(div128, divisor);
        break;
    }
    case IROpcode::Divu128: {
        ret += FOP1(divu128, divisor);
        break;
    }
    case IROpcode::Syscall: {
        ret += FOP(syscall);
        break;
    }
    case IROpcode::VAnd: {
        ret += FOP2(vand, src1, src2);
        break;
    }
    case IROpcode::VOr: {
        ret += FOP2(vor, src1, src2);
        break;
    }
    case IROpcode::VXor: {
        ret += FOP2(vxor, src1, src2);
        break;
    }
    case IROpcode::VShiftLeft: {
        ret += FOP2(vshl, src1, src2);
        break;
    }
    case IROpcode::VShiftRight: {
        ret += FOP2(vshr, src1, src2);
        break;
    }
    case IROpcode::VZext64: {
        ret += FOP1(vzext64, src);
        break;
    }
    case IROpcode::VPackedAddQWord: {
        ret += FOP2(vpaddqword, src1, src2);
        break;
    }
    case IROpcode::VPackedEqualByte: {
        ret += FOP2(vpeqbyte, src1, src2);
        break;
    }
    case IROpcode::VPackedEqualWord: {
        ret += FOP2(vpeqword, src1, src2);
        break;
    }
    case IROpcode::VPackedEqualDWord: {
        ret += FOP2(vpeqdword, src1, src2);
        break;
    }
    case IROpcode::VPackedShuffleDWord: {
        ret += fmt::format("{} {} <- vpshufdword({}, 0x{:x})", GetTypeString(), GetNameString(GetName()), GetNameString(GetOperandName(0)),
                           (u8)GetImmediateData());
        break;
    }
    case IROpcode::VPackedMinByte: {
        ret += FOP2(vpminbyte, src1, src2);
        break;
    }
    case IROpcode::VPackedSubByte: {
        ret += FOP2(vpsubbyte, src1, src2);
        break;
    }
    case IROpcode::VMoveByteMask: {
        ret += FOP1(vmovbytemask, src);
        break;
    }
    case IROpcode::VExtractInteger: {
        ret += FOP1(vextractint, src);
        break;
    }
    case IROpcode::VInsertInteger: {
        ret += FOP2(vinsertint, vector, integer);
        break;
    }
    case IROpcode::VUnpackByteLow: {
        ret += FOP2(vunpackbytelow, src1, src2);
        break;
    }
    case IROpcode::VUnpackWordLow: {
        ret += FOP2(vunpackwordlow, src1, src2);
        break;
    }
    case IROpcode::VUnpackDWordLow: {
        ret += FOP2(vunpackdwordlow, src1, src2);
        break;
    }
    case IROpcode::VUnpackQWordLow: {
        ret += FOP2(vunpackqwordlow, src1, src2);
        break;
    }
    default: {
        ERROR("Unimplemented op: %d", (int)GetOpcode());
        return "";
    }
    }

    if (opcode != IROpcode::Comment) {
        u64 size = ret.size();
        while (size < 50) {
            ret += " ";
            size++;
        }

        ret += "(uses: " + std::to_string(GetUseCount());
        if (IsLocked()) {
            ret += " *locked*";
        }
        ret += ")";
    }

    if (callback)
        ret += callback(this);

    return ret;
}

#undef OP2
#undef OP2I
#undef FOP
#undef FOP1
#undef FOP2
#undef VOP2

bool SSAInstruction::IsVoid() const {
    return return_type == IRType::Void;
}

std::span<SSAInstruction*> SSAInstruction::GetUsedInstructions() {
    switch (expression_type) {
    case ExpressionType::Operands: {
        return {&AsOperands().operands[0], AsOperands().operand_count};
    }
    case ExpressionType::Comment:
    case ExpressionType::GetGuest: {
        break;
    }
    case ExpressionType::SetGuest: {
        return {&AsSetGuest().source, 1};
    }
    case ExpressionType::Phi: {
        return AsPhi().values;
    }
    default: {
        break;
    }
    }
    return {};
}

bool SSAInstruction::ExitsVM() const {
    switch (GetOpcode()) {
    case IROpcode::Syscall:
    case IROpcode::Cpuid:
    case IROpcode::Rdtsc:
    case IROpcode::Div128:
    case IROpcode::Divu128:
        return true;
    default:
        return false;
    }
}

void SSAInstruction::PropagateMovs() {
    auto replace_mov = [](SSAInstruction*& operand, int index) {
        if (operand->GetOpcode() != IROpcode::Mov) {
            return;
        }

        bool is_mov = true;
        SSAInstruction* value_final = operand->GetOperand(0);
        do {
            is_mov = false;
            if (value_final->GetOpcode() == IROpcode::Mov) {
                value_final = value_final->GetOperand(0);
                is_mov = true;
            }
        } while (is_mov);
        operand->RemoveUse();
        operand = value_final;
        operand->AddUse();
    };

    switch (expression_type) {
    case ExpressionType::Operands: {
        Operands& operands = AsOperands();
        for (u8 i = 0; i < operands.operand_count; i++) {
            replace_mov(operands.operands[i], i);
        }
        break;
    }
    case ExpressionType::GetGuest: {
        if (GetOpcode() == IROpcode::GetGuest) {
            ERROR("Shouldn't exist");
        }
        break;
    }
    case ExpressionType::SetGuest: {
        if (GetOpcode() == IROpcode::SetGuest) {
            ERROR("Shouldn't exist");
        } else if (GetOpcode() == IROpcode::StoreGuestToMemory) {
            replace_mov(AsSetGuest().source, 0);
        }
        break;
    }
    case ExpressionType::Phi: {
        Phi& phi = AsPhi();
        for (size_t i = 0; i < phi.blocks.size(); i++) {
            replace_mov(phi.values[i], i);
        }
        break;
    }
    case ExpressionType::Comment: {
        break;
    }
    default: {
        UNREACHABLE();
    }
    }
}

#define OP2(op) fmt::format("{} <- {} {} {}", GetNameString(name), GetNameString(operands[0]), #op, GetNameString(operands[1]))
#define OP2I(op) fmt::format("{} <- {} {} 0x{:x}", GetNameString(name), GetNameString(operands[0]), #op, (i64)immediate_data);
#define FOP(func) fmt::format("{} <- {}()", GetNameString(name), #func)
#define FOP1(func, param) fmt::format("{} <- {}({}: {})", GetNameString(name), #func, #param, GetNameString(operands[0]))
#define FOP2(func, param1, param2)                                                                                                                   \
    fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), #func, #param1, GetNameString(operands[0]), #param2, GetNameString(operands[1]))
#define VOP2(func, param1, param2) fmt::format("{}({}: {}, {}: {})", #func, #param1, GetNameString(operands[0]), #param2, GetNameString(operands[1]))

std::string ReducedInstruction::Print(const std::function<std::string(const ReducedInstruction*)>& callback) const {
    std::string ret;

    switch (opcode) {
    case IROpcode::Immediate: {
        ret += fmt::format("{} <- 0x{:x}", GetNameString(name), immediate_data);
        break;
    }
    case IROpcode::Select: {
        ret += fmt::format("{} <- {} ? {} : {}", GetNameString(name), GetNameString(operands[0]), GetNameString(operands[1]),
                           GetNameString(operands[2]));
        break;
    }
    case IROpcode::Mov: {
        ret += fmt::format("{} <- {}", GetNameString(name), GetNameString(operands[0]));
        break;
    }
    case IROpcode::Rdtsc: {
        ret += FOP(rdtsc);
        break;
    }
    case IROpcode::LoadGuestFromMemory: {
        ret += fmt::format("{} <- load_from_vm {}", GetNameString(name), print_guest_register(ref));
        break;
    }
    case IROpcode::StoreGuestToMemory: {
        ret += fmt::format("store_to_vm {}, {}", print_guest_register(ref), GetNameString(operands[0]));
        break;
    }
    case IROpcode::Add: {
        ret += OP2(+);
        break;
    }
    case IROpcode::Addi: {
        ret += OP2I(+);
        break;
    }
    case IROpcode::Sub: {
        ret += OP2(-);
        break;
    }
    case IROpcode::And: {
        ret += OP2(&);
        break;
    }
    case IROpcode::Or: {
        ret += OP2(|);
        break;
    }
    case IROpcode::Xor: {
        ret += OP2(^);
        break;
    }
    case IROpcode::ShiftLeft: {
        ret += OP2(<<);
        break;
    }
    case IROpcode::ShiftRight: {
        ret += OP2(>>);
        break;
    }
    case IROpcode::ShiftRightArithmetic: {
        ret += OP2(>>);
        break;
    }
    case IROpcode::Equal: {
        ret += OP2(==);
        break;
    }
    case IROpcode::NotEqual: {
        ret += OP2(!=);
        break;
    }
    case IROpcode::UGreaterThan: {
        ret += OP2(>);
        break;
    }
    case IROpcode::IGreaterThan: {
        ret += OP2(>);
        break;
    }
    case IROpcode::ULessThan: {
        ret += OP2(<);
        break;
    }
    case IROpcode::ILessThan: {
        ret += OP2(<);
        break;
    }
    case IROpcode::Mul:
    case IROpcode::Mulh:
    case IROpcode::Mulhu: {
        ret += OP2(*);
        break;
    }
    case IROpcode::LeftRotate8: {
        ret += FOP2(rol8, src, amount);
        break;
    }
    case IROpcode::LeftRotate16: {
        ret += FOP2(rol16, src, amount);
        break;
    }
    case IROpcode::LeftRotate32: {
        ret += FOP2(rol32, src, amount);
        break;
    }
    case IROpcode::LeftRotate64: {
        ret += FOP2(rol64, src, amount);
        break;
    }
    case IROpcode::Cpuid: {
        ret += FOP(cpuid);
        break;
    }
    case IROpcode::WriteByte: {
        ret += VOP2(write8, address, src);
        break;
    }
    case IROpcode::WriteWord: {
        ret += VOP2(write16, address, src);
        break;
    }
    case IROpcode::WriteDWord: {
        ret += VOP2(write32, address, src);
        break;
    }
    case IROpcode::WriteQWord: {
        ret += VOP2(write64, address, src);
        break;
    }
    case IROpcode::WriteXmmWord: {
        ret += VOP2(write128, address, src);
        break;
    }
    case IROpcode::Sext8: {
        ret += FOP1(sext8, src);
        break;
    }
    case IROpcode::Sext16: {
        ret += FOP1(sext16, src);
        break;
    }
    case IROpcode::Sext32: {
        ret += FOP1(sext32, src);
        break;
    }
    case IROpcode::CastIntegerToVector: {
        ret += FOP1(int_to_vec, integer);
        break;
    }
    case IROpcode::CastVectorToInteger: {
        ret += FOP1(vec_to_int, vector);
        break;
    }
    case IROpcode::Clz: {
        ret += FOP1(clz, src);
        break;
    }
    case IROpcode::Ctzh: {
        ret += FOP1(ctzh, src);
        break;
    }
    case IROpcode::Ctzw: {
        ret += FOP1(ctzw, src);
        break;
    }
    case IROpcode::Ctz: {
        ret += FOP1(ctz, src);
        break;
    }
    case IROpcode::Not: {
        ret += FOP1(not, src);
        break;
    }
    case IROpcode::Parity: {
        ret += FOP1(parity, src);
        break;
    }
    case IROpcode::ReadByte: {
        ret += FOP1(read8, address);
        break;
    }
    case IROpcode::ReadWord: {
        ret += FOP1(read16, address);
        break;
    }
    case IROpcode::ReadDWord: {
        ret += FOP1(read32, address);
        break;
    }
    case IROpcode::ReadQWord: {
        ret += FOP1(read64, address);
        break;
    }
    case IROpcode::ReadXmmWord: {
        ret += FOP1(read128, address);
        break;
    }
    case IROpcode::Div: {
        ret += FOP2(div, dividend, divisor);
        break;
    }
    case IROpcode::Divu: {
        ret += FOP2(divu, dividend, divisor);
        break;
    }
    case IROpcode::Divw: {
        ret += FOP2(divw, dividend, divisor);
        break;
    }
    case IROpcode::Divuw: {
        ret += FOP2(divuw, dividend, divisor);
        break;
    }
    case IROpcode::Rem: {
        ret += FOP2(rem, dividend, divisor);
        break;
    }
    case IROpcode::Remu: {
        ret += FOP2(remu, dividend, divisor);
        break;
    }
    case IROpcode::Remw: {
        ret += FOP2(remw, dividend, divisor);
        break;
    }
    case IROpcode::Remuw: {
        ret += FOP2(remuw, dividend, divisor);
        break;
    }
    case IROpcode::Div128: {
        ret += FOP1(div128, divisor);
        break;
    }
    case IROpcode::Divu128: {
        ret += FOP1(divu128, divisor);
        break;
    }
    case IROpcode::Syscall: {
        ret += FOP(syscall);
        break;
    }
    case IROpcode::VAnd: {
        ret += FOP2(vand, src1, src2);
        break;
    }
    case IROpcode::VOr: {
        ret += FOP2(vor, src1, src2);
        break;
    }
    case IROpcode::VXor: {
        ret += FOP2(vxor, src1, src2);
        break;
    }
    case IROpcode::VShiftLeft: {
        ret += FOP2(vshl, src1, src2);
        break;
    }
    case IROpcode::VShiftRight: {
        ret += FOP2(vshr, src1, src2);
        break;
    }
    case IROpcode::VZext64: {
        ret += FOP1(vzext64, src);
        break;
    }
    case IROpcode::VPackedAddQWord: {
        ret += FOP2(vpaddqword, src1, src2);
        break;
    }
    case IROpcode::VPackedEqualByte: {
        ret += FOP2(vpeqbyte, src1, src2);
        break;
    }
    case IROpcode::VPackedEqualWord: {
        ret += FOP2(vpeqword, src1, src2);
        break;
    }
    case IROpcode::VPackedEqualDWord: {
        ret += FOP2(vpeqdword, src1, src2);
        break;
    }
    case IROpcode::VPackedShuffleDWord: {
        ret += fmt::format("{} <- vpshufdword({}, 0x{:x})", name, operands[0], (u8)immediate_data);
        break;
    }
    case IROpcode::VPackedMinByte: {
        ret += FOP2(vpminbyte, src1, src2);
        break;
    }
    case IROpcode::VPackedSubByte: {
        ret += FOP2(vpsubbyte, src1, src2);
        break;
    }
    case IROpcode::VMoveByteMask: {
        ret += FOP1(vmovbytemask, src);
        break;
    }
    case IROpcode::VExtractInteger: {
        ret += FOP1(vextractint, src);
        break;
    }
    case IROpcode::VInsertInteger: {
        ret += FOP2(vinsertint, vector, integer);
        break;
    }
    case IROpcode::VUnpackByteLow: {
        ret += FOP2(vunpackbytelow, src1, src2);
        break;
    }
    case IROpcode::VUnpackWordLow: {
        ret += FOP2(vunpackwordlow, src1, src2);
        break;
    }
    case IROpcode::VUnpackDWordLow: {
        ret += FOP2(vunpackdwordlow, src1, src2);
        break;
    }
    case IROpcode::VUnpackQWordLow: {
        ret += FOP2(vunpackqwordlow, src1, src2);
        break;
    }
    default: {
        ERROR("Bad opcode: %d", (int)opcode);
    }
    }

    return ret;
}