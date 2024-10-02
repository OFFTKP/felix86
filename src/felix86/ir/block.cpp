#include <fmt/format.h>
#include "felix86/common/log.hpp"
#include "felix86/common/print.hpp"
#include "felix86/ir/block.hpp"

#define OP2(op) fmt::format("{} ← {} {} {}", inst.GetNameString(), inst.GetOperandNameString(0), #op, inst.GetOperandNameString(1))
#define SOP2(op) fmt::format("{} ← (i64){} {} (i64){}", inst.GetNameString(), inst.GetOperandNameString(0), #op, inst.GetOperandNameString(1))
#define U8OP2(op) fmt::format("{} ← (u8){} {} (u8){}", inst.GetNameString(), inst.GetOperandNameString(0), #op, inst.GetOperandNameString(1))
#define S8OP2(op) fmt::format("{} ← (i8){} {} (i8){}", inst.GetNameString(), inst.GetOperandNameString(0), #op, inst.GetOperandNameString(1))
#define S16OP2(op) fmt::format("{} ← (i16){} {} (i16){}", inst.GetNameString(), inst.GetOperandNameString(0), #op, inst.GetOperandNameString(1))
#define S32OP2(op) fmt::format("{} ← (i32){} {} (i32){}", inst.GetNameString(), inst.GetOperandNameString(0), #op, inst.GetOperandNameString(1))

#define FOP(func) fmt::format("{} ← {}()", inst.GetNameString(), #func)
#define FOP1(func, param) fmt::format("{} ← {}({}: {})", inst.GetNameString(), #func, #param, inst.GetOperandNameString(0))
#define FOP2(func, param1, param2)                                                                                                                   \
    fmt::format("{} ← {}({}: {}, {}: {})", inst.GetNameString(), #func, #param1, inst.GetOperandNameString(0), #param2, inst.GetOperandNameString(1))
#define FOP3(func, param1, param2, param3)                                                                                                           \
    fmt::format("{} ← {}({}: {}, {}: {}, {}: {})", inst.GetNameString(), #func, #param1, inst.GetOperandNameString(0), #param2,                      \
                inst.GetOperandNameString(1), #param3, inst.GetOperandNameString(2))
#define FOP7(func, param1, param2, param3, param4, param5, param6, param7)                                                                           \
    fmt::format("{} ← {}({}: {}, {}: {}, {}: {}, {}: {}, {}: {}, {}: {}, {}: {})", inst.GetNameString(), #func, #param1,                             \
                inst.GetOperandNameString(0), #param2, inst.GetOperandNameString(1), #param3, inst.GetOperandNameString(2), #param4,                 \
                inst.GetOperandNameString(3), #param5, inst.GetOperandNameString(4), #param6, inst.GetOperandNameString(5), #param7,                 \
                inst.GetOperandNameString(6))

std::string IRBlock::printInstruction(const IRInstruction& inst) const {
    IROpcode opcode = inst.GetOpcode();
    switch (opcode) {
    case IROpcode::Null: {
        return "Null";
    }
    case IROpcode::Phi: {
        const Phi& phi = inst.AsPhi();
        std::string ret = fmt::format("{} ← φ<%{}>(", inst.GetNameString(), print_guest_register(phi.ref));
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
        return inst.AsComment().comment;
    }
    case IROpcode::TupleExtract: {
        const TupleAccess& tup = inst.AsTupleAccess();
        return fmt::format("{} ← get<{}>({})", inst.GetNameString(), tup.index, tup.tuple->GetNameString());
    }
    case IROpcode::Select: {
        return fmt::format("{} ← {} ? {} : {}", inst.GetNameString(), inst.GetOperandNameString(0), inst.GetOperandNameString(1),
                           inst.GetOperandNameString(2));
    }
    case IROpcode::Lea: {
        return fmt::format("{} ← [{} + {} * {} + 0x{:x}]", inst.GetNameString(), inst.GetOperandNameString(0), inst.GetOperandNameString(1),
                           inst.GetOperand(2)->AsImmediate().immediate, inst.GetOperand(3)->AsImmediate().immediate);
    }
    case IROpcode::Mov: {
        return fmt::format("{} ← {}", inst.GetNameString(), inst.GetOperandNameString(0));
    }
    case IROpcode::Immediate: {
        return fmt::format("{} ← {:x}", inst.GetNameString(), inst.AsImmediate().immediate);
    }
    case IROpcode::Rdtsc: {
        return FOP(rdtsc);
    }
    case IROpcode::GetGuest: {
        return fmt::format("{} ← get_guest %{}", inst.GetNameString(), print_guest_register(inst.AsGetGuest().ref));
    }
    case IROpcode::SetGuest: {
        return fmt::format("{} ← set_guest %{}, {}", inst.GetNameString(), print_guest_register(inst.AsSetGuest().ref),
                           inst.AsSetGuest().source->GetNameString());
    }
    case IROpcode::LoadGuestFromMemory: {
        return fmt::format("{} ← load_from_vm %{}", inst.GetNameString(), print_guest_register(inst.AsGetGuest().ref));
    }
    case IROpcode::StoreGuestToMemory: {
        return fmt::format("{} ← store_to_vm %{}, {}", inst.GetNameString(), print_guest_register(inst.AsSetGuest().ref),
                           inst.AsSetGuest().source->GetNameString());
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
        return fmt::format("{} ← vpshufdword({}, {:x})", inst.GetNameString(), inst.GetOperandNameString(0), (u8)inst.GetExtraData());
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
        ERROR("Unimplemented op: %d", (int)inst.GetOpcode());
        return "";
    }
    }
}

std::string IRBlock::Print() const {
    std::string ret;

    ret += fmt::format("Block {}", GetIndex());
    if (GetStartAddress() != IR_NO_ADDRESS) {
        ret += fmt::format("  @ 0x{:016x}", GetStartAddress());
    }
    ret += "\n";

    for (const IRInstruction& inst : instructions) {
        ret += "    ";
        std::string inst_string = printInstruction(inst);
        size_t size = inst_string.size();
        while (size < 50) {
            inst_string += " ";
        }
        ret += inst_string;
        ret += "(uses: " + std::to_string(inst.GetUseCount()) + ")\n";
    }

    return ret;
}