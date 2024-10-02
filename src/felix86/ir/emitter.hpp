#pragma once

#include "felix86/common/utility.hpp"
#include "felix86/frontend/instruction.hpp"
#include "felix86/ir/block.hpp"
#include "felix86/ir/instruction.hpp"

u16 get_bit_size(x86_size_e size);
x86_operand_t get_full_reg(x86_ref_e ref);

void ir_emit_runtime_comment(IRBlock* block, const std::string& comment);

IRInstruction* ir_emit_add(IRBlock* block, IRInstruction* source1, IRInstruction* source2);
IRInstruction* ir_emit_sub(IRBlock* block, IRInstruction* source1, IRInstruction* source2);
IRInstruction* ir_emit_shift_left(IRBlock* block, IRInstruction* source1, IRInstruction* source2);
IRInstruction* ir_emit_shift_right(IRBlock* block, IRInstruction* source1, IRInstruction* source2);
IRInstruction* ir_emit_shift_right_arithmetic(IRBlock* block, IRInstruction* source1, IRInstruction* source2);
IRInstruction* ir_emit_rotate(IRBlock* block, IRInstruction* source1, IRInstruction* source2, x86_size_e size, bool right);
IRInstruction* ir_emit_select(IRBlock* block, IRInstruction* condition, IRInstruction* true_value, IRInstruction* false_value);
IRInstruction* ir_emit_imul(IRBlock* block, IRInstruction* source1, IRInstruction* source2);
IRInstruction* ir_emit_idiv(IRBlock* block, x86_size_e opcode, IRInstruction* rdx, IRInstruction* rax, IRInstruction* divisor);
IRInstruction* ir_emit_udiv(IRBlock* block, x86_size_e opcode, IRInstruction* rdx, IRInstruction* rax, IRInstruction* divisor);
IRInstruction* ir_emit_clz(IRBlock* block, IRInstruction* source);
IRInstruction* ir_emit_ctz(IRBlock* block, IRInstruction* source);
IRInstruction* ir_emit_and(IRBlock* block, IRInstruction* source1, IRInstruction* source2);
IRInstruction* ir_emit_or(IRBlock* block, IRInstruction* source1, IRInstruction* source2);
IRInstruction* ir_emit_xor(IRBlock* block, IRInstruction* source1, IRInstruction* source2);
IRInstruction* ir_emit_not(IRBlock* block, IRInstruction* source);
IRInstruction* ir_emit_popcount(IRBlock* block, IRInstruction* source);
IRInstruction* ir_emit_equal(IRBlock* block, IRInstruction* source1, IRInstruction* source2);
IRInstruction* ir_emit_not_equal(IRBlock* block, IRInstruction* source1, IRInstruction* source2);
IRInstruction* ir_emit_greater_than_signed(IRBlock* block, IRInstruction* source1, IRInstruction* source2);
IRInstruction* ir_emit_less_than_signed(IRBlock* block, IRInstruction* source1, IRInstruction* source2);
IRInstruction* ir_emit_greater_than_unsigned(IRBlock* block, IRInstruction* source1, IRInstruction* source2);
IRInstruction* ir_emit_less_than_unsigned(IRBlock* block, IRInstruction* source1, IRInstruction* source2);
IRInstruction* ir_emit_lea(IRBlock* block, x86_operand_t* rm_operand);
IRInstruction* ir_emit_sext(IRBlock* block, IRInstruction* source, x86_size_e size);
IRInstruction* ir_emit_sext8(IRBlock* block, IRInstruction* source);
IRInstruction* ir_emit_sext16(IRBlock* block, IRInstruction* source);
IRInstruction* ir_emit_sext32(IRBlock* block, IRInstruction* source);
IRInstruction* ir_emit_syscall(IRBlock* block, std::initializer_list<IRInstruction*> args);
IRInstruction* ir_emit_insert_integer_to_vector(IRBlock* block, IRInstruction* source, IRInstruction* dest, u8 idx, x86_size_e sz);
IRInstruction* ir_emit_extract_integer_from_vector(IRBlock* block, IRInstruction* src, u8 idx, x86_size_e sz);
IRInstruction* ir_emit_vector_unpack_byte_low(IRBlock* block, IRInstruction* source1, IRInstruction* source2);
IRInstruction* ir_emit_vector_unpack_word_low(IRBlock* block, IRInstruction* source1, IRInstruction* source2);
IRInstruction* ir_emit_vector_unpack_dword_low(IRBlock* block, IRInstruction* source1, IRInstruction* source2);
IRInstruction* ir_emit_vector_unpack_qword_low(IRBlock* block, IRInstruction* source1, IRInstruction* source2);
IRInstruction* ir_emit_vector_from_integer(IRBlock* block, IRInstruction* source);
IRInstruction* ir_emit_integer_from_vector(IRBlock* block, IRInstruction* source);
IRInstruction* ir_emit_vector_packed_and(IRBlock* block, IRInstruction* source1, IRInstruction* source2);
IRInstruction* ir_emit_vector_packed_or(IRBlock* block, IRInstruction* source1, IRInstruction* source2);
IRInstruction* ir_emit_vector_packed_xor(IRBlock* block, IRInstruction* source1, IRInstruction* source2);
IRInstruction* ir_emit_vector_packed_shift_right(IRBlock* block, IRInstruction* source, IRInstruction* imm);
IRInstruction* ir_emit_vector_packed_shift_left(IRBlock* block, IRInstruction* source, IRInstruction* imm);
IRInstruction* ir_emit_vector_packed_sub_byte(IRBlock* block, IRInstruction* source1, IRInstruction* source2);
IRInstruction* ir_emit_vector_packed_add_qword(IRBlock* block, IRInstruction* source1, IRInstruction* source2);
IRInstruction* ir_emit_vector_packed_compare_eq_byte(IRBlock* block, IRInstruction* source1, IRInstruction* source2);
IRInstruction* ir_emit_vector_packed_compare_eq_word(IRBlock* block, IRInstruction* source1, IRInstruction* source2);
IRInstruction* ir_emit_vector_packed_compare_eq_dword(IRBlock* block, IRInstruction* source1, IRInstruction* source2);
IRInstruction* ir_emit_vector_packed_shuffle_dword(IRBlock* block, IRInstruction* source, u8 control_byte);
IRInstruction* ir_emit_vector_packed_move_byte_mask(IRBlock* block, IRInstruction* source);
IRInstruction* ir_emit_vector_packed_min_byte(IRBlock* block, IRInstruction* source1, IRInstruction* source2);
IRInstruction* ir_emit_vector_packed_compare_implicit_string_index(IRBlock* block, IRInstruction* source1, IRInstruction* source2);

