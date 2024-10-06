#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "felix86/common/log.hpp"
#include "felix86/frontend/instruction.hpp"
#include "felix86/ir/emitter.hpp"
#include "felix86/ir/instruction.hpp"

void ir_store_partial_state(IRBlock* block, std::span<const x86_ref_e> refs) {
    for (x86_ref_e reg : refs) {
        IRInstruction* guest = ir_emit_get_guest(block, reg);
        ir_emit_store_guest_to_memory(block, reg, guest);
    }
}

void ir_load_partial_state(IRBlock* block, std::span<const x86_ref_e> refs) {
    for (x86_ref_e reg : refs) {
        IRInstruction* guest = ir_emit_load_guest_from_memory(block, reg);
        ir_emit_set_guest(block, reg, guest);
    }
}

u16 get_bit_size(x86_size_e size) {
    switch (size) {
    case X86_SIZE_BYTE:
        return 8;
    case X86_SIZE_WORD:
        return 16;
    case X86_SIZE_DWORD:
        return 32;
    case X86_SIZE_QWORD:
        return 64;
    case X86_SIZE_MM:
        return 64;
    case X86_SIZE_XMM:
        return 128;
    case X86_SIZE_YMM:
        return 256;
    case X86_SIZE_ZMM:
        return 512;
    }

    ERROR("Invalid register size");
}

x86_operand_t get_full_reg(x86_ref_e ref) {
    x86_operand_t operand;
    operand.type = X86_OP_TYPE_REGISTER;
    operand.reg.ref = ref;
    operand.size = X86_SIZE_QWORD;
    return operand;
}

IRInstruction* get_reg(IRBlock* block, x86_ref_e ref, x86_size_e size_e) {
    x86_operand_t operand = get_full_reg(ref);
    operand.size = size_e;
    return ir_emit_get_reg(block, &operand);
}

IRInstruction* ir_emit_get_mask(IRBlock* block, x86_size_e size_e) {
    u16 size = get_bit_size(size_e);
    switch (size) {
    case 8:
        return ir_emit_immediate(block, 0xFF);
    case 16:
        return ir_emit_immediate(block, 0xFFFF);
    case 32:
        return ir_emit_immediate(block, 0xFFFFFFFF);
    case 64:
        return ir_emit_immediate(block, 0xFFFFFFFFFFFFFFFF);
    default:
        ERROR("Invalid size");
    }
}

IRInstruction* ir_emit_one_operand(IRBlock* block, IROpcode opcode, IRInstruction* source) {
    IRInstruction instruction(opcode, {source});
    return block->InsertAtEnd(std::move(instruction));
}

IRInstruction* ir_emit_two_operands(IRBlock* block, IROpcode opcode, IRInstruction* source1, IRInstruction* source2) {
    IRInstruction instruction(opcode, {source1, source2});
    return block->InsertAtEnd(std::move(instruction));
}

IRInstruction* ir_emit_three_operands(IRBlock* block, IROpcode opcode, IRInstruction* source1, IRInstruction* source2, IRInstruction* source3) {
    IRInstruction instruction(opcode, {source1, source2, source3});
    return block->InsertAtEnd(std::move(instruction));
}

IRInstruction* ir_emit_four_operands(IRBlock* block, IROpcode opcode, IRInstruction* source1, IRInstruction* source2, IRInstruction* source3,
                                     IRInstruction* source4) {
    IRInstruction instruction(opcode, {source1, source2, source3, source4});
    return block->InsertAtEnd(std::move(instruction));
}

void ir_emit_runtime_comment(IRBlock* block, const std::string& comment) {
    IRInstruction instruction(comment);
    block->InsertAtEnd(std::move(instruction));
}

IRInstruction* ir_emit_add(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Add, source1, source2);
}

IRInstruction* ir_emit_sub(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Sub, source1, source2);
}

IRInstruction* ir_emit_shift_left(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::ShiftLeft, source1, source2);
}

IRInstruction* ir_emit_shift_right(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::ShiftRight, source1, source2);
}

IRInstruction* ir_emit_shift_right_arithmetic(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::ShiftRightArithmetic, source1, source2);
}

IRInstruction* ir_emit_rotate(IRBlock* block, IRInstruction* source1, IRInstruction* source2, x86_size_e size_e, bool right) {
    u8 size = get_bit_size(size_e);
    IROpcode type;
    IRInstruction* count = source2;
    switch (size) {
    case 8:
        type = IROpcode::LeftRotate8;
        if (right) {
            count = ir_emit_sub(block, ir_emit_immediate(block, 8), count);
        }
        break;
    case 16:
        type = IROpcode::LeftRotate16;
        if (right) {
            count = ir_emit_sub(block, ir_emit_immediate(block, 16), count);
        }
        break;
    case 32:
        type = IROpcode::LeftRotate32;
        if (right) {
            count = ir_emit_sub(block, ir_emit_immediate(block, 32), count);
        }
        break;
    case 64:
        type = IROpcode::LeftRotate64;
        if (right) {
            count = ir_emit_sub(block, ir_emit_immediate(block, 64), count);
        }
        break;
    }
    return ir_emit_two_operands(block, type, source1, count);
}

IRInstruction* ir_emit_select(IRBlock* block, IRInstruction* condition, IRInstruction* true_value, IRInstruction* false_value) {
    return ir_emit_three_operands(block, IROpcode::Select, condition, true_value, false_value);
}

IRInstruction* ir_emit_clz(IRBlock* block, IRInstruction* source) {
    return ir_emit_one_operand(block, IROpcode::Clz, source);
}

IRInstruction* ir_emit_ctz(IRBlock* block, IRInstruction* source) {
    return ir_emit_one_operand(block, IROpcode::Ctz, source);
}

IRInstruction* ir_emit_and(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::And, source1, source2);
}

IRInstruction* ir_emit_or(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Or, source1, source2);
}

IRInstruction* ir_emit_xor(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Xor, source1, source2);
}

IRInstruction* ir_emit_not(IRBlock* block, IRInstruction* source) {
    return ir_emit_one_operand(block, IROpcode::Not, source);
}

IRInstruction* ir_emit_equal(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Equal, source1, source2);
}

IRInstruction* ir_emit_not_equal(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::NotEqual, source1, source2);
}

IRInstruction* ir_emit_greater_than_signed(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::IGreaterThan, source1, source2);
}

IRInstruction* ir_emit_less_than_signed(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::ILessThan, source1, source2);
}

IRInstruction* ir_emit_greater_than_unsigned(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::UGreaterThan, source1, source2);
}

IRInstruction* ir_emit_less_than_unsigned(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::ULessThan, source1, source2);
}

IRInstruction* ir_emit_div(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Div, source1, source2);
}

IRInstruction* ir_emit_divu(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Divu, source1, source2);
}

IRInstruction* ir_emit_rem(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Rem, source1, source2);
}

IRInstruction* ir_emit_remu(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Remu, source1, source2);
}

IRInstruction* ir_emit_divw(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Divw, source1, source2);
}

IRInstruction* ir_emit_divuw(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Divuw, source1, source2);
}

IRInstruction* ir_emit_remw(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Remw, source1, source2);
}

IRInstruction* ir_emit_remuw(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Remuw, source1, source2);
}

