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
        SSAInstruction* guest = ir_emit_get_guest(block, reg);
        ir_emit_store_guest_to_memory(block, reg, guest);
    }
}

void ir_load_partial_state(IRBlock* block, std::span<const x86_ref_e> refs) {
    for (x86_ref_e reg : refs) {
        SSAInstruction* guest = ir_emit_load_guest_from_memory(block, reg);
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
    return 0;
}

x86_operand_t get_full_reg(x86_ref_e ref) {
    x86_operand_t operand;
    operand.type = X86_OP_TYPE_REGISTER;
    operand.reg.ref = ref;
    operand.size = X86_SIZE_QWORD;
    return operand;
}

SSAInstruction* get_reg(IRBlock* block, x86_ref_e ref, x86_size_e size_e) {
    x86_operand_t operand = get_full_reg(ref);
    operand.size = size_e;
    return ir_emit_get_reg(block, &operand);
}

SSAInstruction* ir_emit_no_operands(IRBlock* block, IROpcode opcode) {
    SSAInstruction instruction(opcode, {});
    return block->InsertAtEnd(std::move(instruction));
}

SSAInstruction* ir_emit_one_operand(IRBlock* block, IROpcode opcode, SSAInstruction* source) {
    SSAInstruction instruction(opcode, {source});
    return block->InsertAtEnd(std::move(instruction));
}

SSAInstruction* ir_emit_two_operands(IRBlock* block, IROpcode opcode, SSAInstruction* source1, SSAInstruction* source2) {
    SSAInstruction instruction(opcode, {source1, source2});
    return block->InsertAtEnd(std::move(instruction));
}

SSAInstruction* ir_emit_three_operands(IRBlock* block, IROpcode opcode, SSAInstruction* source1, SSAInstruction* source2, SSAInstruction* source3) {
    SSAInstruction instruction(opcode, {source1, source2, source3});
    return block->InsertAtEnd(std::move(instruction));
}

SSAInstruction* ir_emit_four_operands(IRBlock* block, IROpcode opcode, SSAInstruction* source1, SSAInstruction* source2, SSAInstruction* source3,
                                      SSAInstruction* source4) {
    SSAInstruction instruction(opcode, {source1, source2, source3, source4});
    return block->InsertAtEnd(std::move(instruction));
}

void ir_emit_runtime_comment(IRBlock* block, const std::string& comment) {
    SSAInstruction instruction(comment);
    block->InsertAtEnd(std::move(instruction));
}

SSAInstruction* ir_emit_add(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Add, source1, source2);
}

SSAInstruction* ir_emit_sub(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Sub, source1, source2);
}

SSAInstruction* ir_emit_shift_left(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::ShiftLeft, source1, source2);
}

SSAInstruction* ir_emit_shift_right(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::ShiftRight, source1, source2);
}

SSAInstruction* ir_emit_shift_right_arithmetic(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::ShiftRightArithmetic, source1, source2);
}

SSAInstruction* ir_emit_rotate(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2, x86_size_e size_e, bool right) {
    u8 size = get_bit_size(size_e);
    IROpcode type;
    SSAInstruction* count = source2;
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

SSAInstruction* ir_emit_select(IRBlock* block, SSAInstruction* condition, SSAInstruction* true_value, SSAInstruction* false_value) {
    return ir_emit_three_operands(block, IROpcode::Select, condition, true_value, false_value);
}

SSAInstruction* ir_emit_clz(IRBlock* block, SSAInstruction* source) {
    return ir_emit_one_operand(block, IROpcode::Clz, source);
}

SSAInstruction* ir_emit_ctzh(IRBlock* block, SSAInstruction* source) {
    return ir_emit_one_operand(block, IROpcode::Ctzh, source);
}

SSAInstruction* ir_emit_ctzw(IRBlock* block, SSAInstruction* source) {
    return ir_emit_one_operand(block, IROpcode::Ctzw, source);
}

SSAInstruction* ir_emit_ctz(IRBlock* block, SSAInstruction* source) {
    return ir_emit_one_operand(block, IROpcode::Ctz, source);
}

SSAInstruction* ir_emit_addi(IRBlock* block, SSAInstruction* source, i64 imm) {
    SSAInstruction* instruction = ir_emit_one_operand(block, IROpcode::Addi, source);
    instruction->SetImmediateData(imm);
    return instruction;
}

SSAInstruction* ir_emit_and(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::And, source1, source2);
}

SSAInstruction* ir_emit_or(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Or, source1, source2);
}

SSAInstruction* ir_emit_xor(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Xor, source1, source2);
}

SSAInstruction* ir_emit_not(IRBlock* block, SSAInstruction* source) {
    return ir_emit_one_operand(block, IROpcode::Not, source);
}

SSAInstruction* ir_emit_neg(IRBlock* block, SSAInstruction* source) {
    return ir_emit_one_operand(block, IROpcode::Neg, source);
}

SSAInstruction* ir_emit_equal(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Equal, source1, source2);
}

SSAInstruction* ir_emit_not_equal(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::NotEqual, source1, source2);
}

SSAInstruction* ir_emit_greater_than_signed(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::SetLessThanSigned, source2, source1);
}

SSAInstruction* ir_emit_less_than_signed(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::SetLessThanSigned, source1, source2);
}

SSAInstruction* ir_emit_greater_than_unsigned(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::SetLessThanUnsigned, source2, source1);
}

SSAInstruction* ir_emit_less_than_unsigned(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::SetLessThanUnsigned, source1, source2);
}

SSAInstruction* ir_emit_div(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Div, source1, source2);
}

SSAInstruction* ir_emit_divu(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Divu, source1, source2);
}

SSAInstruction* ir_emit_rem(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Rem, source1, source2);
}

SSAInstruction* ir_emit_remu(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Remu, source1, source2);
}

SSAInstruction* ir_emit_divw(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Divw, source1, source2);
}

SSAInstruction* ir_emit_divuw(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Divuw, source1, source2);
}

SSAInstruction* ir_emit_remw(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Remw, source1, source2);
}

SSAInstruction* ir_emit_remuw(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Remuw, source1, source2);
}

SSAInstruction* ir_emit_mul(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Mul, source1, source2);
}

SSAInstruction* ir_emit_mulh(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Mulh, source1, source2);
}

SSAInstruction* ir_emit_mulhu(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::Mulhu, source1, source2);
}

SSAInstruction* ir_emit_get_flags(IRBlock* block) {
    SSAInstruction* c = ir_emit_get_flag(block, X86_REF_CF);
    SSAInstruction* p = ir_emit_get_flag(block, X86_REF_PF);
    SSAInstruction* a = ir_emit_get_flag(block, X86_REF_AF);
    SSAInstruction* z = ir_emit_get_flag(block, X86_REF_ZF);
    SSAInstruction* s = ir_emit_get_flag(block, X86_REF_SF);
    SSAInstruction* o = ir_emit_get_flag(block, X86_REF_OF);
    SSAInstruction* d = ir_emit_get_flag(block, X86_REF_DF);

    SSAInstruction* p_shifted = ir_emit_shift_left(block, p, ir_emit_immediate(block, 2));
    SSAInstruction* a_shifted = ir_emit_shift_left(block, a, ir_emit_immediate(block, 4));
    SSAInstruction* z_shifted = ir_emit_shift_left(block, z, ir_emit_immediate(block, 6));
    SSAInstruction* s_shifted = ir_emit_shift_left(block, s, ir_emit_immediate(block, 7));
    SSAInstruction* d_shifted = ir_emit_shift_left(block, d, ir_emit_immediate(block, 10));
    SSAInstruction* o_shifted = ir_emit_shift_left(block, o, ir_emit_immediate(block, 11));

    SSAInstruction* c_p = ir_emit_or(block, c, p_shifted);
    SSAInstruction* a_z = ir_emit_or(block, a_shifted, z_shifted);
    SSAInstruction* a_z_o = ir_emit_or(block, a_z, o_shifted);
    SSAInstruction* c_p_s = ir_emit_or(block, c_p, s_shifted);
    SSAInstruction* c_p_s_d = ir_emit_or(block, c_p_s, d_shifted);
    SSAInstruction* result_almost = ir_emit_or(block, c_p_s_d, a_z_o);
    SSAInstruction* result = ir_emit_or(block, result_almost, ir_emit_immediate(block, 0b10)); // always set bit 1

    return result;
}

