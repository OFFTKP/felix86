#pragma once

#include "felix86/common/utility.hpp"
#include "felix86/frontend/frontend.hpp"
#include "felix86/frontend/instruction.hpp"
#include "felix86/ir/block.hpp"
#include "felix86/ir/instruction.hpp"

u16 get_bit_size(x86_size_e size);
x86_operand_t get_full_reg(x86_ref_e ref);
SSAInstruction* get_reg(IRBlock* block, x86_ref_e ref, x86_size_e size_e);

void ir_emit_runtime_comment(IRBlock* block, const std::string& comment);

SSAInstruction* ir_emit_get_thread_state_pointer(IRBlock* block);
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
SSAInstruction* ir_emit_neg(IRBlock* block, SSAInstruction* source);
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
SSAInstruction* ir_emit_zext(IRBlock* block, SSAInstruction* source, x86_size_e size);
SSAInstruction* ir_emit_zext8(IRBlock* block, SSAInstruction* source);
SSAInstruction* ir_emit_zext16(IRBlock* block, SSAInstruction* source);
SSAInstruction* ir_emit_zext32(IRBlock* block, SSAInstruction* source);
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
SSAInstruction* ir_emit_get_flags(IRBlock* block);
void ir_emit_set_flags(IRBlock* block, SSAInstruction* flags);
void ir_emit_syscall(IRBlock* block);
SSAInstruction* ir_emit_insert_integer_to_vector(IRBlock* block, SSAInstruction* source, SSAInstruction* dest, u8 idx, x86_size_e sz);
SSAInstruction* ir_emit_extract_integer_from_vector(IRBlock* block, SSAInstruction* src, u8 idx, x86_size_e sz);
SSAInstruction* ir_emit_vector_unpack_byte_low(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_vector_unpack_word_low(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_vector_unpack_dword_low(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_vector_unpack_qword_low(IRBlock* block, SSAInstruction* source1, SSAInstruction* source2);
SSAInstruction* ir_emit_cast_vector_integer(IRBlock* block, SSAInstruction* source);
SSAInstruction* ir_emit_cast_integer_vector(IRBlock* block, SSAInstruction* source);
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

SSAInstruction* ir_emit_amoadd(IRBlock* block, SSAInstruction* address, SSAInstruction* source, MemoryOrdering ordering, x86_size_e size);
SSAInstruction* ir_emit_amoxor(IRBlock* block, SSAInstruction* address, SSAInstruction* source, MemoryOrdering ordering, x86_size_e size);
SSAInstruction* ir_emit_amoor(IRBlock* block, SSAInstruction* address, SSAInstruction* source, MemoryOrdering ordering, x86_size_e size);
SSAInstruction* ir_emit_amoand(IRBlock* block, SSAInstruction* address, SSAInstruction* source, MemoryOrdering ordering, x86_size_e size);
SSAInstruction* ir_emit_amoswap(IRBlock* block, SSAInstruction* address, SSAInstruction* source, MemoryOrdering ordering, x86_size_e size);
SSAInstruction* ir_emit_amocas(IRBlock* block, SSAInstruction* address, SSAInstruction* expected, SSAInstruction* source, MemoryOrdering ordering,
                               x86_size_e size);

void ir_emit_call_host_function(IRBlock* block, u64 function);
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

void ir_emit_rep_start(FrontendState* state, const x86_instruction_t& inst, IRBlock* loop_block, IRBlock* exit_block);
void ir_emit_rep_end(FrontendState* state, const x86_instruction_t& inst, x86_rep_e rep_type, IRBlock* loop_block, IRBlock* exit_block);

// Exits the dispatcher all together ending the current thread, with a byte to specify the reason
void ir_emit_set_exit_reason(IRBlock* block, u8 reason);

// TODO: finish this and replace the above functions
struct IREmitter {
    SSAInstruction* LoadGuestFromMemory(x86_ref_e ref);
    void StoreGuestToMemory(x86_ref_e ref, SSAInstruction* source);
    SSAInstruction* GetGuest(x86_ref_e ref);
    void SetGuest(x86_ref_e ref, SSAInstruction* source);
    SSAInstruction* GetFlag(x86_ref_e flag);
    SSAInstruction* GetFlagNot(x86_ref_e flag);
    void SetFlag(x86_ref_e flag, SSAInstruction* source);
    void SetExitReason(u8 reason);
    void Comment(const std::string& comment);

    SSAInstruction* Immediate(u64 value);
    SSAInstruction* Add(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* Addi(SSAInstruction* source, i64 imm);
    SSAInstruction* Sub(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* ShiftLeft(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* ShiftRight(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* ShiftRightArithmetic(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* Rotate(SSAInstruction* source1, SSAInstruction* source2, x86_size_e size, bool right);
    SSAInstruction* Select(SSAInstruction* condition, SSAInstruction* true_value, SSAInstruction* false_value);
    SSAInstruction* Clz(SSAInstruction* source);
    SSAInstruction* Ctz(SSAInstruction* source);
    SSAInstruction* And(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* Or(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* Xor(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* Not(SSAInstruction* source);
    SSAInstruction* Neg(SSAInstruction* source);
    SSAInstruction* GetParity(SSAInstruction* source);
    SSAInstruction* Equal(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* NotEqual(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* GreaterThanSigned(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* LessThanSigned(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* GreaterThanUnsigned(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* LessThanUnsigned(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* Lea(x86_operand_t* rm_operand);
    SSAInstruction* Sext(SSAInstruction* source, x86_size_e size);
    SSAInstruction* Zext(SSAInstruction* source, x86_size_e size);
    SSAInstruction* Div(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* Divu(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* Rem(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* Remu(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* Divw(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* Divuw(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* Remw(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* Remuw(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* Mul(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* Mulh(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* Mulhu(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* InsertIntegerToVector(SSAInstruction* source, SSAInstruction* dest, u8 idx, x86_size_e sz);
    SSAInstruction* ExtractIntegerFromVector(SSAInstruction* src, u8 idx, x86_size_e sz);
    SSAInstruction* VectorUnpackByteLow(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* VectorUnpackWordLow(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* VectorUnpackDWordLow(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* VectorUnpackQWordLow(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* CastVectorInteger(SSAInstruction* source);
    SSAInstruction* CastIntegerVector(SSAInstruction* source);
    SSAInstruction* VectorPackedAnd(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* VectorPackedOr(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* VectorPackedXor(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* VectorPackedShiftLeft(SSAInstruction* source, SSAInstruction* imm);
    SSAInstruction* VectorPackedShiftRight(SSAInstruction* source, SSAInstruction* imm);
    SSAInstruction* VectorPackedSubByte(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* VectorPackedAddQWord(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* VectorPackedCompareEqByte(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* VectorPackedCompareEQWord(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* VectorPackedCompareEqDWord(SSAInstruction* source1, SSAInstruction* source2);
    SSAInstruction* VectorPackedShuffleDWord(SSAInstruction* source, u8 control_byte);
    SSAInstruction* VectorPackedMoveByteMask(SSAInstruction* source);
    SSAInstruction* VectorPackedMinByte(SSAInstruction* source1, SSAInstruction* source2);

    SSAInstruction* ReadByte(SSAInstruction* address);
    SSAInstruction* ReadWord(SSAInstruction* address);
    SSAInstruction* ReadDWord(SSAInstruction* address);
    SSAInstruction* ReadQWord(SSAInstruction* address);
    SSAInstruction* ReadXmmWord(SSAInstruction* address);
    void WriteByte(SSAInstruction* address, SSAInstruction* source);
    void WriteWord(SSAInstruction* address, SSAInstruction* source);
    void WriteDWord(SSAInstruction* address, SSAInstruction* source);
    void WriteQWord(SSAInstruction* address, SSAInstruction* source);
    void WriteXmmWord(SSAInstruction* address, SSAInstruction* source);

    SSAInstruction* AmoAdd(SSAInstruction* address, SSAInstruction* source, MemoryOrdering ordering, x86_size_e size);
    SSAInstruction* AmoXor(SSAInstruction* address, SSAInstruction* source, MemoryOrdering ordering, x86_size_e size);
    SSAInstruction* AmoOr(SSAInstruction* address, SSAInstruction* source, MemoryOrdering ordering, x86_size_e size);
    SSAInstruction* AmoAnd(SSAInstruction* address, SSAInstruction* source, MemoryOrdering ordering, x86_size_e size);
    SSAInstruction* AmoSwap(SSAInstruction* address, SSAInstruction* source, MemoryOrdering ordering, x86_size_e size);

    void Setcc(x86_instruction_t* inst);
    void Cpuid();
    void Rdtsc();
};