IRInstruction* ir_emit_mul(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Mul, source1, source2);
}

IRInstruction* ir_emit_mulh(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Mulh, source1, source2);
}

IRInstruction* ir_emit_mulhu(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Mulhu, source1, source2);
}

IRInstruction* ir_emit_div128(IRBlock* block, IRInstruction* divisor) {
    return ir_emit_one_operand(block, IROpcode::Div128, divisor);
}

IRInstruction* ir_emit_divu128(IRBlock* block, IRInstruction* divisor) {
    return ir_emit_one_operand(block, IROpcode::Divu128, divisor);
}

IRInstruction* ir_emit_lea(IRBlock* block, x86_operand_t* rm_operand) {
    IRInstruction* (*get_guest)(IRBlock* block, x86_ref_e reg) = rm_operand->memory.address_override ? ir_emit_get_gpr32 : ir_emit_get_gpr64;

    IRInstruction* base = rm_operand->memory.base != X86_REF_COUNT ? get_guest(block, rm_operand->memory.base) : ir_emit_immediate(block, 0);

    IRInstruction* base_final = base;
    if (rm_operand->memory.fs_override) {
        IRInstruction* fs = ir_emit_get_guest(block, X86_REF_FS);
        base_final = ir_emit_add(block, base, fs);
    } else if (rm_operand->memory.gs_override) {
        IRInstruction* gs = ir_emit_get_guest(block, X86_REF_GS);
        base_final = ir_emit_add(block, base, gs);
    }

    IRInstruction* index = rm_operand->memory.index != X86_REF_COUNT ? get_guest(block, rm_operand->memory.index) : ir_emit_immediate(block, 0);
    IRInstruction* displacement = ir_emit_immediate(block, rm_operand->memory.displacement);
    IRInstruction* scale = ir_emit_immediate(block, rm_operand->memory.scale);
    IRInstruction* scaled_index = ir_emit_shift_left(block, index, scale);
    IRInstruction* address = ir_emit_add(block, base_final, scaled_index);
    IRInstruction* displaced_address = ir_emit_add(block, address, displacement);

    IRInstruction* final_address = displaced_address;
    if (rm_operand->memory.address_override) {
        IRInstruction* mask = ir_emit_immediate(block, 0xFFFFFFFF);
        final_address = ir_emit_and(block, address, mask);
    }

    return final_address;
}

IRInstruction* ir_emit_popcount(IRBlock* block, IRInstruction* source) {
    return ir_emit_one_operand(block, IROpcode::Popcount, source);
}

IRInstruction* ir_emit_sext8(IRBlock* block, IRInstruction* source) {
    return ir_emit_one_operand(block, IROpcode::Sext8, source);
}

IRInstruction* ir_emit_sext16(IRBlock* block, IRInstruction* source) {
    return ir_emit_one_operand(block, IROpcode::Sext16, source);
}

IRInstruction* ir_emit_sext32(IRBlock* block, IRInstruction* source) {
    return ir_emit_one_operand(block, IROpcode::Sext32, source);
}

IRInstruction* ir_emit_sext(IRBlock* block, IRInstruction* source, x86_size_e size) {
    switch (size) {
    case X86_SIZE_BYTE:
        return ir_emit_sext8(block, source);
    case X86_SIZE_WORD:
        return ir_emit_sext16(block, source);
    case X86_SIZE_DWORD:
        return ir_emit_sext32(block, source);
    case X86_SIZE_QWORD:
        return source;
    default:
        ERROR("Invalid size");
    }
}

void ir_emit_syscall(IRBlock* block) {
    // The kernel clobbers registers rcx and r11 but preserves all other registers except rax which is the result.
    // We don't have to clobber rcx and r11 as we are not a kernel.
    // First, get_guest and writeback rax, rdi, rsi, rdx, r10, r8, r9 because they may be used by the syscall.
    // Then emit the syscall instruction.
    // Finally, load rax so it's not propagated from before the syscall.
    constexpr static std::array in_regs = {X86_REF_RAX, X86_REF_RDI, X86_REF_RSI, X86_REF_RDX, X86_REF_R10, X86_REF_R8, X86_REF_R9};
    constexpr static std::array out_regs = {X86_REF_RAX};

    ir_store_partial_state(block, in_regs);

    IRInstruction instruction(IROpcode::Syscall, {});
    instruction.Lock();
    block->InsertAtEnd(std::move(instruction));

    ir_load_partial_state(block, out_regs);
}

void ir_terminate_jump(IRBlock* block, IRBlock* target) {
    block->TerminateJump(target);
}

void ir_terminate_jump_conditional(IRBlock* block, IRInstruction* condition, IRBlock* target_true, IRBlock* target_false) {
    block->TerminateJumpConditional(condition, target_true, target_false);
}

IRInstruction* ir_emit_insert_integer_to_vector(IRBlock* block, IRInstruction* source, IRInstruction* dest, u8 idx, x86_size_e sz) {
    u64 extra_data = (u64)sz << 8 | idx;
    IRInstruction* instruction = ir_emit_two_operands(block, IROpcode::VInsertInteger, source, dest);
    instruction->SetExtraData(extra_data);
    return instruction;
}

IRInstruction* ir_emit_extract_integer_from_vector(IRBlock* block, IRInstruction* src, u8 idx, x86_size_e sz) {
    u64 extra_data = (u64)sz << 8 | idx;
    IRInstruction* instruction = ir_emit_one_operand(block, IROpcode::VExtractInteger, src);
    instruction->SetExtraData(extra_data);
    return instruction;
}

IRInstruction* ir_emit_vector_unpack_byte_low(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::VUnpackByteLow, source1, source2);
}

IRInstruction* ir_emit_vector_unpack_word_low(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::VUnpackWordLow, source1, source2);
}

IRInstruction* ir_emit_vector_unpack_dword_low(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::VUnpackDWordLow, source1, source2);
}

IRInstruction* ir_emit_vector_unpack_qword_low(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::VUnpackQWordLow, source1, source2);
}

IRInstruction* ir_emit_vector_from_integer(IRBlock* block, IRInstruction* source) {
    return ir_emit_one_operand(block, IROpcode::CastIntegerToVector, source);
}

IRInstruction* ir_emit_integer_from_vector(IRBlock* block, IRInstruction* source) {
    return ir_emit_one_operand(block, IROpcode::CastVectorToInteger, source);
}

IRInstruction* ir_emit_vector_packed_and(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::VAnd, source1, source2);
}

IRInstruction* ir_emit_vector_packed_or(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::VOr, source1, source2);
}

IRInstruction* ir_emit_vector_packed_xor(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::VXor, source1, source2);
}

IRInstruction* ir_emit_vector_packed_shift_right(IRBlock* block, IRInstruction* source, IRInstruction* imm) {
    return ir_emit_two_operands(block, IROpcode::VShr, source, imm);
}

IRInstruction* ir_emit_vector_packed_shift_left(IRBlock* block, IRInstruction* source, IRInstruction* imm) {
    return ir_emit_two_operands(block, IROpcode::VShl, source, imm);
}

IRInstruction* ir_emit_vector_packed_sub_byte(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::VPackedSubByte, source1, source2);
}

