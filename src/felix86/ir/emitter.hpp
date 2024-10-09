#pragma once

#include "felix86/common/utility.hpp"
#include "felix86/frontend/instruction.hpp"
#include "felix86/ir/block.hpp"
#include "felix86/ir/instruction.hpp"

u16 get_bit_size(x86_size_e size);
x86_operand_t get_full_reg(x86_ref_e ref);

void ir_emit_runtime_comment(IRBlock* block, const std::string& comment);

SSAInstruction* ir_emit_add(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_addi(IRBlock* block, SSAInstruction* source, i64 imm);
SSAInstruction* ir_emit_sub(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_shift_left(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_shift_right(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_shift_right_arithmetic(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_rotate(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2, x86_size_e size, bool right);
SSAInstruction* ir_emit_select(IRBlock* block, SSAInstruction* condition, SSAInstruction* true_value, SSAInstruction* false_value);
SSAInstruction* ir_emit_clz(IRBlock* block, SSAInstruction* source);
SSAInstruction* ir_emit_ctzh(IRBlock* block, SSAInstruction* source);
SSAInstruction* ir_emit_ctzw(IRBlock* block, SSAInstruction* source);
SSAInstruction* ir_emit_ctz(IRBlock* block, SSAInstruction* source);
SSAInstruction* ir_emit_and(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_or(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_xor(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_not(IRBlock* block, SSAInstruction* source);
SSAInstruction* ir_emit_get_parity(IRBlock* block, SSAInstruction* source);
SSAInstruction* ir_emit_equal(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_not_equal(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_greater_than_signed(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_less_than_signed(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_greater_than_unsigned(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_less_than_unsigned(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_lea(IRBlock* block, x86_operand_t* rm_operand);
SSAInstruction* ir_emit_sext(IRBlock* block, SSAInstruction* source, x86_size_e size);
SSAInstruction* ir_emit_sext8(IRBlock* block, SSAInstruction* source);
SSAInstruction* ir_emit_sext16(IRBlock* block, SSAInstruction* source);
SSAInstruction* ir_emit_sext32(IRBlock* block, SSAInstruction* source);
SSAInstruction* ir_emit_div(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_divu(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_rem(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_remu(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_divw(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_divuw(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_remw(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_remuw(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_mul(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_mulh(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_mulhu(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
void ir_emit_syscall(IRBlock* block);
SSAInstruction* ir_emit_insert_integer_to_vector(IRBlock* block, SSAInstruction* source, SSAInstruction* dest, u8 idx, x86_size_e sz);
SSAInstruction* ir_emit_extract_integer_from_vector(IRBlock* block, SSAInstruction* src, u8 idx, x86_size_e sz);
SSAInstruction* ir_emit_vector_unpack_byte_low(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_vector_unpack_word_low(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_vector_unpack_dword_low(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_vector_unpack_qword_low(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_cast_vector_integer(IRBlock* block, SSAInstruction* source);
SSAInstruction* ir_emit_cast_integer_vector(IRBlock* block, SSAInstruction* source);
SSAInstruction* ir_emit_cast_vector_float(IRBlock* block, SSAInstruction* source);
SSAInstruction* ir_emit_cast_float_vector(IRBlock* block, SSAInstruction* source);
SSAInstruction* ir_emit_vector_packed_and(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_vector_packed_or(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_vector_packed_xor(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_vector_packed_shift_right(IRBlock* block, SSAInstruction* source, SSAInstruction* imm);
SSAInstruction* ir_emit_vector_packed_shift_left(IRBlock* block, SSAInstruction* source, SSAInstruction* imm);
SSAInstruction* ir_emit_vector_packed_sub_byte(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_vector_packed_add_qword(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_vector_packed_compare_eq_byte(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_vector_packed_compare_eq_word(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_vector_packed_compare_eq_dword(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_vector_packed_shuffle_dword(IRBlock* block, SSAInstruction* source, u8 control_byte);
SSAInstruction* ir_emit_vector_packed_move_byte_mask(IRBlock* block, SSAInstruction* source);
SSAInstruction* ir_emit_vector_packed_min_byte(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_vector_packed_compare_implicit_string_index(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);

SSAInstruction* ir_emit_load_guest_from_memory(IRBlock* block, x86_ref_e ref);
void ir_emit_store_guest_to_memory(IRBlock* block, x86_ref_e ref, SSAInstruction* source);
SSAInstruction* ir_emit_get_guest(IRBlock* block, x86_ref_e ref);
void ir_emit_set_guest(IRBlock* block, x86_ref_e ref, SSAInstruction* source);
SSAInstruction* ir_emit_get_flag(IRBlock* block, x86_ref_e flag);
void ir_emit_set_flag(IRBlock* block, x86_ref_e flag, SSAInstruction* source);
SSAInstruction* ir_emit_get_flag_not(IRBlock* block, x86_ref_e flag);

SSAInstruction* ir_emit_read_byte(IRBlock* block, SSAInstruction* address);
SSAInstruction* ir_emit_read_word(IRBlock* block, SSAInstruction* address);
SSAInstruction* ir_emit_read_dword(IRBlock* block, SSAInstruction* address);
SSAInstruction* ir_emit_read_qword(IRBlock* block, SSAInstruction* address);
SSAInstruction* ir_emit_read_xmmword(IRBlock* block, SSAInstruction* address);
void ir_emit_write_byte(IRBlock* block, SSAInstruction* address, SSAInstruction* source);
void ir_emit_write_word(IRBlock* block, SSAInstruction* address, SSAInstruction* source);
void ir_emit_write_dword(IRBlock* block, SSAInstruction* address, SSAInstruction* source);
void ir_emit_write_qword(IRBlock* block, SSAInstruction* address, SSAInstruction* source);
void ir_emit_write_xmmword(IRBlock* block, SSAInstruction* address, SSAInstruction* source);

void ir_emit_setcc(IRBlock* block, x86_instruction_t* inst);

void ir_emit_cpuid(IRBlock* block);
void ir_emit_rdtsc(IRBlock* block);

// Helpers
SSAInstruction* ir_emit_immediate(IRBlock* block, u64 value);
SSAInstruction* ir_emit_immediate_sext(IRBlock* block, x86_operand_t* operand);

SSAInstruction* ir_emit_get_reg(IRBlock* block, x86_operand_t* reg_operand);
SSAInstruction* ir_emit_get_rm(IRBlock* block, x86_operand_t* rm_operand);
void ir_emit_set_reg(IRBlock* block, x86_operand_t* reg_operand, SSAInstruction* source);
void ir_emit_set_rm(IRBlock* block, x86_operand_t* rm_operand, SSAInstruction* source);

void ir_emit_write_memory(IRBlock* block, SSAInstruction* address, SSAInstruction* value, x86_size_e size);
SSAInstruction* ir_emit_read_memory(IRBlock* block, SSAInstruction* address, x86_size_e size);

SSAInstruction* ir_emit_get_gpr8_low(IRBlock* block, x86_ref_e reg);
SSAInstruction* ir_emit_get_gpr8_high(IRBlock* block, x86_ref_e reg);
SSAInstruction* ir_emit_get_gpr16(IRBlock* block, x86_ref_e reg);
SSAInstruction* ir_emit_get_gpr32(IRBlock* block, x86_ref_e reg);
SSAInstruction* ir_emit_get_gpr64(IRBlock* block, x86_ref_e reg);
SSAInstruction* ir_emit_get_vector(IRBlock* block, x86_ref_e reg);
void ir_emit_set_gpr8_low(IRBlock* block, x86_ref_e reg, SSAInstruction* source);
void ir_emit_set_gpr8_high(IRBlock* block, x86_ref_e reg, SSAInstruction* source);
void ir_emit_set_gpr16(IRBlock* block, x86_ref_e reg, SSAInstruction* source);
void ir_emit_set_gpr32(IRBlock* block, x86_ref_e reg, SSAInstruction* source);
void ir_emit_set_gpr64(IRBlock* block, x86_ref_e reg, SSAInstruction* source);
void ir_emit_set_vector(IRBlock* block, x86_ref_e reg, SSAInstruction* source);

SSAInstruction* ir_emit_get_parity(IRBlock* block, SSAInstruction* source);
SSAInstruction* ir_emit_get_zero(IRBlock* sta32te, SSAInstruction* source, x86_size_e size);
SSAInstruction* ir_emit_get_sign(IRBlock* block, SSAInstruction* source, x86_size_e size);
SSAInstruction* ir_emit_get_overflow_add(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2, SSAInstruction* result, x86_size_e size);
SSAInstruction* ir_emit_get_overflow_sub(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2, SSAInstruction* result, x86_size_e size);
SSAInstruction* ir_emit_get_carry_add(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2, SSAInstruction* result, x86_size_e size);
SSAInstruction* ir_emit_get_carry_adc(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2, x86_size_e size);
SSAInstruction* ir_emit_get_carry_sub(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2, SSAInstruction* result, x86_size_e size);
SSAInstruction* ir_emit_get_carry_sbb(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2, x86_size_e size_e);
SSAInstruction* ir_emit_get_aux_add(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_get_aux_sub(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);

SSAInstruction* ir_emit_set_cpazso(IRBlock* block, SSAInstruction* c, SSAInstruction* p, SSAInstruction* a, SSAInstruction* z, SSAInstruction* s,
                                   SSAInstruction* o);

SSAInstruction* ir_emit_get_cc(IRBlock* block, u8 opcode);

void ir_emit_group1_imm(IRBlock* block, x86_instruction_t* inst);
void ir_emit_group2(IRBlock* block, x86_instruction_t* inst, SSAInstruction* shift_amount);
void ir_emit_group3(IRBlock* block, x86_instruction_t* inst);

void ir_emit_rep_start(IRBlock* block, x86_size_e size);
void ir_emit_rep_end(IRBlock* block, bool is_nz, x86_size_e size);
