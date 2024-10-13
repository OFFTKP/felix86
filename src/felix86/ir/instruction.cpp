#include <fmt/format.h>
#include "felix86/common/log.hpp"
#include "felix86/common/print.hpp"
#include "felix86/ir/block.hpp"
#include "felix86/ir/instruction.hpp"

bool SSAInstruction::IsSameExpression(const SSAInstruction& other) const {
    if (expression_type != other.expression_type) {
        return false;
    }

    if (opcode != other.opcode) {
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
    default:
        return false;
    }
}

IRType SSAInstruction::GetTypeFromOpcode(IROpcode opcode, x86_ref_e ref) {
    switch (opcode) {
    case IROpcode::Mov: {
        ERROR("Should not be used with Mov");
        return IRType::Void;
    }
    case IROpcode::StoreSpill:
    case IROpcode::LoadSpill: {
        ERROR("Should not be used with LoadSpill");
        return IRType::Void;
    }
    case IROpcode::Null:
    case IROpcode::SetExitReason:
    case IROpcode::Comment:
    case IROpcode::Syscall:
    case IROpcode::Cpuid:
    case IROpcode::Rdtsc:
    case IROpcode::Div128:
    case IROpcode::Divu128: {
        return IRType::Void;
    }
    case IROpcode::GetThreadStatePointer:
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
    case IROpcode::Neg:
    case IROpcode::Equal:
    case IROpcode::NotEqual:
    case IROpcode::SetLessThanSigned:
    case IROpcode::SetLessThanUnsigned:
    case IROpcode::ReadByte:
    case IROpcode::ReadWord:
    case IROpcode::ReadDWord:
    case IROpcode::ReadQWord:
    case IROpcode::CastIntegerFromVector:
    case IROpcode::VExtractInteger:
    case IROpcode::Sext8:
    case IROpcode::Sext16:
    case IROpcode::Sext32:
    case IROpcode::Zext8:
    case IROpcode::Zext16:
    case IROpcode::Zext32:
    case IROpcode::Div:
    case IROpcode::Divu:
    case IROpcode::Divw:
    case IROpcode::Divuw:
    case IROpcode::Rem:
    case IROpcode::Remu:
    case IROpcode::Remw:
    case IROpcode::Remuw:
    case IROpcode::Mul:
    case IROpcode::Mulh:
    case IROpcode::Mulhu:
    case IROpcode::AmoAdd8:
    case IROpcode::AmoAdd16:
    case IROpcode::AmoAdd32:
    case IROpcode::AmoAdd64:
    case IROpcode::AmoAnd8:
    case IROpcode::AmoAnd16:
    case IROpcode::AmoAnd32:
    case IROpcode::AmoAnd64:
    case IROpcode::AmoOr8:
    case IROpcode::AmoOr16:
    case IROpcode::AmoOr32:
    case IROpcode::AmoOr64:
    case IROpcode::AmoXor8:
    case IROpcode::AmoXor16:
    case IROpcode::AmoXor32:
    case IROpcode::AmoXor64:
    case IROpcode::AmoSwap8:
    case IROpcode::AmoSwap16:
    case IROpcode::AmoSwap32:
    case IROpcode::AmoSwap64:
    case IROpcode::AmoCAS8:
    case IROpcode::AmoCAS16:
    case IROpcode::AmoCAS32:
    case IROpcode::AmoCAS64:
    case IROpcode::AmoCAS128:
    case IROpcode::ReadByteRelative:
    case IROpcode::ReadQWordRelative: {
        return IRType::Integer64;
    }
    case IROpcode::ReadXmmWord:
    case IROpcode::ReadXmmWordRelative:
    case IROpcode::CastVectorFromInteger:
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
    case IROpcode::StoreGuestToMemory:
    case IROpcode::WriteByteRelative:
    case IROpcode::WriteQWordRelative:
    case IROpcode::WriteXmmWordRelative: {
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
            return IRType::Float64;
        case X86_REF_CF ... X86_REF_OF:
            return IRType::Integer64;
        case X86_REF_XMM0 ... X86_REF_XMM15:
            return IRType::Vector128;
        default:
            ERROR("Invalid register reference: %d", static_cast<u8>(ref));
            return IRType::Void;
        }
    }
    case IROpcode::Count: {
        UNREACHABLE();
        return IRType::Void;
    }
    }

    UNREACHABLE();
    return IRType::Void;
}