void ir_emit_set_flags(IRBlock* block, SSAInstruction* flags) {
    SSAInstruction* c = ir_emit_and(block, flags, ir_emit_immediate(block, 1));
    SSAInstruction* p = ir_emit_and(block, ir_emit_shift_right(block, flags, ir_emit_immediate(block, 2)), ir_emit_immediate(block, 1));
    SSAInstruction* a = ir_emit_and(block, ir_emit_shift_right(block, flags, ir_emit_immediate(block, 4)), ir_emit_immediate(block, 1));
    SSAInstruction* z = ir_emit_and(block, ir_emit_shift_right(block, flags, ir_emit_immediate(block, 6)), ir_emit_immediate(block, 1));
    SSAInstruction* s = ir_emit_and(block, ir_emit_shift_right(block, flags, ir_emit_immediate(block, 7)), ir_emit_immediate(block, 1));
    SSAInstruction* o = ir_emit_and(block, ir_emit_shift_right(block, flags, ir_emit_immediate(block, 11)), ir_emit_immediate(block, 1));
    SSAInstruction* d = ir_emit_and(block, ir_emit_shift_right(block, flags, ir_emit_immediate(block, 10)), ir_emit_immediate(block, 1));

    ir_emit_set_flag(block, X86_REF_CF, c);
    ir_emit_set_flag(block, X86_REF_PF, p);
    ir_emit_set_flag(block, X86_REF_AF, a);
    ir_emit_set_flag(block, X86_REF_ZF, z);
    ir_emit_set_flag(block, X86_REF_SF, s);
    ir_emit_set_flag(block, X86_REF_OF, o);
    ir_emit_set_flag(block, X86_REF_DF, d);
}

void ir_emit_div128(IRBlock* block, SSAInstruction* divisor) {
    SSAInstruction* instruction = ir_emit_one_operand(block, IROpcode::Div128, divisor);
    instruction->Lock();
}

SSAInstruction* ir_emit_divu128(IRBlock* block, SSAInstruction* divisor) {
    SSAInstruction* instruction = ir_emit_one_operand(block, IROpcode::Divu128, divisor);
    instruction->Lock();
    return instruction;
}

SSAInstruction* ir_emit_lea(IRBlock* block, x86_operand_t* operand_rm) {
    SSAInstruction* (*get_guest)(IRBlock* block, x86_ref_e reg) = operand_rm->memory.address_override ? ir_emit_get_gpr32 : ir_emit_get_gpr64;

    SSAInstruction* base = operand_rm->memory.base != X86_REF_COUNT ? get_guest(block, operand_rm->memory.base) : ir_emit_immediate(block, 0);

    SSAInstruction* base_final = base;
    if (operand_rm->memory.fs_override) {
        SSAInstruction* fs = ir_emit_get_guest(block, X86_REF_FS);
        base_final = ir_emit_add(block, base, fs);
    } else if (operand_rm->memory.gs_override) {
        SSAInstruction* gs = ir_emit_get_guest(block, X86_REF_GS);
        base_final = ir_emit_add(block, base, gs);
    }

    SSAInstruction* index = operand_rm->memory.index != X86_REF_COUNT ? get_guest(block, operand_rm->memory.index) : nullptr;

    SSAInstruction* address = base_final;
    if (index) {
        SSAInstruction* scale = ir_emit_immediate(block, operand_rm->memory.scale);
        SSAInstruction* scaled_index = ir_emit_shift_left(block, index, scale);
        address = ir_emit_add(block, base_final, scaled_index);
    }

    SSAInstruction* displaced_address = address;
    if (operand_rm->memory.displacement) {
        if (IsValidSigned12BitImm(operand_rm->memory.displacement)) {
            displaced_address = ir_emit_addi(block, address, operand_rm->memory.displacement);
        } else {
            // Might benefit more since loading large immediates takes many instructions and
            // this can be reused after optimization passes
            SSAInstruction* displacement = ir_emit_immediate(block, operand_rm->memory.displacement);
            displaced_address = ir_emit_add(block, address, displacement);
        }
    }

    SSAInstruction* final_address = displaced_address;
    if (operand_rm->memory.address_override) {
        final_address = ir_emit_zext32(block, displaced_address);
    }

    return final_address;
}

SSAInstruction* ir_emit_zext8(IRBlock* block, SSAInstruction* source) {
    return ir_emit_one_operand(block, IROpcode::Zext8, source);
}

SSAInstruction* ir_emit_zext16(IRBlock* block, SSAInstruction* source) {
    return ir_emit_one_operand(block, IROpcode::Zext16, source);
}

SSAInstruction* ir_emit_zext32(IRBlock* block, SSAInstruction* source) {
    return ir_emit_one_operand(block, IROpcode::Zext32, source);
}

SSAInstruction* ir_emit_zext(IRBlock* block, SSAInstruction* source, x86_size_e size) {
    switch (size) {
    case X86_SIZE_BYTE:
        return ir_emit_zext8(block, source);
    case X86_SIZE_WORD:
        return ir_emit_zext16(block, source);
    case X86_SIZE_DWORD:
        return ir_emit_zext32(block, source);
    case X86_SIZE_QWORD:
        return source;
    default:
        ERROR("Invalid size");
        return source;
    }
}

SSAInstruction* ir_emit_get_thread_state_pointer(IRBlock* block) {
    return ir_emit_no_operands(block, IROpcode::GetThreadStatePointer);
}

SSAInstruction* ir_emit_get_parity(IRBlock* block, SSAInstruction* source) {
    return ir_emit_one_operand(block, IROpcode::Parity, source);
}

SSAInstruction* ir_emit_sext8(IRBlock* block, SSAInstruction* source) {
    return ir_emit_one_operand(block, IROpcode::Sext8, source);
}

SSAInstruction* ir_emit_sext16(IRBlock* block, SSAInstruction* source) {
    return ir_emit_one_operand(block, IROpcode::Sext16, source);
}

SSAInstruction* ir_emit_sext32(IRBlock* block, SSAInstruction* source) {
    return ir_emit_one_operand(block, IROpcode::Sext32, source);
}

SSAInstruction* ir_emit_sext(IRBlock* block, SSAInstruction* source, x86_size_e size) {
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
        return source;
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

    SSAInstruction instruction(IROpcode::Syscall, {});
    instruction.Lock();
    block->InsertAtEnd(std::move(instruction));

    ir_load_partial_state(block, out_regs);
}