IRInstruction* ir_emit_vector_packed_add_qword(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::VPackedAddQWord, source1, source2);
}

IRInstruction* ir_emit_vector_packed_compare_eq_byte(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::VPackedEqualByte, source1, source2);
}

IRInstruction* ir_emit_vector_packed_compare_eq_word(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::VPackedEqualWord, source1, source2);
}

IRInstruction* ir_emit_vector_packed_compare_eq_dword(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::VPackedEqualDWord, source1, source2);
}

IRInstruction* ir_emit_vector_packed_shuffle_dword(IRBlock* block, IRInstruction* source, u8 control_byte) {
    IRInstruction instruction(IROpcode::VPackedShuffleDWord, {source});
    instruction.SetExtraData(control_byte);
    return block->InsertAtEnd(std::move(instruction));
}

IRInstruction* ir_emit_vector_packed_move_byte_mask(IRBlock* block, IRInstruction* source) {
    return ir_emit_one_operand(block, IROpcode::VMoveByteMask, source);
}

IRInstruction* ir_emit_vector_packed_min_byte(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::VPackedMinByte, source1, source2);
}

IRInstruction* ir_emit_load_guest_from_memory(IRBlock* block, x86_ref_e ref) {
    if (ref == X86_REF_COUNT) {
        ERROR("Invalid register reference");
    }

    IRInstruction instruction(IROpcode::LoadGuestFromMemory, ref);
    return block->InsertAtEnd(std::move(instruction));
}

void ir_emit_store_guest_to_memory(IRBlock* block, x86_ref_e ref, IRInstruction* source) {
    if (ref == X86_REF_COUNT) {
        ERROR("Invalid register reference");
    }

    IRInstruction instruction(IROpcode::StoreGuestToMemory, ref, source);
    instruction.Lock();
    block->InsertAtEnd(std::move(instruction));
}

IRInstruction* ir_emit_get_guest(IRBlock* block, x86_ref_e ref) {
    if (ref == X86_REF_COUNT) {
        ERROR("Invalid register reference");
    }

    IRInstruction instruction(IROpcode::GetGuest, ref);
    return block->InsertAtEnd(std::move(instruction));
}

void ir_emit_set_guest(IRBlock* block, x86_ref_e ref, IRInstruction* source) {
    if (ref == X86_REF_COUNT) {
        ERROR("Invalid register reference");
    }

    IRInstruction instruction(IROpcode::SetGuest, ref, source);
    block->InsertAtEnd(std::move(instruction));
}

IRInstruction* ir_emit_get_flag(IRBlock* block, x86_ref_e flag) {
    if (flag < X86_REF_CF || flag > X86_REF_OF) {
        ERROR("Invalid flag reference");
    }

    return ir_emit_get_guest(block, flag);
}

IRInstruction* ir_emit_get_flag_not(IRBlock* block, x86_ref_e flag) {
    IRInstruction* instruction = ir_emit_get_flag(block, flag);
    IRInstruction* one = ir_emit_immediate(block, 1);
    return ir_emit_xor(block, instruction, one);
}

void ir_emit_set_flag(IRBlock* block, x86_ref_e flag, IRInstruction* source) {
    if (flag < X86_REF_CF || flag > X86_REF_OF) {
        ERROR("Invalid flag reference");
    }

    ir_emit_set_guest(block, flag, source);
}

IRInstruction* ir_emit_read_byte(IRBlock* block, IRInstruction* address) {
    return ir_emit_one_operand(block, IROpcode::ReadByte, address);
}

IRInstruction* ir_emit_read_word(IRBlock* block, IRInstruction* address) {
    return ir_emit_one_operand(block, IROpcode::ReadWord, address);
}

IRInstruction* ir_emit_read_dword(IRBlock* block, IRInstruction* address) {
    return ir_emit_one_operand(block, IROpcode::ReadDWord, address);
}

IRInstruction* ir_emit_read_qword(IRBlock* block, IRInstruction* address) {
    return ir_emit_one_operand(block, IROpcode::ReadQWord, address);
}

IRInstruction* ir_emit_read_xmmword(IRBlock* block, IRInstruction* address) {
    return ir_emit_one_operand(block, IROpcode::ReadXmmWord, address);
}

void ir_emit_write_byte(IRBlock* block, IRInstruction* address, IRInstruction* source) {
    IRInstruction* instruction = ir_emit_two_operands(block, IROpcode::WriteByte, address, source);
    instruction->Lock();
}

void ir_emit_write_word(IRBlock* block, IRInstruction* address, IRInstruction* source) {
    IRInstruction* instruction = ir_emit_two_operands(block, IROpcode::WriteWord, address, source);
    instruction->Lock();
}

void ir_emit_write_dword(IRBlock* block, IRInstruction* address, IRInstruction* source) {
    IRInstruction* instruction = ir_emit_two_operands(block, IROpcode::WriteDWord, address, source);
    instruction->Lock();
}

void ir_emit_write_qword(IRBlock* block, IRInstruction* address, IRInstruction* source) {
    IRInstruction* instruction = ir_emit_two_operands(block, IROpcode::WriteQWord, address, source);
    instruction->Lock();
}

void ir_emit_write_xmmword(IRBlock* block, IRInstruction* address, IRInstruction* source) {
    IRInstruction* instruction = ir_emit_two_operands(block, IROpcode::WriteXmmWord, address, source);
    instruction->Lock();
}

void ir_emit_cpuid(IRBlock* block) {
    // Similar to syscall, cpuid clobbers registers rax, rcx, rdx, rbx but preserves all other registers.
    // It uses rax and rcx as input.
    constexpr static std::array in_regs = {X86_REF_RAX, X86_REF_RCX};
    constexpr static std::array out_regs = {X86_REF_RAX, X86_REF_RBX, X86_REF_RCX, X86_REF_RDX};

    ir_store_partial_state(block, in_regs);

    IRInstruction instruction(IROpcode::Cpuid, {});
    block->InsertAtEnd(std::move(instruction));

    ir_load_partial_state(block, out_regs);
}

void ir_emit_rdtsc(IRBlock* block) {
    // Has no inputs but writes to EDX:EAX
    constexpr static std::array out_regs = {X86_REF_RAX, X86_REF_RDX};

    IRInstruction instruction(IROpcode::Rdtsc, {});
    block->InsertAtEnd(std::move(instruction));

    ir_load_partial_state(block, out_regs);
}

IRInstruction* ir_emit_immediate(IRBlock* block, u64 value) {
    IRInstruction instruction(value);
    return block->InsertAtEnd(std::move(instruction));
}

IRInstruction* ir_emit_immediate_sext(IRBlock* block, x86_operand_t* operand) {
    i64 value = operand->immediate.data;
    switch (operand->size) {
    case X86_SIZE_BYTE:
        value = (i8)value;
        break;
    case X86_SIZE_WORD:
        value = (i16)value;
        break;
    case X86_SIZE_DWORD:
        value = (i32)value;
        break;
    case X86_SIZE_QWORD:
        break;
    default:
        ERROR("Invalid immediate size");
    }

    return ir_emit_immediate(block, value);
}

