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

        // If either are masked the mask at that time (v0) might have been different so we can't CSE
        // At least not naively.
        if (operands.masked == VecMask::Yes || other_operands.masked == VecMask::Yes) {
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
    case IROpcode::SetVectorStateFloat:
    case IROpcode::SetVectorStateDouble:
    case IROpcode::SetVectorStatePacked:
    case IROpcode::SetVectorStatePackedByte:
    case IROpcode::SetVectorStatePackedWord:
    case IROpcode::SetVectorStatePackedDWord:
    case IROpcode::SetVectorStatePackedQWord:
    case IROpcode::CallHostFunction:
    case IROpcode::SetExitReason:
    case IROpcode::Comment:
    case IROpcode::Syscall:
    case IROpcode::Cpuid:
    case IROpcode::Rdtsc:
    case IROpcode::Div128:
    case IROpcode::Divu128:
    case IROpcode::SetVMask: {
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
    case IROpcode::Shl:
    case IROpcode::Shli:
    case IROpcode::Shr:
    case IROpcode::Shri:
    case IROpcode::Sar:
    case IROpcode::Sari:
    case IROpcode::Rol8:
    case IROpcode::Rol16:
    case IROpcode::Rol32:
    case IROpcode::Rol64:
    case IROpcode::Ror8:
    case IROpcode::Ror16:
    case IROpcode::Ror32:
    case IROpcode::Ror64:
    case IROpcode::And:
    case IROpcode::Andi:
    case IROpcode::Or:
    case IROpcode::Ori:
    case IROpcode::Xor:
    case IROpcode::Xori:
    case IROpcode::Not:
    case IROpcode::Neg:
    case IROpcode::Seqz:
    case IROpcode::Snez:
    case IROpcode::Equal:
    case IROpcode::NotEqual:
    case IROpcode::SetLessThanSigned:
    case IROpcode::SetLessThanUnsigned:
    case IROpcode::ReadByte:
    case IROpcode::ReadWord:
    case IROpcode::ReadDWord:
    case IROpcode::ReadQWord:
    case IROpcode::VToI:
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
    case IROpcode::ReadWordRelative:
    case IROpcode::ReadDWordRelative:
    case IROpcode::ReadQWordRelative: {
        return IRType::Integer64;
    }
    case IROpcode::ReadXmmWord:
    case IROpcode::ReadXmmWordRelative:
    case IROpcode::IToV:
    case IROpcode::VAnd:
    case IROpcode::VOr:
    case IROpcode::VXor:
    case IROpcode::VSub:
    case IROpcode::VAdd:
    case IROpcode::VEqual:
    case IROpcode::VInsertInteger:
    case IROpcode::VSplat:
    case IROpcode::VSplati:
    case IROpcode::VMergei:
    case IROpcode::VGather:
    case IROpcode::VIota:
    case IROpcode::VSlli:
    case IROpcode::VSrai: {
        return IRType::Vector128;
    }
    case IROpcode::WriteByte:
    case IROpcode::WriteWord:
    case IROpcode::WriteDWord:
    case IROpcode::WriteQWord:
    case IROpcode::WriteXmmWord:
    case IROpcode::StoreGuestToMemory:
    case IROpcode::WriteByteRelative:
    case IROpcode::WriteWordRelative:
    case IROpcode::WriteDWordRelative:
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
        VALIDATE_OPS_INT(SetVectorStateFloat, 0);
        VALIDATE_OPS_INT(SetVectorStateDouble, 0);
        VALIDATE_OPS_INT(SetVectorStatePacked, 0);
        VALIDATE_OPS_INT(SetVectorStatePackedByte, 0);
        VALIDATE_OPS_INT(SetVectorStatePackedWord, 0);
        VALIDATE_OPS_INT(SetVectorStatePackedDWord, 0);
        VALIDATE_OPS_INT(SetVectorStatePackedQWord, 0);
        VALIDATE_OPS_INT(Rdtsc, 0);
        VALIDATE_OPS_INT(Syscall, 0);
        VALIDATE_OPS_INT(Cpuid, 0);
        VALIDATE_OPS_INT(SetExitReason, 0);
        VALIDATE_OPS_INT(CallHostFunction, 0);
        VALIDATE_OPS_INT(VSplati, 0);

        VALIDATE_OPS_INT(Neg, 1);
        VALIDATE_OPS_INT(Addi, 1);
        VALIDATE_OPS_INT(Andi, 1);
        VALIDATE_OPS_INT(Ori, 1);
        VALIDATE_OPS_INT(Xori, 1);
        VALIDATE_OPS_INT(Shli, 1);
        VALIDATE_OPS_INT(Shri, 1);
        VALIDATE_OPS_INT(Sari, 1);
        VALIDATE_OPS_INT(Seqz, 1);
        VALIDATE_OPS_INT(Snez, 1);
        VALIDATE_OPS_INT(Sext8, 1);
        VALIDATE_OPS_INT(Sext16, 1);
        VALIDATE_OPS_INT(Sext32, 1);
        VALIDATE_OPS_INT(Zext8, 1);
        VALIDATE_OPS_INT(Zext16, 1);
        VALIDATE_OPS_INT(Zext32, 1);
        VALIDATE_OPS_INT(IToV, 1);
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
        VALIDATE_OPS_INT(ReadWordRelative, 1);
        VALIDATE_OPS_INT(ReadDWordRelative, 1);
        VALIDATE_OPS_INT(ReadQWordRelative, 1);
        VALIDATE_OPS_INT(ReadXmmWordRelative, 1);
        VALIDATE_OPS_INT(Div128, 1);
        VALIDATE_OPS_INT(Divu128, 1);
        VALIDATE_OPS_INT(VSplat, 1);

        VALIDATE_OPS_INT(WriteByte, 2);
        VALIDATE_OPS_INT(WriteWord, 2);
        VALIDATE_OPS_INT(WriteDWord, 2);
        VALIDATE_OPS_INT(WriteQWord, 2);
        VALIDATE_OPS_INT(WriteByteRelative, 2);
        VALIDATE_OPS_INT(WriteWordRelative, 2);
        VALIDATE_OPS_INT(WriteDWordRelative, 2);
        VALIDATE_OPS_INT(WriteQWordRelative, 2);
        VALIDATE_OPS_INT(Add, 2);
        VALIDATE_OPS_INT(Sub, 2);
        VALIDATE_OPS_INT(Shl, 2);
        VALIDATE_OPS_INT(Shr, 2);
        VALIDATE_OPS_INT(Sar, 2);
        VALIDATE_OPS_INT(And, 2);
        VALIDATE_OPS_INT(Or, 2);
        VALIDATE_OPS_INT(Xor, 2);
        VALIDATE_OPS_INT(Equal, 2);
        VALIDATE_OPS_INT(NotEqual, 2);
        VALIDATE_OPS_INT(SetLessThanSigned, 2);
        VALIDATE_OPS_INT(SetLessThanUnsigned, 2);
        VALIDATE_OPS_INT(Rol8, 2);
        VALIDATE_OPS_INT(Rol16, 2);
        VALIDATE_OPS_INT(Rol32, 2);
        VALIDATE_OPS_INT(Rol64, 2);
        VALIDATE_OPS_INT(Ror8, 2);
        VALIDATE_OPS_INT(Ror16, 2);
        VALIDATE_OPS_INT(Ror32, 2);
        VALIDATE_OPS_INT(Ror64, 2);
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

        VALIDATE_OPS_VECTOR(VToI, 1);
        VALIDATE_OPS_VECTOR(SetVMask, 1);
        VALIDATE_OPS_VECTOR(VIota, 1);
        VALIDATE_OPS_VECTOR(VExtractInteger, 1);
        VALIDATE_OPS_VECTOR(VMergei, 1);
        VALIDATE_OPS_VECTOR(VSlli, 1);
        VALIDATE_OPS_VECTOR(VSrai, 1);

        VALIDATE_OPS_VECTOR(VAnd, 2);
        VALIDATE_OPS_VECTOR(VOr, 2);
        VALIDATE_OPS_VECTOR(VXor, 2);
        VALIDATE_OPS_VECTOR(VSub, 2);
        VALIDATE_OPS_VECTOR(VAdd, 2);
        VALIDATE_OPS_VECTOR(VEqual, 2);
        VALIDATE_OPS_VECTOR(VGather, 3);

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
        [[fallthrough]];
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
    case IROpcode::SetVectorStateFloat: {
        return fmt::format("SetVectorStateFloat()");
    }
    case IROpcode::SetVectorStateDouble: {
        return fmt::format("SetVectorStateDouble()");
    }
    case IROpcode::SetVectorStatePacked:
    case IROpcode::SetVectorStatePackedByte: {
        return fmt::format("SetVectorStatePackedByte()");
    }
    case IROpcode::SetVectorStatePackedWord: {
        return fmt::format("SetVectorStatePackedWord()");
    }
    case IROpcode::SetVectorStatePackedDWord: {
        return fmt::format("SetVectorStatePackedDWord()");
    }
    case IROpcode::SetVectorStatePackedQWord: {
        return fmt::format("SetVectorStatePackedQWord()");
    }
    case IROpcode::SetExitReason: {
        return fmt::format("SetExitReason({})", (u8)immediate_data);
    }
    case IROpcode::SetVMask: {
        return fmt::format("SetVMask({})", GetNameString(operands[0]));
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
    case IROpcode::CallHostFunction: {
        ret += fmt::format("{} <- call_host_function {}", GetNameString(name), immediate_data);
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
    case IROpcode::Andi: {
        ret += fmt::format("{} <- {} {} 0x{:x}", GetNameString(name), GetNameString(operands[0]), "&", (i64)immediate_data);
        break;
    }
    case IROpcode::Or: {
        ret += fmt::format("{} <- {} {} {}", GetNameString(name), GetNameString(operands[0]), "|", GetNameString(operands[1]));
        break;
    }
    case IROpcode::Ori: {
        ret += fmt::format("{} <- {} {} 0x{:x}", GetNameString(name), GetNameString(operands[0]), "|", (i64)immediate_data);
        break;
    }
    case IROpcode::Xor: {
        ret += fmt::format("{} <- {} {} {}", GetNameString(name), GetNameString(operands[0]), "^", GetNameString(operands[1]));
        break;
    }
    case IROpcode::Xori: {
        ret += fmt::format("{} <- {} {} 0x{:x}", GetNameString(name), GetNameString(operands[0]), "^", (i64)immediate_data);
        break;
    }
    case IROpcode::Seqz: {
        ret += fmt::format("{} <- {} {} {}", GetNameString(name), GetNameString(operands[0]), "==", 0);
        break;
    }
    case IROpcode::Snez: {
        ret += fmt::format("{} <- {} {} {}", GetNameString(name), GetNameString(operands[0]), "!=", 0);
        break;
    }
    case IROpcode::Shl: {
        ret += fmt::format("{} <- {} {} {}", GetNameString(name), GetNameString(operands[0]), "<<", GetNameString(operands[1]));
        break;
    }
    case IROpcode::Shli: {
        ret += fmt::format("{} <- {} {} 0x{:x}", GetNameString(name), GetNameString(operands[0]), "<<", (i64)immediate_data);
        break;
    }
    case IROpcode::Shr: {
        ret += fmt::format("{} <- {} {} {}", GetNameString(name), GetNameString(operands[0]), ">>", GetNameString(operands[1]));
        break;
    }
    case IROpcode::Shri: {
        ret += fmt::format("{} <- {} {} 0x{:x}", GetNameString(name), GetNameString(operands[0]), ">>", (i64)immediate_data);
        break;
    }
    case IROpcode::Sar: {
        ret += fmt::format("{} <- {} {} {}", GetNameString(name), GetNameString(operands[0]), ">>", GetNameString(operands[1]));
        break;
    }
    case IROpcode::Sari: {
        ret += fmt::format("{} <- {} {} 0x{:x}", GetNameString(name), GetNameString(operands[0]), ">>", (i64)immediate_data);
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
    case IROpcode::Rol8: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "rol8", "src", GetNameString(operands[0]), "amount",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::Rol16: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "rol16", "src", GetNameString(operands[0]), "amount",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::Rol32: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "rol32", "src", GetNameString(operands[0]), "amount",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::Rol64: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "rol64", "src", GetNameString(operands[0]), "amount",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::Ror8: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "ror8", "src", GetNameString(operands[0]), "amount",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::Ror16: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "ror16", "src", GetNameString(operands[0]), "amount",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::Ror32: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "ror32", "src", GetNameString(operands[0]), "amount",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::Ror64: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "ror64", "src", GetNameString(operands[0]), "amount",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::Cpuid: {
        ret += fmt::format("CPUID()");
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
    case IROpcode::IToV: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "int_to_vec", "integer", GetNameString(operands[0]));
        break;
    }
    case IROpcode::VToI: {
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
    case IROpcode::ReadWordRelative: {
        ret += fmt::format("{} <- {}({}: {} + 0x{:x})", GetNameString(name), "read16", "address", GetNameString(operands[0]), immediate_data);
        break;
    }
    case IROpcode::ReadDWordRelative: {
        ret += fmt::format("{} <- {}({}: {} + 0x{:x})", GetNameString(name), "read32", "address", GetNameString(operands[0]), immediate_data);
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
    case IROpcode::WriteWordRelative: {
        ret += fmt::format("{}({}: {} + 0x{:x}, {}: {})", "write16", "address", GetNameString(operands[0]), immediate_data, "src",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::WriteDWordRelative: {
        ret += fmt::format("{}({}: {} + 0x{:x}, {}: {})", "write32", "address", GetNameString(operands[0]), immediate_data, "src",
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
    case IROpcode::VAdd: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vadd", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VIota: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "viota", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::VSplat: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "vsplat", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::VSplati: {
        ret += fmt::format("{} <- {}(0x{:x})", GetNameString(name), "vsplati", immediate_data);
        break;
    }
    case IROpcode::VMergei: {
        ret += fmt::format("{} <- {}({}: {}, 0x{:x})", GetNameString(name), "vmergei", "false_value", GetNameString(operands[0]), immediate_data);
        break;
    }
    case IROpcode::VSlli: {
        ret += fmt::format("{} <- {}({}: {}, 0x{:x})", GetNameString(name), "vslli", "src", GetNameString(operands[0]), immediate_data);
        break;
    }
    case IROpcode::VSrai: {
        ret += fmt::format("{} <- {}({}: {}, 0x{:x})", GetNameString(name), "vsrai", "src", GetNameString(operands[0]), immediate_data);
        break;
    }
    case IROpcode::VGather: {
        ret += fmt::format("{} <- {}({}: {}, {}: {}, {}: {}) ", GetNameString(name), "vgather", "dst", GetNameString(operands[0]), "src",
                           GetNameString(operands[1]), "iota", GetNameString(operands[2]));
        break;
    }
    case IROpcode::VEqual: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vequal", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VSub: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vsub", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
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