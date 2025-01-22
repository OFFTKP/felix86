#include <fmt/format.h>
#include "felix86/common/log.hpp"
#include "felix86/common/print.hpp"
#include "felix86/ir/block.hpp"
#include "felix86/ir/instruction.hpp"

struct IROpcodeMetadata {
    IROpcode opcode;
    IRType return_type;
    std::array<IRType, 4> operand_types;
    u8 operand_count;
};

std::vector<IROpcodeMetadata> metadata = {
#define X(name, return_type, ...) {IROpcode::name, IRType::return_type, {__VA_ARGS__}, sizeof((u32[]){__VA_ARGS__}) / sizeof(u32)},
#include "felix86/ir/opcodes.inc"
#undef X
};

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

        // If either are masked the mask at that time (v0) might have been different so we can't CSE
        // At least not naively.
        if (operands.masked == VecMask::Yes || other_operands.masked == VecMask::Yes) {
            return false;
        }

        // The vector state is global and if it's different we can't optimize them away
        if (operands.vector_state != other_operands.vector_state) {
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
    case IROpcode::Null:
    case IROpcode::LoadSpill:
    case IROpcode::StoreSpill:
    case IROpcode::Count: {
        UNREACHABLE();
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
            return IRType::Vector128;
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
        break;
    }
    }

    switch (opcode) {
#define X(name, return_type, ...)                                                                                                                    \
    case IROpcode::name:                                                                                                                             \
        return IRType::return_type;
#include "felix86/ir/opcodes.inc"
#undef X
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

#define BAD(opcode)                                                                                                                                  \
    case IROpcode::opcode:                                                                                                                           \
        ERROR("Invalid opcode %d", static_cast<u8>(IROpcode::opcode));                                                                               \
        break

void SSAInstruction::checkValidity(IROpcode opcode, const Operands& operands) {
    switch (opcode) {
    case IROpcode::Null:
    case IROpcode::LoadSpill:
    case IROpcode::StoreSpill:
    case IROpcode::Count:
    case IROpcode::Mov:
    case IROpcode::Phi:
    case IROpcode::GetGuest:
    case IROpcode::SetGuest:
    case IROpcode::LoadGuestFromMemory:
    case IROpcode::StoreGuestToMemory:
    case IROpcode::Comment:
    case IROpcode::Immediate:
    case IROpcode::AmoCAS128: {
        ERROR("Opcode %d shouldn't be used here", static_cast<u8>(opcode));
        break;
    }
    default: {
        break;
    }
    }

    switch (opcode) {
#define X(name, ...)                                                                                                                                 \
    case IROpcode::name: {                                                                                                                           \
        auto& meta = metadata[(u8)IROpcode::name];                                                                                                   \
        if (meta.opcode != IROpcode::name) {                                                                                                         \
            ERROR("Invalid opcode %d", static_cast<u8>(IROpcode::name));                                                                             \
        }                                                                                                                                            \
        if (meta.operand_count != operands.operand_count) {                                                                                          \
            ERROR("Invalid operand count for %s", Opcode::GetOpcodeString(IROpcode::name).c_str());                                                  \
        }                                                                                                                                            \
        for (u8 i = 0; i < meta.operand_count; i++) {                                                                                                \
            if (meta.operand_types[i] != operands.operands[i]->GetType()) {                                                                          \
                ERROR("Invalid operand %d for %s", i, Opcode::GetOpcodeString(IROpcode::name).c_str());                                              \
            }                                                                                                                                        \
        }                                                                                                                                            \
        break;                                                                                                                                       \
    }
#include "felix86/ir/opcodes.inc"
#undef X
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
    case IROpcode::SetVectorStateFloatBytes: {
        return fmt::format("SetVectorStateFloatBytes()");
    }
    case IROpcode::SetVectorStateDoubleBytes: {
        return fmt::format("SetVectorStateDoubleBytes()");
    }
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
    case IROpcode::Fence: {
        return fmt::format("Fence({}, {})", (u8)immediate_data >> 4, (u8)immediate_data & 0xF);
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
    case IROpcode::Jump: {
        ret += fmt::format("jump");
        break;
    }
    case IROpcode::JumpConditional: {
        ret += fmt::format("jump if {}", GetNameString(operands[0]));
        break;
    }
    case IROpcode::BackToDispatcher: {
        ret += fmt::format("back_to_dispatcher");
        break;
    }
    case IROpcode::BSwap32: {
        ret += fmt::format("{} <- bswap32 {}", GetNameString(name), GetNameString(operands[0]));
        break;
    }
    case IROpcode::BSwap64: {
        ret += fmt::format("{} <- bswap64 {}", GetNameString(name), GetNameString(operands[0]));
        break;
    }
    case IROpcode::Add: {
        ret += fmt::format("{} <- {} {} {}", GetNameString(name), GetNameString(operands[0]), "+", GetNameString(operands[1]));
        break;
    }
    case IROpcode::AddShifted: {
        ret +=
            fmt::format("{} <- {} {} ({} << {}) ", GetNameString(name), GetNameString(operands[0]), "+", GetNameString(operands[1]), immediate_data);
        break;
    }
    case IROpcode::Addi: {
        ret += fmt::format("{} <- {} {} 0x{:x}", GetNameString(name), GetNameString(operands[0]), "+", (i64)immediate_data);
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
    case IROpcode::LoadReserved32: {
        ret += fmt::format("{} <- lr.w {}", GetNameString(name), GetNameString(operands[0]));
        break;
    }
    case IROpcode::LoadReserved64: {
        ret += fmt::format("{} <- lr.d {}", GetNameString(name), GetNameString(operands[0]));
        break;
    }
    case IROpcode::StoreConditional32: {
        ret += fmt::format("sc.w {}, {}", GetNameString(operands[0]), GetNameString(operands[1]));
        break;
    }
    case IROpcode::StoreConditional64: {
        ret += fmt::format("sc.d {}, {}", GetNameString(operands[0]), GetNameString(operands[1]));
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
    case IROpcode::Ctz: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "ctz", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::Not: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "not", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::CZeroEqz: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "czero_eqz", "src", GetNameString(operands[0]), "cond",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::CZeroNez: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "czero_nez", "src", GetNameString(operands[0]), "cond",
                           GetNameString(operands[1]));
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
    case IROpcode::VMin: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vmin", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VMinu: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vminu", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VMax: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vmax", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VMaxu: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vmaxu", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VXori: {
        ret += fmt::format("{} <- {}({}: {}, 0x{:x})", GetNameString(name), "vxori", "src", GetNameString(operands[0]), immediate_data);
        break;
    }
    case IROpcode::VAdd: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vadd", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VAddi: {
        ret += fmt::format("{} <- {}({}: {}, 0x{:x})", GetNameString(name), "vaddi", "src", GetNameString(operands[0]), immediate_data);
        break;
    }
    case IROpcode::VId: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "vid", "src", GetNameString(operands[0]));
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
    case IROpcode::VMerge: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vmerge", "true_value", GetNameString(operands[0]), "false_value",
                           GetNameString(operands[1]));
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
    case IROpcode::VSll: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vsll", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VSrli: {
        ret += fmt::format("{} <- {}({}: {}, 0x{:x})", GetNameString(name), "vsrli", "src", GetNameString(operands[0]), immediate_data);
        break;
    }
    case IROpcode::VSrl: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vsrl", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VSrai: {
        ret += fmt::format("{} <- {}({}: {}, 0x{:x})", GetNameString(name), "vsrai", "src", GetNameString(operands[0]), immediate_data);
        break;
    }
    case IROpcode::VMSeqi: {
        ret += fmt::format("{} <- {}({}: {}, 0x{:x})", GetNameString(name), "vmseqi", "src", GetNameString(operands[0]), immediate_data);
        break;
    }
    case IROpcode::VMSlt: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "VMSlt", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VSlideDowni: {
        ret += fmt::format("{} <- {}({}: {}, 0x{:x})", GetNameString(name), "vslidedowni", "src", GetNameString(operands[0]), immediate_data);
        break;
    }
    case IROpcode::VSlideUpi: {
        ret += fmt::format("{} <- {}({}: {}, 0x{:x})", GetNameString(name), "vslideupi", "src", GetNameString(operands[0]), immediate_data);
        break;
    }
    case IROpcode::VSlideUpZeroesi: {
        ret += fmt::format("{} <- {}({}: {}, 0x{:x})", GetNameString(name), "vslideupzeroesi", "src", GetNameString(operands[0]), immediate_data);
        break;
    }
    case IROpcode::VSlideDownZeroesi: {
        ret += fmt::format("{} <- {}({}: {}, 0x{:x})", GetNameString(name), "vslidedownzeroesi", "src", GetNameString(operands[0]), immediate_data);
        break;
    }
    case IROpcode::VSlide1Up: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vslide1up", "integer", GetNameString(operands[0]), "vector",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VSlide1Down: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vslide1down", "integer", GetNameString(operands[0]), "vector",
                           GetNameString(operands[1]));
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
    case IROpcode::VLessThanSigned: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vltsigned", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VLessThanUnsigned: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vltunsigned", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VGreaterThanSigned: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vgtsigned", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VGreaterThanUnsigned: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vgtunsigned", "src1", GetNameString(operands[0]), "src2",
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
    case IROpcode::VFAdd: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vfadd", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VFSub: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vfsub", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VFMul: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vfmul", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VFSqrt: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "vfsqrt", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::VFRcp: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "vfrcp", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::VFRcpSqrt: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "vfrcpsqrt", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::VFNotEqual: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vfneq", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VFLessThan: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vflt", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VCvtSToF: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "vcvtstof", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::VWCvtSToF: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "vwcvtstof", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::VNCvtSToF: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "vncvtstof", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::VCvtFToS: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "vcvtfstos", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::VCvtFToSRtz: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "vcvtfstos.rtz", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::VNCvtFToS: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "vncvtfstos", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::VNCvtFToSRtz: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "vncvtfstos.rtz", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::VWCvtFToS: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "vwcvtftos", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::VWCvtFToSRtz: {
        ret += fmt::format("{} <- {}({}: {})", GetNameString(name), "vwcvtftos.rtz", "src", GetNameString(operands[0]));
        break;
    }
    case IROpcode::VFDiv: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vfdiv", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VFMin: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vfmin", "src1", GetNameString(operands[0]), "src2",
                           GetNameString(operands[1]));
        break;
    }
    case IROpcode::VFMax: {
        ret += fmt::format("{} <- {}({}: {}, {}: {})", GetNameString(name), "vfmax", "src1", GetNameString(operands[0]), "src2",
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