SSAInstruction* ir_emit_insert_integer_to_vector(IRBlock* block, SSAInstruction* source, SSAInstruction* dest, u8 idx, x86_size_e sz) {
    u64 immediate_data = (u64)sz << 8 | idx;
    SSAInstruction* instruction = ir_emit_two_operands(block, IROpcode::VInsertInteger, source, dest);
    instruction->SetImmediateData(immediate_data);
    return instruction;
}

SSAInstruction* ir_emit_extract_integer_from_vector(IRBlock* block, SSAInstruction* src, u8 idx, x86_size_e sz) {
    u64 immediate_data = (u64)sz << 8 | idx;
    SSAInstruction* instruction = ir_emit_one_operand(block, IROpcode::VExtractInteger, src);
    instruction->SetImmediateData(immediate_data);
    return instruction;
}

SSAInstruction* ir_emit_vector_unpack_byte_low(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::VUnpackByteLow, source1, source2);
}

SSAInstruction* ir_emit_vector_unpack_word_low(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::VUnpackWordLow, source1, source2);
}

SSAInstruction* ir_emit_vector_unpack_dword_low(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::VUnpackDWordLow, source1, source2);
}

SSAInstruction* ir_emit_vector_unpack_qword_low(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::VUnpackQWordLow, source1, source2);
}

SSAInstruction* ir_emit_cast_vector_integer(IRBlock* block, SSAInstruction* source) {
    return ir_emit_one_operand(block, IROpcode::CastVectorFromInteger, source);
}

SSAInstruction* ir_emit_cast_integer_vector(IRBlock* block, SSAInstruction* source) {
    return ir_emit_one_operand(block, IROpcode::CastIntegerFromVector, source);
}

SSAInstruction* ir_emit_vector_packed_and(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::VAnd, source1, source2);
}

SSAInstruction* ir_emit_vector_packed_or(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::VOr, source1, source2);
}

SSAInstruction* ir_emit_vector_packed_xor(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::VXor, source1, source2);
}

SSAInstruction* ir_emit_vector_packed_shift_right(IRBlock* block, SSAInstruction* source, SSAInstruction* imm) {
    return ir_emit_two_operands(block, IROpcode::VShiftRight, source, imm);
}

SSAInstruction* ir_emit_vector_packed_shift_left(IRBlock* block, SSAInstruction* source, SSAInstruction* imm) {
    return ir_emit_two_operands(block, IROpcode::VShiftLeft, source, imm);
}

SSAInstruction* ir_emit_vector_packed_sub_byte(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::VPackedSubByte, source1, source2);
}

SSAInstruction* ir_emit_vector_packed_add_qword(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::VPackedAddQWord, source1, source2);
}

SSAInstruction* ir_emit_vector_packed_compare_eq_byte(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::VPackedEqualByte, source1, source2);
}

SSAInstruction* ir_emit_vector_packed_compare_eq_word(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::VPackedEqualWord, source1, source2);
}

SSAInstruction* ir_emit_vector_packed_compare_eq_dword(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::VPackedEqualDWord, source1, source2);
}

SSAInstruction* ir_emit_vector_packed_shuffle_dword(IRBlock* block, SSAInstruction* source, u8 control_byte) {
    SSAInstruction instruction(IROpcode::VPackedShuffleDWord, {source});
    instruction.SetImmediateData(control_byte);
    return block->InsertAtEnd(std::move(instruction));
}

SSAInstruction* ir_emit_vector_packed_move_byte_mask(IRBlock* block, SSAInstruction* source) {
    return ir_emit_one_operand(block, IROpcode::VMoveByteMask, source);
}

SSAInstruction* ir_emit_vector_packed_min_byte(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    return ir_emit_two_operands(block, IROpcode::VPackedMinByte, source1, source2);
}

SSAInstruction* ir_emit_load_guest_from_memory(IRBlock* block, x86_ref_e ref) {
    if (ref == X86_REF_COUNT) {
        ERROR("Invalid register reference");
    }

    SSAInstruction instruction(IROpcode::LoadGuestFromMemory, ref);
    return block->InsertAtEnd(std::move(instruction));
}

void ir_emit_store_guest_to_memory(IRBlock* block, x86_ref_e ref, SSAInstruction* source) {
    if (ref == X86_REF_COUNT) {
        ERROR("Invalid register reference");
    }

    SSAInstruction instruction(IROpcode::StoreGuestToMemory, ref, source);
    instruction.Lock();
    block->InsertAtEnd(std::move(instruction));
}

SSAInstruction* ir_emit_get_guest(IRBlock* block, x86_ref_e ref) {
    if (ref == X86_REF_COUNT) {
        ERROR("Invalid register reference");
    }

    SSAInstruction instruction(IROpcode::GetGuest, ref);
    return block->InsertAtEnd(std::move(instruction));
}

void ir_emit_set_guest(IRBlock* block, x86_ref_e ref, SSAInstruction* source) {
    if (ref == X86_REF_COUNT) {
        ERROR("Invalid register reference");
    }

    SSAInstruction instruction(IROpcode::SetGuest, ref, source);
    block->InsertAtEnd(std::move(instruction));
}

SSAInstruction* ir_emit_get_flag(IRBlock* block, x86_ref_e flag) {
    if (flag < X86_REF_CF || flag > X86_REF_OF) {
        ERROR("Invalid flag reference");
    }

    return ir_emit_get_guest(block, flag);
}

SSAInstruction* ir_emit_get_flag_not(IRBlock* block, x86_ref_e flag) {
    SSAInstruction* instruction = ir_emit_get_flag(block, flag);
    return ir_emit_xor(block, instruction, ir_emit_immediate(block, 1));
}

void ir_emit_set_flag(IRBlock* block, x86_ref_e flag, SSAInstruction* source) {
    if (flag < X86_REF_CF || flag > X86_REF_OF) {
        ERROR("Invalid flag reference");
    }

    ir_emit_set_guest(block, flag, source);
}

SSAInstruction* ir_emit_read_byte(IRBlock* block, SSAInstruction* address) {
    return ir_emit_one_operand(block, IROpcode::ReadByte, address);
}

SSAInstruction* ir_emit_read_word(IRBlock* block, SSAInstruction* address) {
    return ir_emit_one_operand(block, IROpcode::ReadWord, address);
}

SSAInstruction* ir_emit_read_dword(IRBlock* block, SSAInstruction* address) {
    return ir_emit_one_operand(block, IROpcode::ReadDWord, address);
}

SSAInstruction* ir_emit_read_qword(IRBlock* block, SSAInstruction* address) {
    return ir_emit_one_operand(block, IROpcode::ReadQWord, address);
}

SSAInstruction* ir_emit_read_xmmword(IRBlock* block, SSAInstruction* address) {
    return ir_emit_one_operand(block, IROpcode::ReadXmmWord, address);
}

void ir_emit_write_byte(IRBlock* block, SSAInstruction* address, SSAInstruction* source) {
    SSAInstruction* instruction = ir_emit_two_operands(block, IROpcode::WriteByte, address, source);
    instruction->Lock();
}

void ir_emit_write_word(IRBlock* block, SSAInstruction* address, SSAInstruction* source) {
    SSAInstruction* instruction = ir_emit_two_operands(block, IROpcode::WriteWord, address, source);
    instruction->Lock();
}