IRInstruction* ir_emit_load_guest_from_memory(IRBlock* block, x86_ref_e ref);
void ir_emit_store_guest_to_memory(IRBlock* block, x86_ref_e ref, IRInstruction* source);
IRInstruction* ir_emit_get_guest(IRBlock* block, x86_ref_e ref);
void ir_emit_set_guest(IRBlock* block, x86_ref_e ref, IRInstruction* source);
IRInstruction* ir_emit_get_flag(IRBlock* block, x86_ref_e flag);
void ir_emit_set_flag(IRBlock* block, x86_ref_e flag, IRInstruction* source);
IRInstruction* ir_emit_get_flag_not(IRBlock* block, x86_ref_e flag);

IRInstruction* ir_emit_read_byte(IRBlock* block, IRInstruction* address);
IRInstruction* ir_emit_read_word(IRBlock* block, IRInstruction* address);
IRInstruction* ir_emit_read_dword(IRBlock* block, IRInstruction* address);
IRInstruction* ir_emit_read_qword(IRBlock* block, IRInstruction* address);
IRInstruction* ir_emit_read_xmmword(IRBlock* block, IRInstruction* address);
void ir_emit_write_byte(IRBlock* block, IRInstruction* address, IRInstruction* source);
void ir_emit_write_word(IRBlock* block, IRInstruction* address, IRInstruction* source);
void ir_emit_write_dword(IRBlock* block, IRInstruction* address, IRInstruction* source);
void ir_emit_write_qword(IRBlock* block, IRInstruction* address, IRInstruction* source);
void ir_emit_write_xmmword(IRBlock* block, IRInstruction* address, IRInstruction* source);

void ir_emit_setcc(IRBlock* block, x86_instruction_t* inst);

IRInstruction* ir_emit_cpuid(IRBlock* block, IRInstruction* rax, IRInstruction* rcx);
IRInstruction* ir_emit_rdtsc(IRBlock* block);
IRInstruction* ir_emit_tuple_extract(IRBlock* block, IRInstruction* instruction, u8 index);