void SSAInstruction::Invalidate() {
    if (locked) {
        ERROR("Tried to invalidate locked instruction");
    }

    for (SSAInstruction* used : GetUsedInstructions()) {
        used->RemoveUse();
    }
}

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
    case IROpcode::Null:
    case IROpcode::LoadSpill:
    case IROpcode::StoreSpill: {
        ERROR("Null should not be used");
        break;
    }

        BAD(Count);
        BAD(Mov);
        BAD(Phi);
        BAD(GetGuest);
        BAD(SetGuest);
        BAD(LoadGuestFromMemory);
        BAD(StoreGuestToMemory);
        BAD(Comment);
        BAD(Immediate);
        BAD(AmoCAS128); // implme

        VALIDATE_OPS_INT(GetThreadStatePointer, 0);
        VALIDATE_OPS_INT(Rdtsc, 0);
        VALIDATE_OPS_INT(Syscall, 0);
        VALIDATE_OPS_INT(Cpuid, 0);
        VALIDATE_OPS_INT(SetExitReason, 0);

        VALIDATE_OPS_INT(Neg, 1);
        VALIDATE_OPS_INT(Addi, 1);
        VALIDATE_OPS_INT(Sext8, 1);
        VALIDATE_OPS_INT(Sext16, 1);
        VALIDATE_OPS_INT(Sext32, 1);
        VALIDATE_OPS_INT(Zext8, 1);
        VALIDATE_OPS_INT(Zext16, 1);
        VALIDATE_OPS_INT(Zext32, 1);
        VALIDATE_OPS_INT(CastVectorFromInteger, 1);
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
        VALIDATE_OPS_INT(ReadByteRelative, 1);
        VALIDATE_OPS_INT(ReadQWordRelative, 1);
        VALIDATE_OPS_INT(ReadXmmWordRelative, 1);
        VALIDATE_OPS_INT(Div128, 1);
        VALIDATE_OPS_INT(Divu128, 1);

        VALIDATE_OPS_INT(WriteByte, 2);
        VALIDATE_OPS_INT(WriteWord, 2);
        VALIDATE_OPS_INT(WriteDWord, 2);
        VALIDATE_OPS_INT(WriteQWord, 2);
        VALIDATE_OPS_INT(WriteByteRelative, 2);
        VALIDATE_OPS_INT(WriteQWordRelative, 2);
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
        VALIDATE_OPS_INT(SetLessThanSigned, 2);
        VALIDATE_OPS_INT(SetLessThanUnsigned, 2);
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
        VALIDATE_OPS_INT(AmoAdd8, 2);
        VALIDATE_OPS_INT(AmoAdd16, 2);
        VALIDATE_OPS_INT(AmoAdd32, 2);
        VALIDATE_OPS_INT(AmoAdd64, 2);
        VALIDATE_OPS_INT(AmoAnd8, 2);
        VALIDATE_OPS_INT(AmoAnd16, 2);
        VALIDATE_OPS_INT(AmoAnd32, 2);
        VALIDATE_OPS_INT(AmoAnd64, 2);
        VALIDATE_OPS_INT(AmoOr8, 2);
        VALIDATE_OPS_INT(AmoOr16, 2);
        VALIDATE_OPS_INT(AmoOr32, 2);
        VALIDATE_OPS_INT(AmoOr64, 2);
        VALIDATE_OPS_INT(AmoXor8, 2);
        VALIDATE_OPS_INT(AmoXor16, 2);
        VALIDATE_OPS_INT(AmoXor32, 2);
        VALIDATE_OPS_INT(AmoXor64, 2);
        VALIDATE_OPS_INT(AmoSwap8, 2);
        VALIDATE_OPS_INT(AmoSwap16, 2);
        VALIDATE_OPS_INT(AmoSwap32, 2);
        VALIDATE_OPS_INT(AmoSwap64, 2);

        VALIDATE_OPS_INT(Select, 3);
        VALIDATE_OPS_INT(AmoCAS8, 3);
        VALIDATE_OPS_INT(AmoCAS16, 3);
        VALIDATE_OPS_INT(AmoCAS32, 3);
        VALIDATE_OPS_INT(AmoCAS64, 3);

        VALIDATE_OPS_VECTOR(CastIntegerFromVector, 1);
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
    case IROpcode::WriteXmmWordRelative:
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
    case IRType::Void: {
        return "Void";
    }
    default: {
        UNREACHABLE();
        return "";
    }
    }
}

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