void ir_emit_write_dword(IRBlock* block, SSAInstruction* address, SSAInstruction* source) {
    SSAInstruction* instruction = ir_emit_two_operands(block, IROpcode::WriteDWord, address, source);
    instruction->Lock();
}

void ir_emit_write_qword(IRBlock* block, SSAInstruction* address, SSAInstruction* source) {
    SSAInstruction* instruction = ir_emit_two_operands(block, IROpcode::WriteQWord, address, source);
    instruction->Lock();
}

void ir_emit_write_xmmword(IRBlock* block, SSAInstruction* address, SSAInstruction* source) {
    SSAInstruction* instruction = ir_emit_two_operands(block, IROpcode::WriteXmmWord, address, source);
    instruction->Lock();
}

SSAInstruction* ir_emit_amoadd(IRBlock* block, SSAInstruction* address, SSAInstruction* source, MemoryOrdering ordering, x86_size_e size) {
    IROpcode opcode = IROpcode::Null;
    switch (size) {
    case X86_SIZE_BYTE:
        opcode = IROpcode::AmoAdd8;
        break;
    case X86_SIZE_WORD:
        opcode = IROpcode::AmoAdd16;
        break;
    case X86_SIZE_DWORD:
        opcode = IROpcode::AmoAdd32;
        break;
    case X86_SIZE_QWORD:
        opcode = IROpcode::AmoAdd64;
        break;
    default:
        ERROR("Invalid size");
    }

    SSAInstruction* instruction = ir_emit_two_operands(block, opcode, address, source);
    instruction->SetImmediateData((u8)ordering);
    instruction->Lock();
    return instruction;
}

SSAInstruction* ir_emit_amoxor(IRBlock* block, SSAInstruction* address, SSAInstruction* source, MemoryOrdering ordering, x86_size_e size) {
    IROpcode opcode = IROpcode::Null;
    switch (size) {
    case X86_SIZE_BYTE:
        opcode = IROpcode::AmoXor8;
        break;
    case X86_SIZE_WORD:
        opcode = IROpcode::AmoXor16;
        break;
    case X86_SIZE_DWORD:
        opcode = IROpcode::AmoXor32;
        break;
    case X86_SIZE_QWORD:
        opcode = IROpcode::AmoXor64;
        break;
    default:
        ERROR("Invalid size");
    }

    SSAInstruction* instruction = ir_emit_two_operands(block, opcode, address, source);
    instruction->SetImmediateData((u8)ordering);
    instruction->Lock();
    return instruction;
}

SSAInstruction* ir_emit_amoor(IRBlock* block, SSAInstruction* address, SSAInstruction* source, MemoryOrdering ordering, x86_size_e size) {
    IROpcode opcode = IROpcode::Null;
    switch (size) {
    case X86_SIZE_BYTE:
        opcode = IROpcode::AmoOr8;
        break;
    case X86_SIZE_WORD:
        opcode = IROpcode::AmoOr16;
        break;
    case X86_SIZE_DWORD:
        opcode = IROpcode::AmoOr32;
        break;
    case X86_SIZE_QWORD:
        opcode = IROpcode::AmoOr64;
        break;
    default:
        ERROR("Invalid size");
    }

    SSAInstruction* instruction = ir_emit_two_operands(block, opcode, address, source);
    instruction->SetImmediateData((u8)ordering);
    instruction->Lock();
    return instruction;
}

SSAInstruction* ir_emit_amoand(IRBlock* block, SSAInstruction* address, SSAInstruction* source, MemoryOrdering ordering, x86_size_e size) {
    IROpcode opcode = IROpcode::Null;
    switch (size) {
    case X86_SIZE_BYTE:
        opcode = IROpcode::AmoAnd8;
        break;
    case X86_SIZE_WORD:
        opcode = IROpcode::AmoAnd16;
        break;
    case X86_SIZE_DWORD:
        opcode = IROpcode::AmoAnd32;
        break;
    case X86_SIZE_QWORD:
        opcode = IROpcode::AmoAnd64;
        break;
    default:
        ERROR("Invalid size");
    }

    SSAInstruction* instruction = ir_emit_two_operands(block, opcode, address, source);
    instruction->SetImmediateData((u8)ordering);
    instruction->Lock();
    return instruction;
}

SSAInstruction* ir_emit_amoswap(IRBlock* block, SSAInstruction* address, SSAInstruction* source, MemoryOrdering ordering, x86_size_e size) {
    IROpcode opcode = IROpcode::Null;
    switch (size) {
    case X86_SIZE_BYTE:
        opcode = IROpcode::AmoSwap8;
        break;
    case X86_SIZE_WORD:
        opcode = IROpcode::AmoSwap16;
        break;
    case X86_SIZE_DWORD:
        opcode = IROpcode::AmoSwap32;
        break;
    case X86_SIZE_QWORD:
        opcode = IROpcode::AmoSwap64;
        break;
    default:
        ERROR("Invalid size");
    }

    SSAInstruction* instruction = ir_emit_two_operands(block, opcode, address, source);
    instruction->SetImmediateData((u8)ordering);
    instruction->Lock();
    return instruction;
}

SSAInstruction* ir_emit_amocas(IRBlock* block, SSAInstruction* address, SSAInstruction* expected, SSAInstruction* source, MemoryOrdering ordering,
                               x86_size_e size) {
    IROpcode opcode = IROpcode::Null;
    switch (size) {
    case X86_SIZE_BYTE:
        opcode = IROpcode::AmoCAS8;
        break;
    case X86_SIZE_WORD:
        opcode = IROpcode::AmoCAS16;
        break;
    case X86_SIZE_DWORD:
        opcode = IROpcode::AmoCAS32;
        break;
    case X86_SIZE_QWORD:
        opcode = IROpcode::AmoCAS64;
        break;
    default:
        ERROR("Invalid size");
    }

    SSAInstruction* instruction = ir_emit_three_operands(block, opcode, address, expected, source);
    instruction->SetImmediateData((u8)ordering);
    instruction->Lock();
    return instruction;
}

void ir_emit_cpuid(IRBlock* block) {
    // Similar to syscall, cpuid clobbers registers rax, rcx, rdx, rbx but preserves all other registers.
    // It uses rax and rcx as input.
    constexpr static std::array in_regs = {X86_REF_RAX, X86_REF_RCX};
    constexpr static std::array out_regs = {X86_REF_RAX, X86_REF_RBX, X86_REF_RCX, X86_REF_RDX};

    ir_store_partial_state(block, in_regs);

    SSAInstruction instruction(IROpcode::Cpuid, {});
    instruction.Lock();
    block->InsertAtEnd(std::move(instruction));

    ir_load_partial_state(block, out_regs);
}

void ir_emit_rdtsc(IRBlock* block) {
    // Has no inputs but writes to EDX:EAX
    constexpr static std::array out_regs = {X86_REF_RAX, X86_REF_RDX};

    SSAInstruction instruction(IROpcode::Rdtsc, {});
    instruction.Lock();
    block->InsertAtEnd(std::move(instruction));

    ir_load_partial_state(block, out_regs);
}

SSAInstruction* ir_emit_immediate(IRBlock* block, u64 value) {
    SSAInstruction instruction(value);
    return block->InsertAtEnd(std::move(instruction));
}