// Helpers
IRInstruction* ir_emit_immediate(IRBlock* block, u64 value);
IRInstruction* ir_emit_immediate_sext(IRBlock* block, x86_operand_t* operand);

IRInstruction* ir_emit_get_reg(IRBlock* block, x86_operand_t* reg_operand);
IRInstruction* ir_emit_get_rm(IRBlock* block, x86_operand_t* rm_operand);
void ir_emit_set_reg(IRBlock* block, x86_operand_t* reg_operand, IRInstruction* source);
void ir_emit_set_rm(IRBlock* block, x86_operand_t* rm_operand, IRInstruction* source);

void ir_emit_write_memory(IRBlock* block, IRInstruction* address, IRInstruction* value, x86_size_e size);
IRInstruction* ir_emit_read_memory(IRBlock* block, IRInstruction* address, x86_size_e size);

IRInstruction* ir_emit_get_gpr8_low(IRBlock* block, x86_ref_e reg);
IRInstruction* ir_emit_get_gpr8_high(IRBlock* block, x86_ref_e reg);
IRInstruction* ir_emit_get_gpr16(IRBlock* block, x86_ref_e reg);
IRInstruction* ir_emit_get_gpr32(IRBlock* block, x86_ref_e reg);
IRInstruction* ir_emit_get_gpr64(IRBlock* block, x86_ref_e reg);
IRInstruction* ir_emit_get_vector(IRBlock* block, x86_ref_e reg);
void ir_emit_set_gpr8_low(IRBlock* block, x86_ref_e reg, IRInstruction* source);
void ir_emit_set_gpr8_high(IRBlock* block, x86_ref_e reg, IRInstruction* source);
void ir_emit_set_gpr16(IRBlock* block, x86_ref_e reg, IRInstruction* source);
void ir_emit_set_gpr32(IRBlock* block, x86_ref_e reg, IRInstruction* source);
void ir_emit_set_gpr64(IRBlock* block, x86_ref_e reg, IRInstruction* source);
void ir_emit_set_vector(IRBlock* block, x86_ref_e reg, IRInstruction* source);

IRInstruction* ir_emit_get_parity(IRBlock* block, IRInstruction* source);
IRInstruction* ir_emit_get_zero(IRBlock* sta32te, IRInstruction* source, x86_size_e size);
IRInstruction* ir_emit_get_sign(IRBlock* block, IRInstruction* source, x86_size_e size);
IRInstruction* ir_emit_get_overflow_add(IRBlock* block, IRInstruction* source1, IRInstruction* source2, IRInstruction* result, x86_size_e size);
IRInstruction* ir_emit_get_overflow_sub(IRBlock* block, IRInstruction* source1, IRInstruction* source2, IRInstruction* result, x86_size_e size);
IRInstruction* ir_emit_get_carry_add(IRBlock* block, IRInstruction* source1, IRInstruction* source2, IRInstruction* result, x86_size_e size);
IRInstruction* ir_emit_get_carry_adc(IRBlock* block, IRInstruction* source1, IRInstruction* source2, x86_size_e size);
IRInstruction* ir_emit_get_carry_sub(IRBlock* block, IRInstruction* source1, IRInstruction* source2, IRInstruction* result, x86_size_e size);
IRInstruction* ir_emit_get_carry_sbb(IRBlock* block, IRInstruction* source1, IRInstruction* source2, x86_size_e size_e);
IRInstruction* ir_emit_get_aux_add(IRBlock* block, IRInstruction* source1, IRInstruction* source2);
IRInstruction* ir_emit_get_aux_sub(IRBlock* block, IRInstruction* source1, IRInstruction* source2);

IRInstruction* ir_emit_set_cpazso(IRBlock* block, IRInstruction* c, IRInstruction* p, IRInstruction* a, IRInstruction* z, IRInstruction* s,
                                  IRInstruction* o);

IRInstruction* ir_emit_get_cc(IRBlock* block, u8 opcode);

void ir_emit_group1_imm(IRBlock* block, x86_instruction_t* inst);
void ir_emit_group2(IRBlock* block, x86_instruction_t* inst, IRInstruction* shift_amount);
void ir_emit_group3(IRBlock* block, x86_instruction_t* inst);

void ir_emit_rep_start(IRBlock* block, x86_size_e size);
void ir_emit_rep_end(IRBlock* block, bool is_nz, x86_size_e size);