bool SSAInstruction::PropagateMovs() {
    bool replaced_something = false;
    auto replace_mov = [&replaced_something](SSAInstruction*& operand, int index) {
        if (operand->GetOpcode() != IROpcode::Mov) {
            return;
        }

        bool is_mov = true;
        replaced_something = true;
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
        if (opcode == IROpcode::Mov) {
            break;
        }

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

    return replaced_something;
}

std::string Print(IROpcode opcode, x86_ref_e ref, u32 name, const u32* operands, u64 immediate_data) {
    std::string ret;

    switch (opcode) {
    case IROpcode::Count: {
        UNREACHABLE();
    }
    case IROpcode::Phi:
    case IROpcode::Comment:
    case IROpcode::SetGuest:
    case IROpcode::GetGuest: {
        return "Bad print type???";
    }
    case IROpcode::Null: {
        return "Null";
    }
    case IROpcode::LoadSpill: {
        return fmt::format("{} <- LoadSpill 0x{:x}", GetNameString(name), immediate_data);
    }
    case IROpcode::StoreSpill: {
        return fmt::format("StoreSpill 0x{:x}, {}", immediate_data, GetNameString(operands[0]));
    }
    case IROpcode::GetThreadStatePointer: {
        return fmt::format("{} <- ThreadStatePointer", GetNameString(name));
    }
    case IROpcode::SetExitReason: {
        return fmt::format("SetExitReason({})", (u8)immediate_data);
    }
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
        ret += fmt::format("{} <- {}()", GetNameString(name), "rdtsc");
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
        ret += fmt::format("{} <- {} {} {}", GetNameString(name), GetNameString(operands[0]), "+", GetNameString(operands[1]));
        break;
    }
    case IROpcode::Addi: {
        ret += fmt::format("{} <- {} {} 0x{:x}", GetNameString(name), GetNameString(operands[0]), "+", (i64)immediate_data);
        ;
        break;
    }
    case IROpcode::Sub: {
        ret += fmt::format("{} <- {} {} {}", GetNameString(name), GetNameString(operands[0]), "-", GetNameString(operands[1]));
        break;
    }
    case IROpcode::And: {
        ret += fmt::format("{} <- {} {} {}", GetNameString(name), GetNameString(operands[0]), "&", GetNameString(operands[1]));
        break;
    }
    case IROpcode::Or: {
        ret += fmt::format("{} <- {} {} {}", GetNameString(name), GetNameString(operands[0]), "|", GetNameString(operands[1]));
        break;
    }
    case IROpcode::Xor: {
        ret += fmt::format("{} <- {} {} {}", GetNameString(name), GetNameString(operands[0]), "^", GetNameString(operands[1]));
        break;
    }
    case IROpcode::ShiftLeft: {
        ret += fmt::format("{} <- {} {} {}", GetNameString(name), GetNameString(operands[0]), "<<", GetNameString(operands[1]));
        break;
    }
    case IROpcode::ShiftRight: {
        ret += fmt::format("{} <- {} {} {}", GetNameString(name), GetNameString(operands[0]), ">>", GetNameString(operands[1]));
        break;
    }
    case IROpcode::ShiftRightArithmetic: {
        ret += fmt::format("{} <- {} {} {}", GetNameString(name), GetNameString(operands[0]), ">>", GetNameString(operands[1]));
        break;
    }
    case IROpcode::AmoAdd8: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "amoadd8", "address", GetNameString(operands[0]), "src",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::AmoAdd16: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "amoadd16", "address", GetNameString(operands[0]), "src",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::AmoAdd32: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "amoadd32", "address", GetNameString(operands[0]), "src",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::AmoAdd64: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "amoadd64", "address", GetNameString(operands[0]), "src",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::AmoAnd8: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "amoand8", "address", GetNameString(operands[0]), "src",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::AmoAnd16: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "amoand16", "address", GetNameString(operands[0]), "src",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::AmoAnd32: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "amoand32", "address", GetNameString(operands[0]), "src",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::AmoAnd64: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "amoand64", "address", GetNameString(operands[0]), "src",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::AmoOr8: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "amoor8", "address", GetNameString(operands[0]), "src",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::AmoOr16: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "amoor16", "address", GetNameString(operands[0]), "src",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::AmoOr32: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "amoor32", "address", GetNameString(operands[0]), "src",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::AmoOr64: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "amoor64", "address", GetNameString(operands[0]), "src",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::AmoXor8: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "amoxor8", "address", GetNameString(operands[0]), "src",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::AmoXor16: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "amoxor16", "address", GetNameString(operands[0]), "src",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::AmoXor32: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "amoxor32", "address", GetNameString(operands[0]), "src",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::AmoXor64: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "amoxor64", "address", GetNameString(operands[0]), "src",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::AmoSwap8: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "amoswap8", "address", GetNameString(operands[0]), "src",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::AmoSwap16: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "amoswap16", "address", GetNameString(operands[0]), "src",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::AmoSwap32: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "amoswap32", "address", GetNameString(operands[0]), "src",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::AmoSwap64: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "amoswap64", "address", GetNameString(operands[0]), "src",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::AmoCAS8: {
        ret += fmt::format("{} <- {}({}: {}, {}: {}, {}: {})", GetNameString(name), "amocas8", "address", GetNameString(operands[0]), "expected",
                           GetNameString(operands[1]), "src", GetNameString(operands[2]));
        break;
    }
    case IROpcode::AmoCAS16: {
        ret += fmt::format("{} <- {}({}: {}, {}: {}, {}: {})", GetNameString(name), "amocas16", "address", GetNameString(operands[0]), "expected",
                           GetNameString(operands[1]), "src", GetNameString(operands[2]));
        break;
    }
    case IROpcode::AmoCAS32: {
        ret += fmt::format("{} <- {}({}: {}, {}: {}, {}: {})", GetNameString(name), "amocas32", "address", GetNameString(operands[0]), "expected",
                           GetNameString(operands[1]), "src", GetNameString(operands[2]));
        break;
    }
    case IROpcode::AmoCAS64: {
        ret += fmt::format("{} <- {}({}: {}, {}: {}, {}: {})", GetNameString(name), "amocas64", "address", GetNameString(operands[0]), "expected",
                           GetNameString(operands[1]), "src", GetNameString(operands[2]));
        break;
    }
    case IROpcode::AmoCAS128: {
        ret += fmt::format("{} <- {}({}: {}, {}: {}, {}: {})", GetNameString(name), "amocas128", "address", GetNameString(operands[0]), "expected",
                           GetNameString(operands[1]), "src", GetNameString(operands[2]));
        break;
    }
    case IROpcode::Equal: {
        ret += fmt::format("{} <- {} {} {}", GetNameString(name), GetNameString(operands[0]), "==", GetNameString(operands[1]));
        break;
    }
    case IROpcode::NotEqual: {
        ret += fmt::format("{} <- {} {} {}", GetNameString(name), GetNameString(operands[0]), "!=", GetNameString(operands[1]));
        break;
    }
    case IROpcode::SetLessThanUnsigned: {
        ret += fmt::format("{} <- {} {} {}", GetNameString(name), GetNameString(operands[0]), "<", GetNameString(operands[1]));
        break;
    }
    case IROpcode::SetLessThanSigned: {
        ret += fmt::format("{} <- {} {} {}", GetNameString(name), GetNameString(operands[0]), "<", GetNameString(operands[1]));
        break;
    }
    case IROpcode::Neg: {
        ret += fmt::format("{} <- {} {}", GetNameString(name), "-", GetNameString(operands[0]));
        break;
    }
    case IROpcode::Mul:
    case IROpcode::Mulh:
    case IROpcode::Mulhu: {
        ret += fmt::format("{} <- {} {} {}", GetNameString(name), GetNameString(operands[0]), "*", GetNameString(operands[1]));
        break;
    }
    case IROpcode::LeftRotate8: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "rol8", "src", GetNameString(operands[0]), "amount",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::LeftRotate16: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "rol16", "src", GetNameString(operands[0]), "amount",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::LeftRotate32: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "rol32", "src", GetNameString(operands[0]), "amount",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::LeftRotate64: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "rol64", "src", GetNameString(operands[0]), "amount",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::Cpuid: {
        ret += fmt::format("{} <- {}()", GetNameString(name), "cpuid");
        break;
    }
    case IROpcode::WriteByte: {
        ret += fmt::format("{}({}: {}, {}: {})", "write8", "address", GetNameString(operands[0]), "src", GetNameString(operands[1]));
        break;
    }
    case IROpcode::WriteWord: {
        ret += fmt::format("{}({}: {}, {}: {})", "write16", "address", GetNameString(operands[0]), "src", GetNameString(operands[1]));
        break;
    }
    case IROpcode::WriteDWord: {
        ret += fmt::format("{}({}: {}, {}: {})", "write32", "address", GetNameString(operands[0]), "src", GetNameString(operands[1]));
        break;
    }
    case IROpcode::WriteQWord: {
        ret += fmt::format("{}({}: {}, {}: {})", "write64", "address", GetNameString(operands[0]), "src", GetNameString(operands[1]));
        break;
    }
    case IROpcode::WriteXmmWord: {
        ret += fmt::format("{}({}: {}, {}: {})", "write128", "address", GetNameString(operands[0]), "src", GetNameString(operands[1]));
        break;
    }
    case IROpcode::Sext8: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "sext8", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::Sext16: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "sext16", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::Sext32: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "sext32", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::Zext8: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "zext8", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::Zext16: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "zext16", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::Zext32: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "zext32", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::CastVectorFromInteger: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "int_to_vec", "integer", GetNameString(operands[0]));
        break;
    }
    case IROpcode::CastIntegerFromVector: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "vec_to_int", "vector", GetNameString(operands[0]));
        break;
    }
    case IROpcode::Clz: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "clz", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::Ctzh: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "ctzh", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::Ctzw: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "ctzw", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::Ctz: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "ctz", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::Not: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "not", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::Parity: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "parity", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::ReadByte: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "read8", "address", GetNameString(operands[0]));
        break;
    }
    case IROpcode::ReadWord: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "read16", "address", GetNameString(operands[0]));
        break;
    }
    case IROpcode::ReadDWord: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "read32", "address", GetNameString(operands[0]));
        break;
    }
    case IROpcode::ReadQWord: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "read64", "address", GetNameString(operands[0]));
        break;
    }
    case IROpcode::ReadXmmWord: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "read128", "address", GetNameString(operands[0]));
        break;
    }
    case IROpcode::ReadByteRelative: {
        ret += fmt::format("{} <- {}({}: {} + 0x{:x})", GetNameString(name), "read8", "address", GetNameString(operands[0]), immediate_data);
        break;
    }
    case IROpcode::ReadQWordRelative: {
        ret += fmt::format("{} <- {}({}: {} + 0x{:x})", GetNameString(name), "read64", "address", GetNameString(operands[0]), immediate_data);
        break;
    }
    case IROpcode::ReadXmmWordRelative: {
        ret += fmt::format("{} <- {}({}: {} + 0x{:x})", GetNameString(name), "read128", "address", GetNameString(operands[0]), immediate_data);
        break;
    }
    case IROpcode::WriteByteRelative: {
        ret += fmt::format("{}({}: {} + 0x{:x}, {}: {})", "write8", "address", GetNameString(operands[0]), immediate_data, "src",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::WriteQWordRelative: {
        ret += fmt::format("{}({}: {} + 0x{:x}, {}: {})", "write64", "address", GetNameString(operands[0]), immediate_data, "src",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::WriteXmmWordRelative: {
        ret += fmt::format("{}({}: {} + 0x{:x}, {}: {})", "write128", "address", GetNameString(operands[0]), immediate_data, "src",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::Div: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "div", "dividend", GetNameString(operands[0]), "divisor",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::Divu: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "divu", "dividend", GetNameString(operands[0]), "divisor",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::Divw: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "divw", "dividend", GetNameString(operands[0]), "divisor",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::Divuw: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "divuw", "dividend", GetNameString(operands[0]), "divisor",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::Rem: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "rem", "dividend", GetNameString(operands[0]), "divisor",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::Remu: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "remu", "dividend", GetNameString(operands[0]), "divisor",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::Remw: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "remw", "dividend", GetNameString(operands[0]), "divisor",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::Remuw: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "remuw", "dividend", GetNameString(operands[0]), "divisor",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::Div128: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "div128", "divisor", GetNameString(operands[0]));
        break;
    }
    case IROpcode::Divu128: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "divu128", "divisor", GetNameString(operands[0]));
        break;
    }
    case IROpcode::Syscall: {
        ret += fmt::format("{} <- {}()", GetNameString(name), "syscall");
        break;
    }
    case IROpcode::VAnd: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vand", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VOr: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vor", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VXor: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vxor", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VShiftLeft: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vshl", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VShiftRight: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vshr", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VZext64: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "vzext64", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::VPackedAddQWord: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vpaddqword", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VPackedEqualByte: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vpeqbyte", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VPackedEqualWord: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vpeqword", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VPackedEqualDWord: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vpeqdword", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VPackedShuffleDWord: {
        ret += fmt::format("{} <- vpshufdword({}, 0x{:x})", name, operands[0], (u8)immediate_data);
        break;
    }
    case IROpcode::VPackedMinByte: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vpminbyte", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VPackedSubByte: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vpsubbyte", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VMoveByteMask: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "vmovbytemask", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::VExtractInteger: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "vextractint", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::VInsertInteger: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vinsertint", "vector", GetNameString(operands[0]), "integer",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VUnpackByteLow: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vunpackbytelow", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VUnpackWordLow: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vunpackwordlow", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VUnpackDWordLow: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vunpackdwordlow", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VUnpackQWordLow: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vunpackqwordlow", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    }

    return ret;
}

std::string SSAInstruction::Print(const std::function<std::string(const SSAInstruction*)>& callback) const {
    IROpcode opcode = GetOpcode();
    std::string ret;

    x86_ref_e ref = X86_REF_COUNT;
    if (IsSetGuest()) {
        ref = AsSetGuest().ref;
    } else if (IsGetGuest()) {
        ref = AsGetGuest().ref;
    }

    std::array<u32, 4> operands;
    u8 operand_count = 0;
    u64 immediate_data = 0;

    if (IsOperands()) {
        operand_count = GetOperandCount();
        immediate_data = GetImmediateData();
        for (int i = 0; i < operand_count; i++) {
            operands[i] = GetOperandName(i);
        }
    }

    switch (opcode) {
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
    default: {
        ret += ::Print(GetOpcode(), ref, GetName(), operands.data(), immediate_data);
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