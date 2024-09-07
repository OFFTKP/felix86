#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "felix86/common/utility.h"
#include "felix86/frontend/instruction.h"
#include "felix86/ir/block.h"
#include "felix86/ir/instruction.h"

u16 get_bit_size(x86_size_e size);
x86_operand_t get_full_reg(x86_ref_e ref);

void ir_emit_hint_inputs(ir_instruction_list_t* instructions, x86_ref_e* refs, u8 count);
void ir_emit_hint_outputs(ir_instruction_list_t* instructions, x86_ref_e* refs, u8 count);

ir_instruction_t* ir_emit_add(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2);
ir_instruction_t* ir_emit_sub(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2);
ir_instruction_t* ir_emit_shift_left(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2);
ir_instruction_t* ir_emit_shift_right(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2);
ir_instruction_t* ir_emit_shift_right_arithmetic(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2);
ir_instruction_t* ir_emit_rotate(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2, x86_size_e size, bool right);
ir_instruction_t* ir_emit_select(ir_instruction_list_t* instructions, ir_instruction_t* condition, ir_instruction_t* source1, ir_instruction_t* source2);
ir_instruction_t* ir_emit_udiv(ir_instruction_list_t* instructions, ir_instruction_t* source);
ir_instruction_t* ir_emit_and(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2);
ir_instruction_t* ir_emit_or(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2);
ir_instruction_t* ir_emit_xor(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2);
ir_instruction_t* ir_emit_not(ir_instruction_list_t* instructions, ir_instruction_t* source);
ir_instruction_t* ir_emit_popcount(ir_instruction_list_t* instructions, ir_instruction_t* source);
ir_instruction_t* ir_emit_equal(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2);
ir_instruction_t* ir_emit_not_equal(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2);
ir_instruction_t* ir_emit_greater_than_signed(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2);
ir_instruction_t* ir_emit_less_than_signed(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2);
ir_instruction_t* ir_emit_greater_than_unsigned(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2);
ir_instruction_t* ir_emit_less_than_unsigned(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2);
ir_instruction_t* ir_emit_lea(ir_instruction_list_t* instructions, x86_operand_t* rm_operand);
ir_instruction_t* ir_emit_sext8(ir_instruction_list_t* instructions, ir_instruction_t* source);
ir_instruction_t* ir_emit_sext16(ir_instruction_list_t* instructions, ir_instruction_t* source);
ir_instruction_t* ir_emit_sext32(ir_instruction_list_t* instructions, ir_instruction_t* source);
ir_instruction_t* ir_emit_syscall(ir_instruction_list_t* instructions);
ir_instruction_t* ir_emit_exit(ir_instruction_list_t* instructions);
ir_instruction_t* ir_emit_jump(ir_instruction_list_t* instructions, ir_block_t* block);
ir_instruction_t* ir_emit_jump_conditional(
	ir_instruction_list_t* instructions, ir_instruction_t* condition, ir_block_t* target_true, ir_block_t* target_false
);
ir_instruction_t* ir_emit_jump_register(ir_instruction_list_t* instructions, ir_instruction_t* target);
ir_instruction_t* ir_emit_insert_integer_to_vector(
	ir_instruction_list_t* instructions, ir_instruction_t* dst, ir_instruction_t* source, u8 idx, x86_size_e sz
);
ir_instruction_t* ir_emit_extract_integer_from_vector(ir_instruction_list_t* instructions, ir_instruction_t* src, u8 idx, x86_size_e sz);
ir_instruction_t* ir_emit_vector_unpack_dword_low(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2);
ir_instruction_t* ir_emit_vector_from_integer(ir_instruction_list_t* instructions, ir_instruction_t* source);
ir_instruction_t* ir_emit_integer_from_vector(ir_instruction_list_t* instructions, ir_instruction_t* source);
ir_instruction_t* ir_emit_vector_packed_and(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2);

ir_instruction_t* ir_emit_get_guest(ir_instruction_list_t* instructions, x86_ref_e ref);
void ir_emit_set_guest(ir_instruction_list_t* instructions, x86_ref_e ref, ir_instruction_t* source);
ir_instruction_t* ir_emit_get_flag(ir_instruction_list_t* instructions, x86_ref_e flag);
void ir_emit_set_flag(ir_instruction_list_t* instructions, x86_ref_e flag, ir_instruction_t* source);
ir_instruction_t* ir_emit_get_flag_not(ir_instruction_list_t* instructions, x86_ref_e flag);

ir_instruction_t* ir_emit_read_byte(ir_instruction_list_t* instructions, ir_instruction_t* address);
ir_instruction_t* ir_emit_read_word(ir_instruction_list_t* instructions, ir_instruction_t* address);
ir_instruction_t* ir_emit_read_dword(ir_instruction_list_t* instructions, ir_instruction_t* address);
ir_instruction_t* ir_emit_read_qword(ir_instruction_list_t* instructions, ir_instruction_t* address);
ir_instruction_t* ir_emit_read_xmmword(ir_instruction_list_t* instructions, ir_instruction_t* address);
void ir_emit_write_byte(ir_instruction_list_t* instructions, ir_instruction_t* address, ir_instruction_t* source);
void ir_emit_write_word(ir_instruction_list_t* instructions, ir_instruction_t* address, ir_instruction_t* source);
void ir_emit_write_dword(ir_instruction_list_t* instructions, ir_instruction_t* address, ir_instruction_t* source);
void ir_emit_write_qword(ir_instruction_list_t* instructions, ir_instruction_t* address, ir_instruction_t* source);