SSAInstruction* ir_emit_immediate_sext(IRBlock* block, x86_operand_t* operand) {
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

SSAInstruction* ir_emit_get_reg(IRBlock* block, x86_operand_t* operand_reg) {
    if (operand_reg->type != X86_OP_TYPE_REGISTER) {
        ERROR("Invalid operand type");
    }

    switch (operand_reg->size) {
    case X86_SIZE_BYTE: {
        if (operand_reg->reg.high8) {
            return ir_emit_get_gpr8_high(block, operand_reg->reg.ref);
        } else {
            return ir_emit_get_gpr8_low(block, operand_reg->reg.ref);
        }
    }
    case X86_SIZE_WORD:
        return ir_emit_get_gpr16(block, operand_reg->reg.ref);
    case X86_SIZE_DWORD:
        return ir_emit_get_gpr32(block, operand_reg->reg.ref);
    case X86_SIZE_QWORD:
        return ir_emit_get_gpr64(block, operand_reg->reg.ref);
    case X86_SIZE_XMM:
    case X86_SIZE_YMM:
    case X86_SIZE_ZMM:
        return ir_emit_get_vector(block, operand_reg->reg.ref);
    default:
        ERROR("Invalid register size");
        return nullptr;
    }
}

SSAInstruction* ir_emit_get_rm(IRBlock* block, x86_operand_t* operand_rm) {
    if (operand_rm->type == X86_OP_TYPE_REGISTER) {
        return ir_emit_get_reg(block, operand_rm);
    } else {
        SSAInstruction* address = ir_emit_lea(block, operand_rm);
        return ir_emit_read_memory(block, address, operand_rm->size);
    }
}

void ir_emit_set_reg(IRBlock* block, x86_operand_t* operand_reg, SSAInstruction* source) {
    switch (operand_reg->size) {
    case X86_SIZE_BYTE: {
        if (operand_reg->reg.high8) {
            return ir_emit_set_gpr8_high(block, operand_reg->reg.ref, source);
        } else {
            return ir_emit_set_gpr8_low(block, operand_reg->reg.ref, source);
        }
    }
    case X86_SIZE_WORD:
        return ir_emit_set_gpr16(block, operand_reg->reg.ref, source);
    case X86_SIZE_DWORD:
        return ir_emit_set_gpr32(block, operand_reg->reg.ref, source);
    case X86_SIZE_QWORD:
        return ir_emit_set_gpr64(block, operand_reg->reg.ref, source);
    case X86_SIZE_XMM:
    case X86_SIZE_YMM:
    case X86_SIZE_ZMM:
        return ir_emit_set_vector(block, operand_reg->reg.ref, source);
    default:
        ERROR("Invalid register size");
        return;
    }
}

void ir_emit_set_reg(IRBlock* block, x86_ref_e ref, x86_size_e size, SSAInstruction* source, bool high = false) {
    x86_operand_t operand;
    operand.type = X86_OP_TYPE_REGISTER;
    operand.reg.ref = ref;
    operand.reg.high8 = high;
    operand.size = size;
    ir_emit_set_reg(block, &operand, source);
}

void ir_emit_set_rm(IRBlock* block, x86_operand_t* operand_rm, SSAInstruction* source) {
    if (operand_rm->type == X86_OP_TYPE_REGISTER) {
        return ir_emit_set_reg(block, operand_rm, source);
    } else {
        SSAInstruction* address = ir_emit_lea(block, operand_rm);
        return ir_emit_write_memory(block, address, source, operand_rm->size);
    }
}

void ir_emit_write_memory(IRBlock* block, SSAInstruction* address, SSAInstruction* value, x86_size_e size) {
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

SSAInstruction* ir_emit_read_memory(IRBlock* block, SSAInstruction* address, x86_size_e size) {
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
        return nullptr;
    }
}

SSAInstruction* ir_emit_get_gpr8_low(IRBlock* block, x86_ref_e reg) {
    if (reg < X86_REF_RAX || reg > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    SSAInstruction* full_reg = ir_emit_get_guest(block, reg);
    SSAInstruction* instruction = ir_emit_zext8(block, full_reg);

    return instruction;
}

SSAInstruction* ir_emit_get_gpr8_high(IRBlock* block, x86_ref_e reg) {
    if (reg < X86_REF_RAX || reg > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    SSAInstruction* full_reg = ir_emit_get_guest(block, reg);
    SSAInstruction* shift = ir_emit_immediate(block, 8);
    SSAInstruction* shifted = ir_emit_shift_right(block, full_reg, shift);
    SSAInstruction* instruction = ir_emit_zext8(block, shifted);

    return instruction;
}

SSAInstruction* ir_emit_get_gpr16(IRBlock* block, x86_ref_e reg) {
    if (reg < X86_REF_RAX || reg > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    SSAInstruction* full_reg = ir_emit_get_guest(block, reg);
    SSAInstruction* instruction = ir_emit_zext16(block, full_reg);

    return instruction;
}

SSAInstruction* ir_emit_get_gpr32(IRBlock* block, x86_ref_e reg) {
    if (reg < X86_REF_RAX || reg > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    SSAInstruction* full_reg = ir_emit_get_guest(block, reg);
    SSAInstruction* instruction = ir_emit_zext32(block, full_reg);

    return instruction;
}

SSAInstruction* ir_emit_get_gpr64(IRBlock* block, x86_ref_e reg) {
    if (reg < X86_REF_RAX || reg > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    SSAInstruction* instruction = ir_emit_get_guest(block, reg);
    return instruction;
}

SSAInstruction* ir_emit_get_vector(IRBlock* block, x86_ref_e reg) {
    if (reg < X86_REF_XMM0 || reg > X86_REF_XMM15) {
        ERROR("Invalid register reference");
    }

    SSAInstruction* instruction = ir_emit_get_guest(block, reg);
    return instruction;
}

void ir_emit_set_gpr8_low(IRBlock* block, x86_ref_e reg, SSAInstruction* source) {
    SSAInstruction* full_reg = ir_emit_get_guest(block, reg);
    SSAInstruction* mask = ir_emit_immediate(block, 0xFFFFFFFFFFFFFF00);
    SSAInstruction* masked = ir_emit_and(block, full_reg, mask);
    SSAInstruction* value = ir_emit_zext8(block, source);
    SSAInstruction* final_value = ir_emit_or(block, masked, value);
    ir_emit_set_guest(block, reg, final_value);
}

void ir_emit_set_gpr8_high(IRBlock* block, x86_ref_e reg, SSAInstruction* source) {
    SSAInstruction* full_reg = ir_emit_get_guest(block, reg);
    SSAInstruction* mask = ir_emit_immediate(block, 0xFFFFFFFFFFFF00FF);
    SSAInstruction* masked = ir_emit_and(block, full_reg, mask);
    SSAInstruction* value = ir_emit_zext8(block, source);
    SSAInstruction* shift = ir_emit_immediate(block, 8);
    SSAInstruction* shifted = ir_emit_shift_left(block, value, shift);
    SSAInstruction* final_value = ir_emit_or(block, masked, shifted);
    ir_emit_set_guest(block, reg, final_value);
}

void ir_emit_set_gpr16(IRBlock* block, x86_ref_e reg, SSAInstruction* source) {
    SSAInstruction* full_reg = ir_emit_get_guest(block, reg);
    SSAInstruction* mask = ir_emit_immediate(block, 0xFFFFFFFFFFFF0000);
    SSAInstruction* masked = ir_emit_and(block, full_reg, mask);
    SSAInstruction* value = ir_emit_zext16(block, source);
    SSAInstruction* final_value = ir_emit_or(block, masked, value);
    ir_emit_set_guest(block, reg, final_value);
}

void ir_emit_set_gpr32(IRBlock* block, x86_ref_e reg, SSAInstruction* source) {
    SSAInstruction* final_value = ir_emit_zext32(block, source);
    ir_emit_set_guest(block, reg, final_value);
}

void ir_emit_set_gpr64(IRBlock* block, x86_ref_e reg, SSAInstruction* source) {
    ir_emit_set_guest(block, reg, source);
}

void ir_emit_set_vector(IRBlock* block, x86_ref_e reg, SSAInstruction* source) {
    ir_emit_set_guest(block, reg, source);
}

SSAInstruction* ir_emit_get_zero(IRBlock* block, SSAInstruction* source, x86_size_e size_e) {
    SSAInstruction* zero = ir_emit_immediate(block, 0);
    SSAInstruction* zext = ir_emit_zext(block, source, size_e);
    SSAInstruction* instruction = ir_emit_equal(block, zext, zero);

    return instruction;
}

SSAInstruction* ir_emit_get_size(IRBlock* block, x86_size_e size_e) {
    return ir_emit_immediate(block, get_bit_size(size_e));
}

SSAInstruction* ir_emit_get_sign_mask(IRBlock* block, x86_size_e size_e) {
    u16 size = get_bit_size(size_e);
    return ir_emit_immediate(block, 1ull << (size - 1));
}

SSAInstruction* ir_emit_get_shift_mask_left(IRBlock* block, SSAInstruction* source, x86_size_e size_e) {
    SSAInstruction* one = ir_emit_immediate(block, 1);
    SSAInstruction* shift_max = ir_emit_get_size(block, size_e);
    SSAInstruction* shift = ir_emit_sub(block, shift_max, source);
    SSAInstruction* mask = ir_emit_shift_left(block, one, shift);
    return mask;
}

SSAInstruction* ir_emit_get_shift_mask_right(IRBlock* block, SSAInstruction* source) {
    SSAInstruction* zero = ir_emit_immediate(block, 0);
    SSAInstruction* is_zero = ir_emit_equal(block, source, zero);
    SSAInstruction* one = ir_emit_immediate(block, 1);
    SSAInstruction* shift = ir_emit_sub(block, source, one);
    SSAInstruction* mask = ir_emit_shift_left(block, one, shift);
    return ir_emit_select(block, is_zero, zero, mask);
}

SSAInstruction* ir_emit_get_sign(IRBlock* block, SSAInstruction* source, x86_size_e size_e) {
    SSAInstruction* mask = ir_emit_get_sign_mask(block, size_e);
    SSAInstruction* masked = ir_emit_and(block, source, mask);
    SSAInstruction* instruction = ir_emit_equal(block, masked, mask);

    return instruction;
}

SSAInstruction* ir_emit_get_overflow_add(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2, SSAInstruction* result,
                                         x86_size_e size_e) {
    SSAInstruction* mask = ir_emit_get_sign_mask(block, size_e);

    // for x + y = z, overflow occurs if ((z ^ x) & (z ^ y) & mask) == mask
    // which essentially checks if the sign bits of x and y are equal, but the
    // sign bit of z is different
    SSAInstruction* xor1 = ir_emit_xor(block, result, source1);
    SSAInstruction* xor2 = ir_emit_xor(block, result, source2);
    SSAInstruction* masked1 = ir_emit_and(block, xor1, xor2);
    SSAInstruction* masked2 = ir_emit_and(block, masked1, mask);

    return ir_emit_equal(block, masked2, mask);
}

SSAInstruction* ir_emit_get_overflow_sub(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2, SSAInstruction* result,
                                         x86_size_e size_e) {
    SSAInstruction* mask = ir_emit_get_sign_mask(block, size_e);

    // for x - y = z, overflow occurs if ((x ^ y) & (x ^ z) & mask) == mask
    SSAInstruction* xor1 = ir_emit_xor(block, source1, source2);
    SSAInstruction* xor2 = ir_emit_xor(block, source1, result);
    SSAInstruction* masked1 = ir_emit_and(block, xor1, xor2);
    SSAInstruction* masked2 = ir_emit_and(block, masked1, mask);

    return ir_emit_equal(block, masked2, mask);
}

SSAInstruction* ir_emit_get_carry_add(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2, SSAInstruction* result, x86_size_e size_e) {
    (void)source2; // dont need, just keeping for consistency

    // CF = result < source1, as that means that the result overflowed
    SSAInstruction* zext_result = ir_emit_zext(block, result, size_e);
    return ir_emit_less_than_unsigned(block, zext_result, source1);
}

SSAInstruction* ir_emit_get_carry_adc(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2, x86_size_e size_e) {
    SSAInstruction* carry_in = ir_emit_get_flag(block, X86_REF_CF);
    SSAInstruction* sum = ir_emit_add(block, source1, source2);
    SSAInstruction* sum_with_carry = ir_emit_add(block, sum, carry_in);

    SSAInstruction* carry1 = ir_emit_get_carry_add(block, source1, source2, sum, size_e);
    SSAInstruction* carry2 = ir_emit_get_carry_add(block, sum, carry_in, sum_with_carry, size_e);

    return ir_emit_or(block, carry1, carry2);
}

SSAInstruction* ir_emit_get_carry_sub(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2, SSAInstruction* result, x86_size_e size_e) {
    (void)result; // dont need, just keeping for consistency

    // CF = source1 < source2, as that means that the result would underflow
    return ir_emit_less_than_unsigned(block, source1, source2);
}

SSAInstruction* ir_emit_get_carry_sbb(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2, x86_size_e size_e) {
    SSAInstruction* carry_in = ir_emit_get_flag(block, X86_REF_CF);
    SSAInstruction* sum = ir_emit_sub(block, source1, source2);
    SSAInstruction* sum_with_carry = ir_emit_sub(block, sum, carry_in);

    SSAInstruction* carry1 = ir_emit_get_carry_sub(block, source1, source2, sum, size_e);
    SSAInstruction* carry2 = ir_emit_get_carry_sub(block, sum, carry_in, sum_with_carry, size_e);

    return ir_emit_or(block, carry1, carry2);
}

SSAInstruction* ir_emit_get_aux_add(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    SSAInstruction* mask = ir_emit_immediate(block, 0xF);
    SSAInstruction* and1 = ir_emit_and(block, source1, mask);
    SSAInstruction* and2 = ir_emit_and(block, source2, mask);
    SSAInstruction* result = ir_emit_add(block, and1, and2);

    return ir_emit_greater_than_unsigned(block, result, mask);
}

SSAInstruction* ir_emit_get_aux_sub(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2) {
    SSAInstruction* mask = ir_emit_immediate(block, 0xF);
    SSAInstruction* and1 = ir_emit_and(block, source1, mask);
    SSAInstruction* and2 = ir_emit_and(block, source2, mask);

    return ir_emit_less_than_unsigned(block, and1, and2);
}

SSAInstruction* ir_emit_set_cpazso(IRBlock* block, SSAInstruction* c, SSAInstruction* p, SSAInstruction* a, SSAInstruction* z, SSAInstruction* s,
                                   SSAInstruction* o) {
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

    return nullptr;
}

void ir_emit_group1_imm(IRBlock* block, x86_instruction_t* inst) {
    x86_group1_e opcode = (x86_group1_e)((inst->operand_reg.reg.ref & 0x7) - X86_REF_RAX);

    x86_size_e size_e = inst->operand_rm.size;
    SSAInstruction* rm = ir_emit_get_rm(block, &inst->operand_rm);
    SSAInstruction* imm = ir_emit_immediate_sext(block, &inst->operand_imm);
    SSAInstruction* result = nullptr;
    SSAInstruction* zero = ir_emit_immediate(block, 0);
    SSAInstruction* c = zero;
    SSAInstruction* o = zero;
    SSAInstruction* a = nullptr;

    switch (opcode) {
    case X86_GROUP1_ADD: {
        result = ir_emit_add(block, rm, imm);
        c = ir_emit_get_carry_add(block, rm, imm, result, size_e);
        o = ir_emit_get_overflow_add(block, rm, imm, result, size_e);
        a = ir_emit_get_aux_add(block, rm, imm);
        break;
    }
    case X86_GROUP1_ADC: {
        SSAInstruction* carry_in = ir_emit_get_flag(block, X86_REF_CF);
        SSAInstruction* imm_carry = ir_emit_add(block, imm, carry_in);
        result = ir_emit_add(block, rm, imm_carry);
        c = ir_emit_get_carry_adc(block, rm, imm_carry, size_e);
        o = ir_emit_get_overflow_add(block, rm, imm_carry, result, size_e);
        a = ir_emit_get_aux_add(block, rm, imm_carry);
        break;
    }
    case X86_GROUP1_SBB: {
        SSAInstruction* carry_in = ir_emit_get_flag(block, X86_REF_CF);
        SSAInstruction* imm_carry = ir_emit_add(block, imm, carry_in);
        result = ir_emit_sub(block, rm, imm_carry);
        c = ir_emit_get_carry_sbb(block, rm, imm_carry, size_e);
        o = ir_emit_get_overflow_sub(block, rm, imm_carry, result, size_e);
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
        c = ir_emit_get_carry_sub(block, rm, imm, result, size_e);
        o = ir_emit_get_overflow_sub(block, rm, imm, result, size_e);
        a = ir_emit_get_aux_sub(block, rm, imm);
        break;
    }
    case X86_GROUP1_XOR: {
        result = ir_emit_xor(block, rm, imm);
        break;
    }
    case X86_GROUP1_CMP: {
        result = ir_emit_sub(block, rm, imm);
        c = ir_emit_get_carry_sub(block, rm, imm, result, size_e);
        o = ir_emit_get_overflow_sub(block, rm, imm, result, size_e);
        a = ir_emit_get_aux_sub(block, rm, imm);
        break;
    }
    }

    SSAInstruction* p = ir_emit_get_parity(block, result);
    SSAInstruction* z = ir_emit_get_zero(block, result, size_e);
    SSAInstruction* s = ir_emit_get_sign(block, result, size_e);

    ir_emit_set_cpazso(block, c, p, a, z, s, o);

    if (opcode != X86_GROUP1_CMP) {
        ir_emit_set_rm(block, &inst->operand_rm, result);
    }
}

void ir_emit_group2(IRBlock* block, x86_instruction_t* inst, SSAInstruction* shift_amount) {
    x86_group2_e opcode = (x86_group2_e)((inst->operand_reg.reg.ref & 0x7) - X86_REF_RAX);

    x86_size_e size_e = inst->operand_rm.size;
    u8 shift_mask = size_e == X86_SIZE_QWORD ? 0x3F : 0x1F;
    SSAInstruction* rm = ir_emit_get_rm(block, &inst->operand_rm);
    SSAInstruction* shift_value = ir_emit_and(block, shift_amount, ir_emit_immediate(block, shift_mask));
    SSAInstruction* result = nullptr;
    SSAInstruction* c = nullptr;
    SSAInstruction* p = nullptr;
    SSAInstruction* a = nullptr;
    SSAInstruction* z = nullptr;
    SSAInstruction* s = nullptr;
    SSAInstruction* o = nullptr;

    switch (opcode) {
    case X86_GROUP2_ROL: {
        SSAInstruction* size = ir_emit_get_size(block, size_e);
        SSAInstruction* shift_mask = ir_emit_sub(block, size, ir_emit_immediate(block, 1));
        SSAInstruction* shift_masked = ir_emit_and(block, shift_value, shift_mask);
        result = ir_emit_rotate(block, rm, shift_masked, size_e, false);
        c = ir_emit_and(block, result, ir_emit_immediate(block, 1));
        SSAInstruction* msb = ir_emit_get_sign(block, result, size_e);
        o = ir_emit_xor(block, c, msb);
        break;
    }
    case X86_GROUP2_ROR: {
        SSAInstruction* size = ir_emit_get_size(block, size_e);
        SSAInstruction* shift_mask = ir_emit_sub(block, size, ir_emit_immediate(block, 1));
        SSAInstruction* shift_masked = ir_emit_and(block, shift_value, shift_mask);
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
        SSAInstruction* msb_mask = ir_emit_get_shift_mask_left(block, shift_value, size_e);
        result = ir_emit_shift_left(block, rm, shift_value);
        c = ir_emit_equal(block, ir_emit_and(block, rm, msb_mask), msb_mask);
        SSAInstruction* sign = ir_emit_get_sign(block, result, size_e);
        o = ir_emit_xor(block, c, sign);
        break;
    }
    case X86_GROUP2_SHR: {
        SSAInstruction* msb_mask = ir_emit_get_shift_mask_right(block, shift_value);
        result = ir_emit_shift_right(block, rm, shift_value);
        c = ir_emit_equal(block, ir_emit_and(block, rm, msb_mask), msb_mask);
        o = ir_emit_get_sign(block, rm, size_e);
        break;
    }
    case X86_GROUP2_SAR: {
        // Shift left to place MSB to bit 63
        SSAInstruction* shift_left_count = ir_emit_immediate(block, 64 - get_bit_size(size_e));
        SSAInstruction* shifted_left = ir_emit_shift_left(block, rm, shift_left_count);
        SSAInstruction* shift_right = ir_emit_add(block, shift_left_count, shift_value);
        result = ir_emit_shift_right_arithmetic(block, shifted_left, shift_right);
        o = ir_emit_immediate(block, 0);
        SSAInstruction* msb_mask = ir_emit_get_shift_mask_right(block, shift_value);
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
    x86_group3_e opcode = (x86_group3_e)((inst->operand_reg.reg.ref & 0x7) - X86_REF_RAX);

    x86_size_e size_e = inst->operand_rm.size;
    SSAInstruction* rm = ir_emit_get_rm(block, &inst->operand_rm);
    SSAInstruction* result = nullptr;
    SSAInstruction* c = nullptr;
    SSAInstruction* p = nullptr;
    SSAInstruction* a = nullptr;
    SSAInstruction* z = nullptr;
    SSAInstruction* s = nullptr;
    SSAInstruction* o = nullptr;

    switch (opcode) {
    case X86_GROUP3_TEST:
    case X86_GROUP3_TEST_: {
        SSAInstruction* imm = ir_emit_immediate_sext(block, &inst->operand_imm);
        SSAInstruction* masked = ir_emit_and(block, rm, imm);
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
        SSAInstruction* zero = ir_emit_immediate(block, 0);
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
            SSAInstruction* ax = get_reg(block, X86_REF_RAX, X86_SIZE_WORD);
            SSAInstruction* quotient = ir_emit_divuw(block, ax, rm);
            SSAInstruction* remainder = ir_emit_remuw(block, ax, rm);
            ir_emit_set_reg(block, X86_REF_RAX, X86_SIZE_BYTE, quotient);
            ir_emit_set_reg(block, X86_REF_RAX, X86_SIZE_BYTE, remainder, true);
            break;
        }
        case X86_SIZE_WORD: {
            // dx:ax / rm, ax := quotient, dx := remainder
            SSAInstruction* ax = get_reg(block, X86_REF_RAX, X86_SIZE_WORD);
            SSAInstruction* dx = get_reg(block, X86_REF_RDX, X86_SIZE_WORD);
            SSAInstruction* dx_shifted = ir_emit_shift_left(block, dx, ir_emit_immediate(block, 16));
            SSAInstruction* dx_ax = ir_emit_or(block, dx_shifted, ax);
            SSAInstruction* quotient = ir_emit_divuw(block, dx_ax, rm);
            SSAInstruction* remainder = ir_emit_remuw(block, dx_ax, rm);
            ir_emit_set_reg(block, X86_REF_RAX, X86_SIZE_WORD, quotient);
            ir_emit_set_reg(block, X86_REF_RDX, X86_SIZE_WORD, remainder);
            break;
        }
        case X86_SIZE_DWORD: {
            // edx:eax / rm, eax := quotient, edx := remainder
            SSAInstruction* eax = get_reg(block, X86_REF_RAX, X86_SIZE_DWORD);
            SSAInstruction* edx = get_reg(block, X86_REF_RDX, X86_SIZE_DWORD);
            SSAInstruction* edx_shifted = ir_emit_shift_left(block, edx, ir_emit_immediate(block, 32));
            SSAInstruction* edx_eax = ir_emit_or(block, edx_shifted, eax);
            SSAInstruction* quotient = ir_emit_divu(block, edx_eax, rm);
            SSAInstruction* remainder = ir_emit_remu(block, edx_eax, rm);
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
            SSAInstruction* ax = get_reg(block, X86_REF_RAX, X86_SIZE_WORD);
            SSAInstruction* quotient = ir_emit_divw(block, ax, rm);
            SSAInstruction* remainder = ir_emit_remw(block, ax, rm);
            ir_emit_set_reg(block, X86_REF_RAX, X86_SIZE_BYTE, quotient);
            ir_emit_set_reg(block, X86_REF_RAX, X86_SIZE_BYTE, remainder, true);
            break;
        }
        case X86_SIZE_WORD: {
            // dx:ax / rm, ax := quotient, dx := remainder
            SSAInstruction* ax = get_reg(block, X86_REF_RAX, X86_SIZE_WORD);
            SSAInstruction* dx = get_reg(block, X86_REF_RDX, X86_SIZE_WORD);
            SSAInstruction* dx_shifted = ir_emit_shift_left(block, dx, ir_emit_immediate(block, 16));
            SSAInstruction* dx_ax = ir_emit_or(block, dx_shifted, ax);
            SSAInstruction* quotient = ir_emit_divw(block, dx_ax, rm);
            SSAInstruction* remainder = ir_emit_remw(block, dx_ax, rm);
            ir_emit_set_reg(block, X86_REF_RAX, X86_SIZE_WORD, quotient);
            ir_emit_set_reg(block, X86_REF_RDX, X86_SIZE_WORD, remainder);
            break;
        }
        case X86_SIZE_DWORD: {
            // edx:eax / rm, eax := quotient, edx := remainder
            SSAInstruction* eax = get_reg(block, X86_REF_RAX, X86_SIZE_DWORD);
            SSAInstruction* edx = get_reg(block, X86_REF_RDX, X86_SIZE_DWORD);
            SSAInstruction* edx_shifted = ir_emit_shift_left(block, edx, ir_emit_immediate(block, 32));
            SSAInstruction* edx_eax = ir_emit_or(block, edx_shifted, eax);
            SSAInstruction* quotient = ir_emit_div(block, edx_eax, rm);
            SSAInstruction* remainder = ir_emit_rem(block, edx_eax, rm);
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

SSAInstruction* ir_emit_get_cc(IRBlock* block, u8 opcode) {
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
    return nullptr;
}

void ir_emit_setcc(IRBlock* block, x86_instruction_t* inst) {
    ir_emit_set_rm(block, &inst->operand_rm, ir_emit_get_cc(block, inst->opcode));
}

void ir_emit_set_exit_reason(IRBlock* block, u8 reason) {
    SSAInstruction* set_exit_reason = ir_emit_no_operands(block, IROpcode::SetExitReason);
    set_exit_reason->SetImmediateData(reason);
    set_exit_reason->Lock();
}

void ir_emit_rep_start(FrontendState* state, const x86_instruction_t& inst, IRBlock* loop_block, IRBlock* exit_block) {
    x86_operand_t rcx_reg = get_full_reg(X86_REF_RCX);
    rcx_reg.size = inst.operand_reg.size;
    SSAInstruction* rcx = ir_emit_get_reg(state->current_block, &rcx_reg);
    SSAInstruction* zero = ir_emit_immediate(state->current_block, 0);
    SSAInstruction* condition = ir_emit_equal(state->current_block, rcx, zero);
    state->current_block->TerminateJumpConditional(condition, exit_block, loop_block);

    // Write the instruction in the loop body
    state->current_block = loop_block;
}

void ir_emit_rep_end(FrontendState* state, const x86_instruction_t& inst, x86_rep_e rep_type, IRBlock* loop_block, IRBlock* exit_block) {
    x86_operand_t rcx_reg = get_full_reg(X86_REF_RCX);
    rcx_reg.size = inst.operand_reg.size;
    SSAInstruction* rcx = ir_emit_get_reg(state->current_block, &rcx_reg);
    SSAInstruction* zero = ir_emit_immediate(state->current_block, 0);
    SSAInstruction* one = ir_emit_immediate(state->current_block, 1);
    SSAInstruction* sub = ir_emit_sub(state->current_block, rcx, one);
    ir_emit_set_reg(state->current_block, &rcx_reg, sub);
    SSAInstruction* rcx_zero = ir_emit_equal(state->current_block, sub, zero);
    SSAInstruction* condition;
    SSAInstruction* zf = ir_emit_get_flag(state->current_block, X86_REF_ZF);
    if (rep_type == REP) { // Some instructions don't check the ZF flag
        condition = zero;
    } else if (rep_type == REP_NZ) {
        condition = ir_emit_not_equal(state->current_block, zf, zero);
    } else if (rep_type == REP_Z) {
        condition = ir_emit_equal(state->current_block, zf, zero);
    } else {
        UNREACHABLE();
        return;
    }

    SSAInstruction* final_condition = ir_emit_or(state->current_block, rcx_zero, condition);
    state->current_block->TerminateJumpConditional(final_condition, exit_block, loop_block);

    frontend_compile_block(*state->emulator, state->function, exit_block);
    state->exit = true;
}

void ir_emit_call_host_function(IRBlock* block, u64 function) {
    SSAInstruction* call = ir_emit_no_operands(block, IROpcode::CallHostFunction);
    call->SetImmediateData(function);
    call->Lock();
}