// ██   ██ ███████ ██      ██████  ███████ ██████  ███████
// ██   ██ ██      ██      ██   ██ ██      ██   ██ ██
// ███████ █████   ██      ██████  █████   ██████  ███████
// ██   ██ ██      ██      ██      ██      ██   ██      ██
// ██   ██ ███████ ███████ ██      ███████ ██   ██ ███████

IRInstruction* ir_emit_get_reg(IRBlock* block, x86_operand_t* reg_operand) {
    if (reg_operand->type != X86_OP_TYPE_REGISTER) {
        ERROR("Invalid operand type");
    }

    switch (reg_operand->size) {
    case X86_SIZE_BYTE: {
        if (reg_operand->reg.high8) {
            return ir_emit_get_gpr8_high(block, reg_operand->reg.ref);
        } else {
            return ir_emit_get_gpr8_low(block, reg_operand->reg.ref);
        }
    }
    case X86_SIZE_WORD:
        return ir_emit_get_gpr16(block, reg_operand->reg.ref);
    case X86_SIZE_DWORD:
        return ir_emit_get_gpr32(block, reg_operand->reg.ref);
    case X86_SIZE_QWORD:
        return ir_emit_get_gpr64(block, reg_operand->reg.ref);
    case X86_SIZE_XMM:
    case X86_SIZE_YMM:
    case X86_SIZE_ZMM:
        return ir_emit_get_vector(block, reg_operand->reg.ref);
    default:
        ERROR("Invalid register size");
        return NULL;
    }
}

IRInstruction* ir_emit_get_rm(IRBlock* block, x86_operand_t* rm_operand) {
    if (rm_operand->type == X86_OP_TYPE_REGISTER) {
        return ir_emit_get_reg(block, rm_operand);
    } else {
        IRInstruction* address = ir_emit_lea(block, rm_operand);
        return ir_emit_read_memory(block, address, rm_operand->size);
    }
}

void ir_emit_set_reg(IRBlock* block, x86_operand_t* reg_operand, IRInstruction* source) {
    switch (reg_operand->size) {
    case X86_SIZE_BYTE: {
        if (reg_operand->reg.high8) {
            return ir_emit_set_gpr8_high(block, reg_operand->reg.ref, source);
        } else {
            return ir_emit_set_gpr8_low(block, reg_operand->reg.ref, source);
        }
    }
    case X86_SIZE_WORD:
        return ir_emit_set_gpr16(block, reg_operand->reg.ref, source);
    case X86_SIZE_DWORD:
        return ir_emit_set_gpr32(block, reg_operand->reg.ref, source);
    case X86_SIZE_QWORD:
        return ir_emit_set_gpr64(block, reg_operand->reg.ref, source);
    case X86_SIZE_XMM:
    case X86_SIZE_YMM:
    case X86_SIZE_ZMM:
        return ir_emit_set_vector(block, reg_operand->reg.ref, source);
    default:
        ERROR("Invalid register size");
        return;
    }
}

void ir_emit_set_reg(IRBlock* block, x86_ref_e ref, x86_size_e size, IRInstruction* source, bool high = false) {
    x86_operand_t operand;
    operand.type = X86_OP_TYPE_REGISTER;
    operand.reg.ref = ref;
    operand.reg.high8 = high;
    operand.size = size;
    ir_emit_set_reg(block, &operand, source);
}

void ir_emit_set_rm(IRBlock* block, x86_operand_t* rm_operand, IRInstruction* source) {
    if (rm_operand->type == X86_OP_TYPE_REGISTER) {
        return ir_emit_set_reg(block, rm_operand, source);
    } else {
        IRInstruction* address = ir_emit_lea(block, rm_operand);
        return ir_emit_write_memory(block, address, source, rm_operand->size);
    }
}

void ir_emit_write_memory(IRBlock* block, IRInstruction* address, IRInstruction* value, x86_size_e size) {
    switch (size) {
    case X86_SIZE_BYTE:
        return ir_emit_write_byte(block, address, value);
    case X86_SIZE_WORD:
        return ir_emit_write_word(block, address, value);
    case X86_SIZE_DWORD:
        return ir_emit_write_dword(block, address, value);
    case X86_SIZE_QWORD:
        return ir_emit_write_qword(block, address, value);
    case X86_SIZE_XMM:
        return ir_emit_write_xmmword(block, address, value);
    default:
        ERROR("Invalid memory size");
        return;
    }
}

IRInstruction* ir_emit_read_memory(IRBlock* block, IRInstruction* address, x86_size_e size) {
    switch (size) {
    case X86_SIZE_BYTE:
        return ir_emit_read_byte(block, address);
    case X86_SIZE_WORD:
        return ir_emit_read_word(block, address);
    case X86_SIZE_DWORD:
        return ir_emit_read_dword(block, address);
    case X86_SIZE_QWORD:
        return ir_emit_read_qword(block, address);
    case X86_SIZE_XMM:
        return ir_emit_read_xmmword(block, address);
    default:
        ERROR("Invalid memory size");
        return NULL;
    }
}