ir_instruction_t* ir_emit_cpuid(ir_instruction_list_t* instructions);

void ir_emit_setcc(ir_instruction_list_t* instructions, x86_instruction_t* inst);

// Helpers
ir_instruction_t* ir_emit_immediate(ir_instruction_list_t* instructions, u64 value);
ir_instruction_t* ir_emit_immediate_sext(ir_instruction_list_t* instructions, x86_operand_t* operand);

ir_instruction_t* ir_emit_get_reg(ir_instruction_list_t* instructions, x86_operand_t* reg_operand);
ir_instruction_t* ir_emit_get_rm(ir_instruction_list_t* instructions, x86_operand_t* rm_operand);
void ir_emit_set_reg(ir_instruction_list_t* instructions, x86_operand_t* reg_operand, ir_instruction_t* source);
void ir_emit_set_rm(ir_instruction_list_t* instructions, x86_operand_t* rm_operand, ir_instruction_t* source);

void ir_emit_write_memory(ir_instruction_list_t* instructions, ir_instruction_t* address, ir_instruction_t* value, x86_size_e size);
ir_instruction_t* ir_emit_read_memory(ir_instruction_list_t* instructions, ir_instruction_t* address, x86_size_e size);

ir_instruction_t* ir_emit_get_gpr8_low(ir_instruction_list_t* instructions, x86_ref_e reg);
ir_instruction_t* ir_emit_get_gpr8_high(ir_instruction_list_t* instructions, x86_ref_e reg);
ir_instruction_t* ir_emit_get_gpr16(ir_instruction_list_t* instructions, x86_ref_e reg);
ir_instruction_t* ir_emit_get_gpr32(ir_instruction_list_t* instructions, x86_ref_e reg);
ir_instruction_t* ir_emit_get_gpr64(ir_instruction_list_t* instructions, x86_ref_e reg);
ir_instruction_t* ir_emit_get_vector(ir_instruction_list_t* instructions, x86_ref_e reg);
void ir_emit_set_gpr8_low(ir_instruction_list_t* instructions, x86_ref_e reg, ir_instruction_t* source);
void ir_emit_set_gpr8_high(ir_instruction_list_t* instructions, x86_ref_e reg, ir_instruction_t* source);
void ir_emit_set_gpr16(ir_instruction_list_t* instructions, x86_ref_e reg, ir_instruction_t* source);
void ir_emit_set_gpr32(ir_instruction_list_t* instructions, x86_ref_e reg, ir_instruction_t* source);
void ir_emit_set_gpr64(ir_instruction_list_t* instructions, x86_ref_e reg, ir_instruction_t* source);
void ir_emit_set_vector(ir_instruction_list_t* instructions, x86_ref_e reg, ir_instruction_t* source);

ir_instruction_t* ir_emit_get_parity(ir_instruction_list_t* instructions, ir_instruction_t* source);
ir_instruction_t* ir_emit_get_zero(ir_instruction_list_t* sta32te, ir_instruction_t* source);
ir_instruction_t* ir_emit_get_sign(ir_instruction_list_t* instructions, ir_instruction_t* source, x86_size_e size);
ir_instruction_t* ir_emit_get_overflow_add(
	ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2, ir_instruction_t* result, x86_size_e size
);
ir_instruction_t* ir_emit_get_overflow_sub(
	ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2, ir_instruction_t* result, x86_size_e size
);
ir_instruction_t* ir_emit_get_carry_add(
	ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2, ir_instruction_t* result, x86_size_e size
);
ir_instruction_t* ir_emit_get_carry_adc(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2, x86_size_e size);
ir_instruction_t* ir_emit_get_carry_sub(
	ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2, ir_instruction_t* result, x86_size_e size
);
ir_instruction_t* ir_emit_get_carry_sbb(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2, x86_size_e size_e);
ir_instruction_t* ir_emit_get_aux_add(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2);
ir_instruction_t* ir_emit_get_aux_sub(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2);

ir_instruction_t* ir_emit_set_cpazso(
	ir_instruction_list_t* instructions, ir_instruction_t* c, ir_instruction_t* p, ir_instruction_t* a, ir_instruction_t* z, ir_instruction_t* s,
	ir_instruction_t* o
);

ir_instruction_t* ir_emit_debug_info_compile_time(ir_instruction_list_t* instructions, const char* format, ...);

ir_instruction_t* ir_emit_get_cc(ir_instruction_list_t* instructions, u8 opcode);

void ir_emit_group1_imm(ir_instruction_list_t* instructions, x86_instruction_t* inst);
void ir_emit_group2_imm(ir_instruction_list_t* instructions, x86_instruction_t* inst);
void ir_emit_group3(ir_instruction_list_t* instructions, x86_instruction_t* inst);

void ir_emit_rep_start(ir_instruction_list_t* instructions, x86_size_e size);
void ir_emit_rep_end(ir_instruction_list_t* instructions, bool is_nz, x86_size_e size);

#ifdef __cplusplus
}
#endif