IRInstruction* ir_emit_get_gpr8_low(IRBlock* block, x86_ref_e reg) {
    if (reg < X86_REF_RAX || reg > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    IRInstruction* full_reg = ir_emit_get_guest(block, reg);
    IRInstruction* mask = ir_emit_immediate(block, 0xFF);
    IRInstruction* instruction = ir_emit_and(block, full_reg, mask);

    return instruction;
}

IRInstruction* ir_emit_get_gpr8_high(IRBlock* block, x86_ref_e reg) {
    if (reg < X86_REF_RAX || reg > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    IRInstruction* full_reg = ir_emit_get_guest(block, reg);
    IRInstruction* shift = ir_emit_immediate(block, 8);
    IRInstruction* shifted = ir_emit_shift_right(block, full_reg, shift);
    IRInstruction* mask = ir_emit_immediate(block, 0xFF);
    IRInstruction* instruction = ir_emit_and(block, shifted, mask);

    return instruction;
}

IRInstruction* ir_emit_get_gpr16(IRBlock* block, x86_ref_e reg) {
    if (reg < X86_REF_RAX || reg > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    IRInstruction* full_reg = ir_emit_get_guest(block, reg);
    IRInstruction* mask = ir_emit_immediate(block, 0xFFFF);
    IRInstruction* instruction = ir_emit_and(block, full_reg, mask);

    return instruction;
}

IRInstruction* ir_emit_get_gpr32(IRBlock* block, x86_ref_e reg) {
    if (reg < X86_REF_RAX || reg > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    IRInstruction* full_reg = ir_emit_get_guest(block, reg);
    IRInstruction* mask = ir_emit_immediate(block, 0xFFFFFFFF);
    IRInstruction* instruction = ir_emit_and(block, full_reg, mask);

    return instruction;
}

IRInstruction* ir_emit_get_gpr64(IRBlock* block, x86_ref_e reg) {
    if (reg < X86_REF_RAX || reg > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    IRInstruction* instruction = ir_emit_get_guest(block, reg);
    return instruction;
}

IRInstruction* ir_emit_get_vector(IRBlock* block, x86_ref_e reg) {
    if (reg < X86_REF_XMM0 || reg > X86_REF_XMM15) {
        ERROR("Invalid register reference");
    }

    IRInstruction* instruction = ir_emit_get_guest(block, reg);
    return instruction;
}

void ir_emit_set_gpr8_low(IRBlock* block, x86_ref_e reg, IRInstruction* source) {
    IRInstruction* full_reg = ir_emit_get_guest(block, reg);
    IRInstruction* mask = ir_emit_immediate(block, 0xFFFFFFFFFFFFFF00);
    IRInstruction* masked = ir_emit_and(block, full_reg, mask);
    IRInstruction* value_mask = ir_emit_immediate(block, 0xFF);
    IRInstruction* value = ir_emit_and(block, source, value_mask);
    IRInstruction* final_value = ir_emit_or(block, masked, value);
    ir_emit_set_guest(block, reg, final_value);
}

void ir_emit_set_gpr8_high(IRBlock* block, x86_ref_e reg, IRInstruction* source) {
    IRInstruction* full_reg = ir_emit_get_guest(block, reg);
    IRInstruction* mask = ir_emit_immediate(block, 0xFFFFFFFFFFFF00FF);
    IRInstruction* masked = ir_emit_and(block, full_reg, mask);
    IRInstruction* value_mask = ir_emit_immediate(block, 0xFF);
    IRInstruction* value = ir_emit_and(block, source, value_mask);
    IRInstruction* shift = ir_emit_immediate(block, 8);
    IRInstruction* shifted = ir_emit_shift_left(block, value, shift);
    IRInstruction* final_value = ir_emit_or(block, masked, shifted);
    ir_emit_set_guest(block, reg, final_value);
}

void ir_emit_set_gpr16(IRBlock* block, x86_ref_e reg, IRInstruction* source) {
    IRInstruction* full_reg = ir_emit_get_guest(block, reg);
    IRInstruction* mask = ir_emit_immediate(block, 0xFFFFFFFFFFFF0000);
    IRInstruction* masked = ir_emit_and(block, full_reg, mask);
    IRInstruction* value_mask = ir_emit_immediate(block, 0xFFFF);
    IRInstruction* value = ir_emit_and(block, source, value_mask);
    IRInstruction* final_value = ir_emit_or(block, masked, value);
    ir_emit_set_guest(block, reg, final_value);
}

void ir_emit_set_gpr32(IRBlock* block, x86_ref_e reg, IRInstruction* source) {
    IRInstruction* value_mask = ir_emit_immediate(block, 0xFFFFFFFF);
    IRInstruction* final_value = ir_emit_and(block, source, value_mask);
    ir_emit_set_guest(block, reg, final_value);
}

void ir_emit_set_gpr64(IRBlock* block, x86_ref_e reg, IRInstruction* source) {
    ir_emit_set_guest(block, reg, source);
}

void ir_emit_set_vector(IRBlock* block, x86_ref_e reg, IRInstruction* source) {
    ir_emit_set_guest(block, reg, source);
}

IRInstruction* ir_emit_get_parity(IRBlock* block, IRInstruction* source) {
    IRInstruction* mask = ir_emit_immediate(block, 0xFF);
    IRInstruction* masked = ir_emit_and(block, source, mask);
    IRInstruction* popcount = ir_emit_popcount(block, masked);
    IRInstruction* one = ir_emit_immediate(block, 1);
    IRInstruction* result = ir_emit_and(block, popcount, one);
    IRInstruction* instruction = ir_emit_xor(block, result, one);

    return instruction;
}

IRInstruction* ir_emit_get_zero(IRBlock* block, IRInstruction* source, x86_size_e size_e) {
    IRInstruction* zero = ir_emit_immediate(block, 0);
    IRInstruction* masked = ir_emit_and(block, source, ir_emit_get_mask(block, size_e));
    IRInstruction* instruction = ir_emit_equal(block, masked, zero);

    return instruction;
}

IRInstruction* ir_emit_get_size(IRBlock* block, x86_size_e size_e) {
    return ir_emit_immediate(block, get_bit_size(size_e));
}

IRInstruction* ir_emit_get_sign_mask(IRBlock* block, x86_size_e size_e) {
    u16 size = get_bit_size(size_e);
    return ir_emit_immediate(block, 1ull << (size - 1));
}

IRInstruction* ir_emit_get_shift_mask_left(IRBlock* block, IRInstruction* source, x86_size_e size_e) {
    IRInstruction* one = ir_emit_immediate(block, 1);
    IRInstruction* shiftMax = ir_emit_get_size(block, size_e);
    IRInstruction* shift = ir_emit_sub(block, shiftMax, source);
    IRInstruction* mask = ir_emit_shift_left(block, one, shift);
    return mask;
}

IRInstruction* ir_emit_get_shift_mask_right(IRBlock* block, IRInstruction* source) {
    IRInstruction* zero = ir_emit_immediate(block, 0);
    IRInstruction* is_zero = ir_emit_equal(block, source, zero);
    IRInstruction* one = ir_emit_immediate(block, 1);
    IRInstruction* shift = ir_emit_sub(block, source, one);
    IRInstruction* mask = ir_emit_shift_left(block, one, shift);
    return ir_emit_select(block, is_zero, zero, mask);
}

IRInstruction* ir_emit_get_sign(IRBlock* block, IRInstruction* source, x86_size_e size_e) {
    IRInstruction* mask = ir_emit_get_sign_mask(block, size_e);
    IRInstruction* masked = ir_emit_and(block, source, mask);
    IRInstruction* instruction = ir_emit_equal(block, masked, mask);

    return instruction;
}

IRInstruction* ir_emit_get_overflow_add(IRBlock* block, IRInstruction* source1, IRInstruction* source2, IRInstruction* result, x86_size_e size_e) {
    IRInstruction* mask = ir_emit_get_sign_mask(block, size_e);

    // for x + y = z, overflow occurs if ((z ^ x) & (z ^ y) & mask) == mask
    // which essentially checks if the sign bits of x and y are equal, but the
    // sign bit of z is different
    IRInstruction* xor1 = ir_emit_xor(block, result, source1);
    IRInstruction* xor2 = ir_emit_xor(block, result, source2);
    IRInstruction* masked1 = ir_emit_and(block, xor1, xor2);
    IRInstruction* masked2 = ir_emit_and(block, masked1, mask);

    return ir_emit_equal(block, masked2, mask);
}

IRInstruction* ir_emit_get_overflow_sub(IRBlock* block, IRInstruction* source1, IRInstruction* source2, IRInstruction* result, x86_size_e size_e) {
    IRInstruction* mask = ir_emit_get_sign_mask(block, size_e);

    // for x - y = z, overflow occurs if ((x ^ y) & (x ^ z) & mask) == mask
    IRInstruction* xor1 = ir_emit_xor(block, source1, source2);
    IRInstruction* xor2 = ir_emit_xor(block, source1, result);
    IRInstruction* masked1 = ir_emit_and(block, xor1, xor2);
    IRInstruction* masked2 = ir_emit_and(block, masked1, mask);

    return ir_emit_equal(block, masked2, mask);
}

IRInstruction* ir_emit_get_carry_add(IRBlock* block, IRInstruction* source1, IRInstruction* source2, IRInstruction* result, x86_size_e size_e) {
    (void)source2; // dont need, just keeping for consistency

    // CF = result < source1, as that means that the result overflowed
    IRInstruction* mask = ir_emit_get_mask(block, size_e);
    IRInstruction* masked_result = ir_emit_and(block, result, mask);
    return ir_emit_less_than_unsigned(block, masked_result, source1);
}

IRInstruction* ir_emit_get_carry_adc(IRBlock* block, IRInstruction* source1, IRInstruction* source2, x86_size_e size_e) {
    IRInstruction* carry_in = ir_emit_get_flag(block, X86_REF_CF);
    IRInstruction* sum = ir_emit_add(block, source1, source2);
    IRInstruction* sum_with_carry = ir_emit_add(block, sum, carry_in);

    IRInstruction* carry1 = ir_emit_get_carry_add(block, source1, source2, sum, size_e);
    IRInstruction* carry2 = ir_emit_get_carry_add(block, sum, carry_in, sum_with_carry, size_e);

    return ir_emit_or(block, carry1, carry2);
}

IRInstruction* ir_emit_get_carry_sub(IRBlock* block, IRInstruction* source1, IRInstruction* source2, IRInstruction* result, x86_size_e size_e) {
    (void)result; // dont need, just keeping for consistency

    // CF = source1 < source2, as that means that the result would underflow
    return ir_emit_less_than_unsigned(block, source1, source2);
}

IRInstruction* ir_emit_get_carry_sbb(IRBlock* block, IRInstruction* source1, IRInstruction* source2, x86_size_e size_e) {
    IRInstruction* carry_in = ir_emit_get_flag(block, X86_REF_CF);
    IRInstruction* sum = ir_emit_sub(block, source1, source2);
    IRInstruction* sum_with_carry = ir_emit_sub(block, sum, carry_in);

    IRInstruction* carry1 = ir_emit_get_carry_sub(block, source1, source2, sum, size_e);
    IRInstruction* carry2 = ir_emit_get_carry_sub(block, sum, carry_in, sum_with_carry, size_e);

    return ir_emit_or(block, carry1, carry2);
}

IRInstruction* ir_emit_get_aux_add(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    IRInstruction* mask = ir_emit_immediate(block, 0xF);
    IRInstruction* and1 = ir_emit_and(block, source1, mask);
    IRInstruction* and2 = ir_emit_and(block, source2, mask);
    IRInstruction* result = ir_emit_add(block, and1, and2);

    return ir_emit_greater_than_unsigned(block, result, mask);
}

IRInstruction* ir_emit_get_aux_sub(IRBlock* block, IRInstruction* source1, IRInstruction* source2) {
    IRInstruction* mask = ir_emit_immediate(block, 0xF);
    IRInstruction* and1 = ir_emit_and(block, source1, mask);
    IRInstruction* and2 = ir_emit_and(block, source2, mask);

    return ir_emit_less_than_unsigned(block, and1, and2);
}

IRInstruction* ir_emit_set_cpazso(IRBlock* block, IRInstruction* c, IRInstruction* p, IRInstruction* a, IRInstruction* z, IRInstruction* s,
                                  IRInstruction* o) {
    if (c)
        ir_emit_set_flag(block, X86_REF_CF, c);
    if (p)
        ir_emit_set_flag(block, X86_REF_PF, p);
    if (a)
        ir_emit_set_flag(block, X86_REF_AF, a);
    if (z)
        ir_emit_set_flag(block, X86_REF_ZF, z);
    if (s)
        ir_emit_set_flag(block, X86_REF_SF, s);
    if (o)
        ir_emit_set_flag(block, X86_REF_OF, o);

    return NULL;
}

void ir_emit_group1_imm(IRBlock* block, x86_instruction_t* inst) {
    x86_group1_e opcode = (x86_group1_e)(inst->operand_reg.reg.ref - X86_REF_RAX);

    x86_size_e size_e = inst->operand_rm.size;
    IRInstruction* rm = ir_emit_get_rm(block, &inst->operand_rm);
    IRInstruction* imm = ir_emit_immediate_sext(block, &inst->operand_imm);
    IRInstruction* result = NULL;
    IRInstruction* zero = ir_emit_immediate(block, 0);
    IRInstruction* c = zero;
    IRInstruction* o = zero;
    IRInstruction* a = NULL;

    switch (opcode) {
    case X86_GROUP1_ADD: {
        result = ir_emit_add(block, rm, imm);
        c = ir_emit_get_carry_add(block, rm, imm, result, inst->operand_rm.size);
        o = ir_emit_get_overflow_add(block, rm, imm, result, inst->operand_rm.size);
        a = ir_emit_get_aux_add(block, rm, imm);
        break;
    }
    case X86_GROUP1_ADC: {
        IRInstruction* carry_in = ir_emit_get_flag(block, X86_REF_CF);
        IRInstruction* imm_carry = ir_emit_add(block, imm, carry_in);
        result = ir_emit_add(block, rm, imm_carry);
        c = ir_emit_get_carry_adc(block, rm, imm_carry, inst->operand_rm.size);
        o = ir_emit_get_overflow_add(block, rm, imm_carry, result, inst->operand_rm.size);
        a = ir_emit_get_aux_add(block, rm, imm_carry);
        break;
    }
    case X86_GROUP1_SBB: {
        IRInstruction* carry_in = ir_emit_get_flag(block, X86_REF_CF);
        IRInstruction* imm_carry = ir_emit_add(block, imm, carry_in);
        result = ir_emit_sub(block, rm, imm_carry);
        c = ir_emit_get_carry_sbb(block, rm, imm_carry, inst->operand_rm.size);
        o = ir_emit_get_overflow_sub(block, rm, imm_carry, result, inst->operand_rm.size);
        a = ir_emit_get_aux_sub(block, rm, imm_carry);
        break;
    }
    case X86_GROUP1_OR: {
        result = ir_emit_or(block, rm, imm);
        break;
    }
    case X86_GROUP1_AND: {
        result = ir_emit_and(block, rm, imm);
        break;
    }
    case X86_GROUP1_SUB: {
        result = ir_emit_sub(block, rm, imm);
        c = ir_emit_get_carry_sub(block, rm, imm, result, inst->operand_rm.size);
        o = ir_emit_get_overflow_sub(block, rm, imm, result, inst->operand_rm.size);
        a = ir_emit_get_aux_sub(block, rm, imm);
        break;
    }
    case X86_GROUP1_XOR: {
        result = ir_emit_xor(block, rm, imm);
        break;
    }
    case X86_GROUP1_CMP: {
        result = ir_emit_sub(block, rm, imm);
        c = ir_emit_get_carry_sub(block, rm, imm, result, inst->operand_rm.size);
        o = ir_emit_get_overflow_sub(block, rm, imm, result, inst->operand_rm.size);
        a = ir_emit_get_aux_sub(block, rm, imm);
        break;
    }
    }

    IRInstruction* p = ir_emit_get_parity(block, result);
    IRInstruction* z = ir_emit_get_zero(block, result, size_e);
    IRInstruction* s = ir_emit_get_sign(block, result, inst->operand_rm.size);

    ir_emit_set_cpazso(block, c, p, a, z, s, o);

    if (opcode != X86_GROUP1_CMP) {
        ir_emit_set_rm(block, &inst->operand_rm, result);
    }
}

void ir_emit_group2(IRBlock* block, x86_instruction_t* inst, IRInstruction* shift_amount) {
    x86_group2_e opcode = (x86_group2_e)(inst->operand_reg.reg.ref - X86_REF_RAX);

    x86_size_e size_e = inst->operand_rm.size;
    u8 shift_mask = get_bit_size(size_e) - 1;
    IRInstruction* rm = ir_emit_get_rm(block, &inst->operand_rm);
    IRInstruction* shift_value = ir_emit_and(block, shift_amount, ir_emit_immediate(block, shift_mask));
    IRInstruction* result = NULL;
    IRInstruction* c = NULL;
    IRInstruction* p = NULL;
    IRInstruction* a = NULL;
    IRInstruction* z = NULL;
    IRInstruction* s = NULL;
    IRInstruction* o = NULL;

    switch (opcode) {
    case X86_GROUP2_ROL: {
        IRInstruction* size = ir_emit_get_size(block, size_e);
        IRInstruction* shift_mask = ir_emit_sub(block, size, ir_emit_immediate(block, 1));
        IRInstruction* shift_masked = ir_emit_and(block, shift_value, shift_mask);
        result = ir_emit_rotate(block, rm, shift_masked, size_e, false);
        c = ir_emit_and(block, result, ir_emit_immediate(block, 1));
        IRInstruction* msb = ir_emit_get_sign(block, result, size_e);
        o = ir_emit_xor(block, c, msb);
        break;
    }
    case X86_GROUP2_ROR: {
        IRInstruction* size = ir_emit_get_size(block, size_e);
        IRInstruction* shift_mask = ir_emit_sub(block, size, ir_emit_immediate(block, 1));
        IRInstruction* shift_masked = ir_emit_and(block, shift_value, shift_mask);
        result = ir_emit_rotate(block, rm, shift_masked, size_e, true);
        c = ir_emit_get_sign(block, result, size_e);
        WARN("ROR OF unimplemented");
        break;
    }
    case X86_GROUP2_RCL: {
        ERROR("Why? :(");
        break;
    }
    case X86_GROUP2_RCR: {
        ERROR("Why? :(");
        break;
    }
    case X86_GROUP2_SAL:
    case X86_GROUP2_SHL: {
        IRInstruction* msb_mask = ir_emit_get_shift_mask_left(block, shift_value, size_e);
        result = ir_emit_shift_left(block, rm, shift_value);
        c = ir_emit_equal(block, ir_emit_and(block, rm, msb_mask), msb_mask);
        IRInstruction* sign = ir_emit_get_sign(block, result, size_e);
        o = ir_emit_xor(block, c, sign);
        break;
    }
    case X86_GROUP2_SHR: {
        IRInstruction* msb_mask = ir_emit_get_shift_mask_right(block, shift_value);
        result = ir_emit_shift_right(block, rm, shift_value);
        c = ir_emit_equal(block, ir_emit_and(block, rm, msb_mask), msb_mask);
        o = ir_emit_get_sign(block, rm, size_e);
        break;
    }
    case X86_GROUP2_SAR: {
        // Shift left to place MSB to bit 63
        IRInstruction* shift_left_count = ir_emit_immediate(block, 64 - get_bit_size(size_e));
        IRInstruction* shifted_left = ir_emit_shift_left(block, rm, shift_left_count);
        IRInstruction* shift_right = ir_emit_add(block, shift_left_count, shift_value);
        result = ir_emit_shift_right_arithmetic(block, shifted_left, shift_right);
        o = ir_emit_immediate(block, 0);
        IRInstruction* msb_mask = ir_emit_get_shift_mask_right(block, shift_value);
        c = ir_emit_equal(block, ir_emit_and(block, rm, msb_mask), msb_mask);
        break;
    }
    }

    p = ir_emit_get_parity(block, result);
    z = ir_emit_get_zero(block, result, size_e);
    s = ir_emit_get_sign(block, result, size_e);

    ir_emit_set_cpazso(block, c, p, a, z, s, o);

    ir_emit_set_rm(block, &inst->operand_rm, result);
}

void ir_emit_group3(IRBlock* block, x86_instruction_t* inst) {
    x86_group3_e opcode = (x86_group3_e)(inst->operand_reg.reg.ref - X86_REF_RAX);

    x86_size_e size_e = inst->operand_rm.size;
    IRInstruction* rm = ir_emit_get_rm(block, &inst->operand_rm);
    IRInstruction* result = NULL;
    IRInstruction* c = NULL;
    IRInstruction* p = NULL;
    IRInstruction* a = NULL;
    IRInstruction* z = NULL;
    IRInstruction* s = NULL;
    IRInstruction* o = NULL;

    switch (opcode) {
    case X86_GROUP3_TEST:
    case X86_GROUP3_TEST_: {
        IRInstruction* imm = ir_emit_immediate_sext(block, &inst->operand_imm);
        IRInstruction* masked = ir_emit_and(block, rm, imm);
        s = ir_emit_get_sign(block, masked, size_e);
        z = ir_emit_get_zero(block, masked, size_e);
        p = ir_emit_get_parity(block, masked);
        break;
    }
    case X86_GROUP3_NOT: {
        result = ir_emit_not(block, rm);
        break;
    }
    case X86_GROUP3_NEG: {
        IRInstruction* zero = ir_emit_immediate(block, 0);
        result = ir_emit_sub(block, zero, rm);
        z = ir_emit_get_zero(block, result, size_e);
        c = ir_emit_equal(block, z, zero);
        s = ir_emit_get_sign(block, result, size_e);
        o = ir_emit_get_overflow_sub(block, zero, rm, result, size_e);
        a = ir_emit_get_aux_sub(block, zero, rm);
        p = ir_emit_get_parity(block, result);
        break;
    }
    case X86_GROUP3_MUL: {
        ERROR("Unimplemented");
        break;
    }
    case X86_GROUP3_IMUL: {
        ERROR("Unimplemented");
        break;
    }
    case X86_GROUP3_DIV: {
        switch (size_e) {
        case X86_SIZE_BYTE: {
            // ax / rm, al := quotient, ah := remainder
            IRInstruction* ax = get_reg(block, X86_REF_RAX, X86_SIZE_WORD);
            IRInstruction* quotient = ir_emit_divuw(block, ax, rm);
            IRInstruction* remainder = ir_emit_remuw(block, ax, rm);
            ir_emit_set_reg(block, X86_REF_RAX, X86_SIZE_BYTE, quotient);
            ir_emit_set_reg(block, X86_REF_RAX, X86_SIZE_BYTE, remainder, true);
            break;
        }
        case X86_SIZE_WORD: {
            // dx:ax / rm, ax := quotient, dx := remainder
            IRInstruction* ax = get_reg(block, X86_REF_RAX, X86_SIZE_WORD);
            IRInstruction* dx = get_reg(block, X86_REF_RDX, X86_SIZE_WORD);
            IRInstruction* dx_shifted = ir_emit_shift_left(block, dx, ir_emit_immediate(block, 16));
            IRInstruction* dx_ax = ir_emit_or(block, dx_shifted, ax);
            IRInstruction* quotient = ir_emit_divuw(block, dx_ax, rm);
            IRInstruction* remainder = ir_emit_remuw(block, dx_ax, rm);
            ir_emit_set_reg(block, X86_REF_RAX, X86_SIZE_WORD, quotient);
            ir_emit_set_reg(block, X86_REF_RDX, X86_SIZE_WORD, remainder);
            break;
        }
        case X86_SIZE_DWORD: {
            // edx:eax / rm, eax := quotient, edx := remainder
            IRInstruction* eax = get_reg(block, X86_REF_RAX, X86_SIZE_DWORD);
            IRInstruction* edx = get_reg(block, X86_REF_RDX, X86_SIZE_DWORD);
            IRInstruction* edx_shifted = ir_emit_shift_left(block, edx, ir_emit_immediate(block, 32));
            IRInstruction* edx_eax = ir_emit_or(block, edx_shifted, eax);
            IRInstruction* quotient = ir_emit_divu(block, edx_eax, rm);
            IRInstruction* remainder = ir_emit_remu(block, edx_eax, rm);
            ir_emit_set_reg(block, X86_REF_RAX, X86_SIZE_DWORD, quotient);
            ir_emit_set_reg(block, X86_REF_RDX, X86_SIZE_DWORD, remainder);
            break;
        }
        case X86_SIZE_QWORD: {
            // rdx:rax / rm, rax := quotient, rdx := remainder
            constexpr static std::array reg_refs = {X86_REF_RAX, X86_REF_RDX};

            ir_store_partial_state(block, reg_refs);

            ir_emit_divu128(block, rm);

            ir_load_partial_state(block, reg_refs);
            break;
        }
        default: {
            UNREACHABLE();
        }
        }
        break;
    }
    case X86_GROUP3_IDIV: {
        switch (size_e) {
        case X86_SIZE_BYTE: {
            // ax / rm, al := quotient, ah := remainder
            IRInstruction* ax = get_reg(block, X86_REF_RAX, X86_SIZE_WORD);
            IRInstruction* quotient = ir_emit_divw(block, ax, rm);
            IRInstruction* remainder = ir_emit_remw(block, ax, rm);
            ir_emit_set_reg(block, X86_REF_RAX, X86_SIZE_BYTE, quotient);
            ir_emit_set_reg(block, X86_REF_RAX, X86_SIZE_BYTE, remainder, true);
            break;
        }
        case X86_SIZE_WORD: {
            // dx:ax / rm, ax := quotient, dx := remainder
            IRInstruction* ax = get_reg(block, X86_REF_RAX, X86_SIZE_WORD);
            IRInstruction* dx = get_reg(block, X86_REF_RDX, X86_SIZE_WORD);
            IRInstruction* dx_shifted = ir_emit_shift_left(block, dx, ir_emit_immediate(block, 16));
            IRInstruction* dx_ax = ir_emit_or(block, dx_shifted, ax);
            IRInstruction* quotient = ir_emit_divw(block, dx_ax, rm);
            IRInstruction* remainder = ir_emit_remw(block, dx_ax, rm);
            ir_emit_set_reg(block, X86_REF_RAX, X86_SIZE_WORD, quotient);
            ir_emit_set_reg(block, X86_REF_RDX, X86_SIZE_WORD, remainder);
            break;
        }
        case X86_SIZE_DWORD: {
            // edx:eax / rm, eax := quotient, edx := remainder
            IRInstruction* eax = get_reg(block, X86_REF_RAX, X86_SIZE_DWORD);
            IRInstruction* edx = get_reg(block, X86_REF_RDX, X86_SIZE_DWORD);
            IRInstruction* edx_shifted = ir_emit_shift_left(block, edx, ir_emit_immediate(block, 32));
            IRInstruction* edx_eax = ir_emit_or(block, edx_shifted, eax);
            IRInstruction* quotient = ir_emit_div(block, edx_eax, rm);
            IRInstruction* remainder = ir_emit_rem(block, edx_eax, rm);
            ir_emit_set_reg(block, X86_REF_RAX, X86_SIZE_DWORD, quotient);
            ir_emit_set_reg(block, X86_REF_RDX, X86_SIZE_DWORD, remainder);
            break;
        }
        case X86_SIZE_QWORD: {
            // rdx:rax / rm, rax := quotient, rdx := remainder
            constexpr static std::array reg_refs = {X86_REF_RAX, X86_REF_RDX};

            ir_store_partial_state(block, reg_refs);

            ir_emit_div128(block, rm);

            ir_load_partial_state(block, reg_refs);
            break;
        }
        default: {
            UNREACHABLE();
        }
        }
        break;
    }
    }

    ir_emit_set_cpazso(block, c, p, a, z, s, o);

    if (result) {
        ir_emit_set_rm(block, &inst->operand_rm, result);
    }
}

IRInstruction* ir_emit_get_cc(IRBlock* block, u8 opcode) {
    switch (opcode & 0xF) {
    case 0:
        return ir_emit_get_flag(block, X86_REF_OF);
    case 1:
        return ir_emit_get_flag_not(block, X86_REF_OF);
    case 2:
        return ir_emit_get_flag(block, X86_REF_CF);
    case 3:
        return ir_emit_get_flag_not(block, X86_REF_CF);
    case 4:
        return ir_emit_get_flag(block, X86_REF_ZF);
    case 5:
        return ir_emit_get_flag_not(block, X86_REF_ZF);
    case 6:
        return ir_emit_or(block, ir_emit_get_flag(block, X86_REF_CF), ir_emit_get_flag(block, X86_REF_ZF));
    case 7:
        return ir_emit_and(block, ir_emit_get_flag_not(block, X86_REF_CF), ir_emit_get_flag_not(block, X86_REF_ZF));
    case 8:
        return ir_emit_get_flag(block, X86_REF_SF);
    case 9:
        return ir_emit_get_flag_not(block, X86_REF_SF);
    case 10:
        return ir_emit_get_flag(block, X86_REF_PF);
    case 11:
        return ir_emit_get_flag_not(block, X86_REF_PF);
    case 12:
        return ir_emit_not_equal(block, ir_emit_get_flag(block, X86_REF_SF), ir_emit_get_flag(block, X86_REF_OF));
    case 13:
        return ir_emit_equal(block, ir_emit_get_flag(block, X86_REF_SF), ir_emit_get_flag(block, X86_REF_OF));
    case 14:
        return ir_emit_or(block, ir_emit_equal(block, ir_emit_get_flag(block, X86_REF_ZF), ir_emit_immediate(block, 1)),
                          ir_emit_not_equal(block, ir_emit_get_flag(block, X86_REF_SF), ir_emit_get_flag(block, X86_REF_OF)));
    case 15:
        return ir_emit_and(block, ir_emit_equal(block, ir_emit_get_flag(block, X86_REF_ZF), ir_emit_immediate(block, 0)),
                           ir_emit_equal(block, ir_emit_get_flag(block, X86_REF_SF), ir_emit_get_flag(block, X86_REF_OF)));
    }

    ERROR("Invalid condition code");
}

void ir_emit_setcc(IRBlock* block, x86_instruction_t* inst) {
    ir_emit_set_rm(block, &inst->operand_rm, ir_emit_get_cc(block, inst->opcode));
}
