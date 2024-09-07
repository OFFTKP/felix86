#include "felix86/common/log.h"
#include "felix86/frontend/instruction.h"
#include "felix86/ir/emitter.h"
#include "felix86/ir/instruction.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

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

ir_instruction_t* ir_emit_one_operand(ir_instruction_list_t* instructions, ir_opcode_e opcode, ir_instruction_t* source) {
    ir_instruction_t* instruction = ir_ilist_push_back(instructions);
    instruction->opcode = opcode;
    instruction->type = IR_TYPE_ONE_OPERAND;
    instruction->operands.args[0] = source;
    source->uses++;
    return instruction;
}

ir_instruction_t* ir_emit_two_operands(ir_instruction_list_t* instructions, ir_opcode_e opcode, ir_instruction_t* source1, ir_instruction_t* source2) {
    ir_instruction_t* instruction = ir_ilist_push_back(instructions);
    instruction->opcode = opcode;
    instruction->type = IR_TYPE_TWO_OPERANDS;
    instruction->operands.args[0] = source1;
    instruction->operands.args[1] = source2;
    source1->uses++;
    source2->uses++;
    return instruction;
}

ir_instruction_t* ir_emit_three_operands(ir_instruction_list_t* instructions, ir_opcode_e opcode, ir_instruction_t* source1, ir_instruction_t* source2, ir_instruction_t* source3) {
    ir_instruction_t* instruction = ir_ilist_push_back(instructions);
    instruction->opcode = opcode;
    instruction->type = IR_TYPE_THREE_OPERANDS;
    instruction->operands.args[0] = source1;
    instruction->operands.args[1] = source2;
    instruction->operands.args[2] = source3;
    source1->uses++;
    source2->uses++;
    source3->uses++;
    return instruction;
}

ir_instruction_t* ir_emit_four_operands(ir_instruction_list_t* instructions, ir_opcode_e opcode, ir_instruction_t* source1, ir_instruction_t* source2, ir_instruction_t* source3, ir_instruction_t* source4) {
    ir_instruction_t* instruction = ir_ilist_push_back(instructions);
    instruction->opcode = opcode;
    instruction->type = IR_TYPE_FOUR_OPERANDS;
    instruction->operands.args[0] = source1;
    instruction->operands.args[1] = source2;
    instruction->operands.args[2] = source3;
    instruction->operands.args[3] = source4;
    source1->uses++;
    source2->uses++;
    source3->uses++;
    source4->uses++;
    return instruction;
}

void ir_emit_side_effect(ir_instruction_list_t* instructions, ir_opcode_e opcode, x86_ref_e* refs, u8 count) {
    ir_instruction_t* instruction = ir_ilist_push_back(instructions);
    instruction->opcode = opcode;
    instruction->type = IR_TYPE_SIDE_EFFECTS;
    for (u8 i = 0; i < count; i++) {
        instruction->side_effect.registers_affected[i] = refs[i];
    }
    instruction->side_effect.count = count;
}

void ir_emit_hint_inputs(ir_instruction_list_t* instructions, x86_ref_e* refs, u8 count)
{
    ir_emit_side_effect(instructions, IR_HINT_INPUTS, refs, count);
}

void ir_emit_hint_outputs(ir_instruction_list_t* instructions, x86_ref_e* refs, u8 count)
{
    ir_emit_side_effect(instructions, IR_HINT_OUTPUTS, refs, count);
}

ir_instruction_t* ir_emit_add(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2)
{
    return ir_emit_two_operands(instructions, IR_ADD, source1, source2);
}

ir_instruction_t* ir_emit_sub(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2)
{
    return ir_emit_two_operands(instructions, IR_SUB, source1, source2);
}

ir_instruction_t* ir_emit_shift_left(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2)
{
    return ir_emit_two_operands(instructions, IR_SHIFT_LEFT, source1, source2);
}

ir_instruction_t* ir_emit_shift_right(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2)
{
    return ir_emit_two_operands(instructions, IR_SHIFT_RIGHT, source1, source2);
}

ir_instruction_t* ir_emit_shift_right_arithmetic(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2)
{
    return ir_emit_two_operands(instructions, IR_SHIFT_RIGHT_ARITHMETIC, source1, source2);
}

ir_instruction_t* ir_emit_rotate(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2, x86_size_e size_e, bool right)
{
    u8 size = get_bit_size(size_e);
    ir_opcode_e type;
    ir_instruction_t* count = source2;
    switch (size) {
        case 8: type = IR_LEFT_ROTATE8; if (right) { count = ir_emit_sub(instructions, ir_emit_immediate(instructions, 8), count); } break;
        case 16: type = IR_LEFT_ROTATE16; if (right) { count = ir_emit_sub(instructions, ir_emit_immediate(instructions, 16), count); } break;
        case 32: type = IR_LEFT_ROTATE32; if (right) { count = ir_emit_sub(instructions, ir_emit_immediate(instructions, 32), count); } break;
        case 64: type = IR_LEFT_ROTATE64; if (right) { count = ir_emit_sub(instructions, ir_emit_immediate(instructions, 64), count); } break;
    }
    return ir_emit_two_operands(instructions, type, source1, count);
}

ir_instruction_t* ir_emit_select(ir_instruction_list_t* instructions, ir_instruction_t* condition, ir_instruction_t* source1, ir_instruction_t* source2)
{
    return ir_emit_three_operands(instructions, IR_SELECT, condition, source1, source2);
}

ir_instruction_t* ir_emit_udiv(ir_instruction_list_t* instructions, ir_instruction_t* source)
{
    return ir_emit_one_operand(instructions, IR_UDIV, source);
}

ir_instruction_t* ir_emit_and(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2)
{
    return ir_emit_two_operands(instructions, IR_AND, source1, source2);
}

ir_instruction_t* ir_emit_or(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2)
{
    return ir_emit_two_operands(instructions, IR_OR, source1, source2);
}

ir_instruction_t* ir_emit_xor(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2)
{
    return ir_emit_two_operands(instructions, IR_XOR, source1, source2);
}

ir_instruction_t* ir_emit_not(ir_instruction_list_t* instructions, ir_instruction_t* source)
{
    return ir_emit_one_operand(instructions, IR_NOT, source);
}

ir_instruction_t* ir_emit_equal(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2)
{
    return ir_emit_two_operands(instructions, IR_EQUAL, source1, source2);
}

ir_instruction_t* ir_emit_not_equal(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2)
{
    return ir_emit_two_operands(instructions, IR_NOT_EQUAL, source1, source2);
}

ir_instruction_t* ir_emit_greater_than_signed(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2)
{
    return ir_emit_two_operands(instructions, IR_GREATER_THAN_SIGNED, source1, source2);
}

ir_instruction_t* ir_emit_less_than_signed(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2)
{
    return ir_emit_two_operands(instructions, IR_LESS_THAN_SIGNED, source1, source2);
}

ir_instruction_t* ir_emit_greater_than_unsigned(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2)
{
    return ir_emit_two_operands(instructions, IR_GREATER_THAN_UNSIGNED, source1, source2);
}

ir_instruction_t* ir_emit_less_than_unsigned(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2)
{
    return ir_emit_two_operands(instructions, IR_LESS_THAN_UNSIGNED, source1, source2);
}

ir_instruction_t* ir_emit_lea(ir_instruction_list_t* instructions, x86_operand_t* rm_operand)
{
    ir_instruction_t* (*get_guest)(ir_instruction_list_t* instructions, x86_ref_e reg) = rm_operand->memory.address_override ? ir_emit_get_gpr32 : ir_emit_get_gpr64;

    ir_instruction_t* base = rm_operand->memory.base != X86_REF_COUNT ? get_guest(instructions, rm_operand->memory.base) : ir_emit_immediate(instructions, 0);

    ir_instruction_t* base_final = base;
    if (rm_operand->memory.fs_override) {
        ir_instruction_t* fs = ir_emit_get_guest(instructions, X86_REF_FS);
        base_final = ir_emit_add(instructions, base, fs);
    } else if (rm_operand->memory.gs_override) {
        ir_instruction_t* gs = ir_emit_get_guest(instructions, X86_REF_GS);
        base_final = ir_emit_add(instructions, base, gs);
    }

    ir_instruction_t* index = rm_operand->memory.index != X86_REF_COUNT ? get_guest(instructions, rm_operand->memory.index) : ir_emit_immediate(instructions, 0);
    ir_instruction_t* displacement = ir_emit_immediate(instructions, rm_operand->memory.displacement);
    ir_instruction_t* scale = ir_emit_immediate(instructions, rm_operand->memory.scale);
    ir_instruction_t* address = ir_emit_four_operands(instructions, IR_LEA, base_final, index, scale, displacement);

    ir_instruction_t* final_address = address;
    if (rm_operand->memory.address_override) {
        ir_instruction_t* mask = ir_emit_immediate(instructions, 0xFFFFFFFF);
        final_address = ir_emit_and(instructions, address, mask);
    }

    return final_address;
}

ir_instruction_t* ir_emit_popcount(ir_instruction_list_t* instructions, ir_instruction_t* source)
{
    return ir_emit_one_operand(instructions, IR_POPCOUNT, source);
}

ir_instruction_t* ir_emit_sext8(ir_instruction_list_t* instructions, ir_instruction_t* source)
{
    return ir_emit_one_operand(instructions, IR_SEXT_GPR8, source);
}

ir_instruction_t* ir_emit_sext16(ir_instruction_list_t* instructions, ir_instruction_t* source)
{
    return ir_emit_one_operand(instructions, IR_SEXT_GPR16, source);
}

ir_instruction_t* ir_emit_sext32(ir_instruction_list_t* instructions, ir_instruction_t* source)
{
    return ir_emit_one_operand(instructions, IR_SEXT_GPR32, source);
}

ir_instruction_t* ir_emit_syscall(ir_instruction_list_t* instructions)
{
    ir_instruction_t* instruction = ir_ilist_push_back(instructions);
    instruction->opcode = IR_SYSCALL;
    instruction->type = IR_TYPE_NO_OPERANDS;
    return instruction;
}

ir_instruction_t* ir_emit_exit(ir_instruction_list_t* instructions)
{
    ir_instruction_t* instruction = ir_ilist_push_back(instructions);
    instruction->opcode = IR_EXIT;
    instruction->type = IR_TYPE_NO_OPERANDS;
    return instruction;
}

ir_instruction_t* ir_emit_jump_register(ir_instruction_list_t* instructions, ir_instruction_t* target)
{
    return ir_emit_one_operand(instructions, IR_JUMP_REGISTER, target);
}

ir_instruction_t* ir_emit_jump(ir_instruction_list_t* instructions, ir_block_t* target)
{
    ir_instruction_t* instruction = ir_ilist_push_back(instructions);
    instruction->opcode = IR_JUMP;
    instruction->type = IR_TYPE_JUMP;
    instruction->jump.target = target;
    return instruction;
}

ir_instruction_t* ir_emit_jump_conditional(ir_instruction_list_t* instructions, ir_instruction_t* condition, ir_block_t* target_true, ir_block_t* target_false)
{
    ir_instruction_t* instruction = ir_ilist_push_back(instructions);
    instruction->opcode = IR_JUMP_CONDITIONAL;
    instruction->type = IR_TYPE_JUMP_CONDITIONAL;
    instruction->jump_conditional.condition = condition;
    instruction->jump_conditional.target_true = target_true;
    instruction->jump_conditional.target_false = target_false;
    condition->uses++;
    return instruction;
}

ir_instruction_t* ir_emit_insert_integer_to_vector(ir_instruction_list_t* instructions, ir_instruction_t* dst, ir_instruction_t* source, u8 idx, x86_size_e sz)
{
    ir_instruction_t* index = ir_emit_immediate(instructions, idx);
    ir_instruction_t* size = ir_emit_immediate(instructions, sz);
    return ir_emit_four_operands(instructions, IR_INSERT_INTEGER_TO_VECTOR, dst, source, index, size);
}

ir_instruction_t* ir_emit_extract_integer_from_vector(ir_instruction_list_t* instructions, ir_instruction_t* src, u8 idx, x86_size_e sz)
{
    ir_instruction_t* index = ir_emit_immediate(instructions, idx);
    ir_instruction_t* size = ir_emit_immediate(instructions, sz);
    return ir_emit_three_operands(instructions, IR_EXTRACT_INTEGER_FROM_VECTOR, src, index, size);
}

ir_instruction_t* ir_emit_vector_unpack_dword_low(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2)
{
    return ir_emit_two_operands(instructions, IR_VECTOR_UNPACK_DWORD_LOW, source1, source2);
}

ir_instruction_t* ir_emit_vector_from_integer(ir_instruction_list_t* instructions, ir_instruction_t* source)
{
    return ir_emit_one_operand(instructions, IR_VECTOR_FROM_INTEGER, source);
}

ir_instruction_t* ir_emit_integer_from_vector(ir_instruction_list_t* instructions, ir_instruction_t* source)
{
    return ir_emit_one_operand(instructions, IR_INTEGER_FROM_VECTOR, source);
}

ir_instruction_t* ir_emit_vector_packed_and(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2)
{
    return ir_emit_two_operands(instructions, IR_VECTOR_PACKED_AND, source1, source2);
}

ir_instruction_t* ir_emit_get_guest(ir_instruction_list_t* instructions, x86_ref_e ref)
{
    if (ref == X86_REF_COUNT) {
        ERROR("Invalid register reference");
    }

    ir_instruction_t* instruction = ir_ilist_push_back(instructions);
    instruction->opcode = IR_GET_GUEST;
    instruction->type = IR_TYPE_GET_GUEST;
    instruction->get_guest.ref = ref;
    return instruction;
}

void ir_emit_set_guest(ir_instruction_list_t* instructions, x86_ref_e ref, ir_instruction_t* source)
{
    ir_instruction_t* instruction = ir_ilist_push_back(instructions);
    instruction->opcode = IR_SET_GUEST;
    instruction->type = IR_TYPE_SET_GUEST;
    instruction->set_guest.ref = ref;
    instruction->set_guest.source = source;
    source->uses++;
}

ir_instruction_t* ir_emit_get_flag(ir_instruction_list_t* instructions, x86_ref_e flag) {
    ir_instruction_t* instruction = ir_ilist_push_back(instructions);
    instruction->opcode = IR_GET_GUEST;
    instruction->type = IR_TYPE_GET_GUEST;
    instruction->get_guest.ref = flag;
    return instruction;
}

ir_instruction_t* ir_emit_get_flag_not(ir_instruction_list_t* instructions, x86_ref_e flag) {
    ir_instruction_t* instruction = ir_emit_get_flag(instructions, flag);
    ir_instruction_t* one = ir_emit_immediate(instructions, 1);
    return ir_emit_xor(instructions, instruction, one);
}

void ir_emit_set_flag(ir_instruction_list_t* instructions, x86_ref_e flag, ir_instruction_t* source) {
    ir_instruction_t* instruction = ir_ilist_push_back(instructions);
    instruction->opcode = IR_SET_GUEST;
    instruction->type = IR_TYPE_SET_GUEST;
    instruction->set_guest.ref = flag;
    instruction->set_guest.source = source;
    source->uses++;
}

ir_instruction_t* ir_emit_read_byte(ir_instruction_list_t* instructions, ir_instruction_t* address)
{
    return ir_emit_one_operand(instructions, IR_READ_BYTE, address);
}

ir_instruction_t* ir_emit_read_word(ir_instruction_list_t* instructions, ir_instruction_t* address)
{
    return ir_emit_one_operand(instructions, IR_READ_WORD, address);
}

ir_instruction_t* ir_emit_read_dword(ir_instruction_list_t* instructions, ir_instruction_t* address)
{
    return ir_emit_one_operand(instructions, IR_READ_DWORD, address);
}

ir_instruction_t* ir_emit_read_qword(ir_instruction_list_t* instructions, ir_instruction_t* address)
{
    return ir_emit_one_operand(instructions, IR_READ_QWORD, address);
}

ir_instruction_t* ir_emit_read_xmmword(ir_instruction_list_t* instructions, ir_instruction_t* address)
{
    return ir_emit_one_operand(instructions, IR_READ_XMMWORD, address);
}

void ir_emit_write_byte(ir_instruction_list_t* instructions, ir_instruction_t* address, ir_instruction_t* source)
{
    ir_emit_two_operands(instructions, IR_WRITE_BYTE, address, source);
}

void ir_emit_write_word(ir_instruction_list_t* instructions, ir_instruction_t* address, ir_instruction_t* source)
{
    ir_emit_two_operands(instructions, IR_WRITE_WORD, address, source);
}

void ir_emit_write_dword(ir_instruction_list_t* instructions, ir_instruction_t* address, ir_instruction_t* source)
{
    ir_emit_two_operands(instructions, IR_WRITE_DWORD, address, source);
}

void ir_emit_write_qword(ir_instruction_list_t* instructions, ir_instruction_t* address, ir_instruction_t* source)
{
    ir_emit_two_operands(instructions, IR_WRITE_QWORD, address, source);
}

ir_instruction_t* ir_emit_cpuid(ir_instruction_list_t* instructions)
{
    ir_instruction_t* instruction = ir_ilist_push_back(instructions);
    instruction->opcode = IR_CPUID;
    instruction->type = IR_TYPE_NO_OPERANDS;
    return instruction;
}

ir_instruction_t* ir_emit_immediate(ir_instruction_list_t* instructions, u64 value)
{
    ir_instruction_t* instruction = ir_ilist_push_back(instructions);
    instruction->opcode = IR_IMMEDIATE;
    instruction->type = IR_TYPE_LOAD_IMMEDIATE;
    instruction->load_immediate.immediate = value;
    return instruction;
}

ir_instruction_t* ir_emit_immediate_sext(ir_instruction_list_t* instructions, x86_operand_t* operand)
{
    i64 value = operand->immediate.data;
    switch (operand->size) {
        case X86_SIZE_BYTE: value = (i8)value; break;
        case X86_SIZE_WORD: value = (i16)value; break;
        case X86_SIZE_DWORD: value = (i32)value; break;
        case X86_SIZE_QWORD: break;
        default: ERROR("Invalid immediate size");
    }

    ir_instruction_t* instruction = ir_ilist_push_back(instructions);
    instruction->opcode = IR_IMMEDIATE;
    instruction->type = IR_TYPE_LOAD_IMMEDIATE;
    instruction->load_immediate.immediate = value;
    return instruction;
}

// ██   ██ ███████ ██      ██████  ███████ ██████  ███████
// ██   ██ ██      ██      ██   ██ ██      ██   ██ ██     
// ███████ █████   ██      ██████  █████   ██████  ███████
// ██   ██ ██      ██      ██      ██      ██   ██      ██
// ██   ██ ███████ ███████ ██      ███████ ██   ██ ███████

ir_instruction_t* ir_emit_get_reg(ir_instruction_list_t* instructions, x86_operand_t* reg_operand)
{
    switch (reg_operand->size) {
        case X86_SIZE_BYTE: {
            if (reg_operand->reg.high8) {
                return ir_emit_get_gpr8_high(instructions, reg_operand->reg.ref);
            } else {
                return ir_emit_get_gpr8_low(instructions, reg_operand->reg.ref);
            }
        }
        case X86_SIZE_WORD: return ir_emit_get_gpr16(instructions, reg_operand->reg.ref);
        case X86_SIZE_DWORD: return ir_emit_get_gpr32(instructions, reg_operand->reg.ref);
        case X86_SIZE_QWORD: return ir_emit_get_gpr64(instructions, reg_operand->reg.ref);
        case X86_SIZE_XMM:
        case X86_SIZE_YMM: 
        case X86_SIZE_ZMM: return ir_emit_get_vector(instructions, reg_operand->reg.ref);
        default: ERROR("Invalid register size"); return NULL;
    }
}

ir_instruction_t* ir_emit_get_rm(ir_instruction_list_t* instructions, x86_operand_t* rm_operand)
{
    if (rm_operand->type == X86_OP_TYPE_REGISTER) {
        return ir_emit_get_reg(instructions, rm_operand);
    } else {
        ir_instruction_t* address = ir_emit_lea(instructions, rm_operand);
        return ir_emit_read_memory(instructions, address, rm_operand->size);
    }
}

void ir_emit_set_reg(ir_instruction_list_t* instructions, x86_operand_t* reg_operand, ir_instruction_t* source)
{
    switch (reg_operand->size) {
        case X86_SIZE_BYTE: {
            if (reg_operand->reg.high8) {
                return ir_emit_set_gpr8_high(instructions, reg_operand->reg.ref, source);
            } else {
                return ir_emit_set_gpr8_low(instructions, reg_operand->reg.ref, source);
            }
        }
        case X86_SIZE_WORD: return ir_emit_set_gpr16(instructions, reg_operand->reg.ref, source);
        case X86_SIZE_DWORD: return ir_emit_set_gpr32(instructions, reg_operand->reg.ref, source);
        case X86_SIZE_QWORD: return ir_emit_set_gpr64(instructions, reg_operand->reg.ref, source);
        case X86_SIZE_XMM:
        case X86_SIZE_YMM:
        case X86_SIZE_ZMM: return ir_emit_set_vector(instructions, reg_operand->reg.ref, source);
        default: ERROR("Invalid register size"); return;
    }
}

void ir_emit_set_rm(ir_instruction_list_t* instructions, x86_operand_t* rm_operand, ir_instruction_t* source)
{
    if (rm_operand->type == X86_OP_TYPE_REGISTER) {
        return ir_emit_set_reg(instructions, rm_operand, source);
    } else {
        ir_instruction_t* address = ir_emit_lea(instructions, rm_operand);
        return ir_emit_write_memory(instructions, address, source, rm_operand->size);
    }
}

void ir_emit_write_memory(ir_instruction_list_t* instructions, ir_instruction_t* address, ir_instruction_t* value, x86_size_e size)
{
    switch (size) {
        case X86_SIZE_BYTE: return ir_emit_write_byte(instructions, address, value);
        case X86_SIZE_WORD: return ir_emit_write_word(instructions, address, value);
        case X86_SIZE_DWORD: return ir_emit_write_dword(instructions, address, value);
        case X86_SIZE_QWORD: return ir_emit_write_qword(instructions, address, value);
        default: ERROR("Invalid memory size"); return;
    }
}

ir_instruction_t* ir_emit_read_memory(ir_instruction_list_t* instructions, ir_instruction_t* address, x86_size_e size)
{
    switch (size) {
        case X86_SIZE_BYTE: return ir_emit_read_byte(instructions, address);
        case X86_SIZE_WORD: return ir_emit_read_word(instructions, address);
        case X86_SIZE_DWORD: return ir_emit_read_dword(instructions, address);
        case X86_SIZE_QWORD: return ir_emit_read_qword(instructions, address);
        case X86_SIZE_XMM: return ir_emit_read_xmmword(instructions, address);
        default: ERROR("Invalid memory size"); return NULL;
    }
}

ir_instruction_t* ir_emit_get_gpr8_low(ir_instruction_list_t* instructions, x86_ref_e reg)
{
    if (reg < X86_REF_RAX || reg > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    ir_instruction_t* full_reg = ir_emit_get_guest(instructions, reg);
    ir_instruction_t* mask = ir_emit_immediate(instructions, 0xFF);
    ir_instruction_t* instruction = ir_emit_and(instructions, full_reg, mask);

    return instruction;
}

ir_instruction_t* ir_emit_get_gpr8_high(ir_instruction_list_t* instructions, x86_ref_e reg)
{
    if (reg < X86_REF_RAX || reg > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    ir_instruction_t* full_reg = ir_emit_get_guest(instructions, reg);
    ir_instruction_t* shift = ir_emit_immediate(instructions, 8);
    ir_instruction_t* shifted = ir_emit_shift_right(instructions, full_reg, shift);
    ir_instruction_t* mask = ir_emit_immediate(instructions, 0xFF);
    ir_instruction_t* instruction = ir_emit_and(instructions, shifted, mask);

    return instruction;
}

ir_instruction_t* ir_emit_get_gpr16(ir_instruction_list_t* instructions, x86_ref_e reg)
{
    if (reg < X86_REF_RAX || reg > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    ir_instruction_t* full_reg = ir_emit_get_guest(instructions, reg);
    ir_instruction_t* mask = ir_emit_immediate(instructions, 0xFFFF);
    ir_instruction_t* instruction = ir_emit_and(instructions, full_reg, mask);

    return instruction;
}

ir_instruction_t* ir_emit_get_gpr32(ir_instruction_list_t* instructions, x86_ref_e reg)
{
    if (reg < X86_REF_RAX || reg > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    ir_instruction_t* full_reg = ir_emit_get_guest(instructions, reg);
    ir_instruction_t* mask = ir_emit_immediate(instructions, 0xFFFFFFFF);
    ir_instruction_t* instruction = ir_emit_and(instructions, full_reg, mask);

    return instruction;
}

ir_instruction_t* ir_emit_get_gpr64(ir_instruction_list_t* instructions, x86_ref_e reg)
{
    if (reg < X86_REF_RAX || reg > X86_REF_R15) {
        ERROR("Invalid register reference");
    }

    ir_instruction_t* instruction = ir_emit_get_guest(instructions, reg);
    return instruction;
}

ir_instruction_t* ir_emit_get_vector(ir_instruction_list_t* instructions, x86_ref_e reg)
{
    if (reg < X86_REF_XMM0 || reg > X86_REF_XMM15) {
        ERROR("Invalid register reference");
    }

    ir_instruction_t* instruction = ir_emit_get_guest(instructions, reg);
    return instruction;
}

void ir_emit_set_gpr8_low(ir_instruction_list_t* instructions, x86_ref_e reg, ir_instruction_t* source)
{
    ir_instruction_t* full_reg = ir_emit_get_guest(instructions, reg);
    ir_instruction_t* mask = ir_emit_immediate(instructions, 0xFFFFFFFFFFFFFF00);
    ir_instruction_t* masked = ir_emit_and(instructions, full_reg, mask);
    ir_instruction_t* value_mask = ir_emit_immediate(instructions, 0xFF);
    ir_instruction_t* value = ir_emit_and(instructions, source, value_mask);
    ir_instruction_t* final_value = ir_emit_or(instructions, masked, value);
    ir_emit_set_guest(instructions, reg, final_value);
}

void ir_emit_set_gpr8_high(ir_instruction_list_t* instructions, x86_ref_e reg, ir_instruction_t* source)
{
    ir_instruction_t* full_reg = ir_emit_get_guest(instructions, reg);
    ir_instruction_t* mask = ir_emit_immediate(instructions, 0xFFFFFFFFFFFF00FF);
    ir_instruction_t* masked = ir_emit_and(instructions, full_reg, mask);
    ir_instruction_t* value_mask = ir_emit_immediate(instructions, 0xFF);
    ir_instruction_t* value = ir_emit_and(instructions, source, value_mask);
    ir_instruction_t* shift = ir_emit_immediate(instructions, 8);
    ir_instruction_t* shifted = ir_emit_shift_left(instructions, value, shift);
    ir_instruction_t* final_value = ir_emit_or(instructions, masked, shifted);
    ir_emit_set_guest(instructions, reg, final_value);
}

void ir_emit_set_gpr16(ir_instruction_list_t* instructions, x86_ref_e reg, ir_instruction_t* source)
{
    ir_instruction_t* full_reg = ir_emit_get_guest(instructions, reg);
    ir_instruction_t* mask = ir_emit_immediate(instructions, 0xFFFFFFFFFFFF0000);
    ir_instruction_t* masked = ir_emit_and(instructions, full_reg, mask);
    ir_instruction_t* value_mask = ir_emit_immediate(instructions, 0xFFFF);
    ir_instruction_t* value = ir_emit_and(instructions, source, value_mask);
    ir_instruction_t* final_value = ir_emit_or(instructions, masked, value);
    ir_emit_set_guest(instructions, reg, final_value);
}

void ir_emit_set_gpr32(ir_instruction_list_t* instructions, x86_ref_e reg, ir_instruction_t* source)
{
    ir_instruction_t* value_mask = ir_emit_immediate(instructions, 0xFFFFFFFF);
    ir_instruction_t* final_value = ir_emit_and(instructions, source, value_mask);
    ir_emit_set_guest(instructions, reg, final_value);
}

void ir_emit_set_gpr64(ir_instruction_list_t* instructions, x86_ref_e reg, ir_instruction_t* source)
{
    ir_emit_set_guest(instructions, reg, source);
}

void ir_emit_set_vector(ir_instruction_list_t* instructions, x86_ref_e reg, ir_instruction_t* source)
{
    ir_emit_set_guest(instructions, reg, source);
}

ir_instruction_t* ir_emit_get_parity(ir_instruction_list_t* instructions, ir_instruction_t* source)
{
    ir_instruction_t* mask = ir_emit_immediate(instructions, 0xFF);
    ir_instruction_t* masked = ir_emit_and(instructions, source, mask);
    ir_instruction_t* popcount = ir_emit_popcount(instructions, masked);
    ir_instruction_t* one = ir_emit_immediate(instructions, 1);
    ir_instruction_t* result = ir_emit_and(instructions, popcount, one);
    ir_instruction_t* instruction = ir_emit_xor(instructions, result, one);

    return instruction;
}

ir_instruction_t* ir_emit_get_zero(ir_instruction_list_t* instructions, ir_instruction_t* source)
{
    ir_instruction_t* zero = ir_emit_immediate(instructions, 0);
    ir_instruction_t* instruction = ir_emit_equal(instructions, source, zero);

    return instruction;
}

ir_instruction_t* ir_emit_get_size(ir_instruction_list_t* instructions, x86_size_e size_e)
{
    return ir_emit_immediate(instructions, get_bit_size(size_e));
}

ir_instruction_t* ir_emit_get_sign_mask(ir_instruction_list_t* instructions, x86_size_e size_e)
{
    u16 size = get_bit_size(size_e);
    return ir_emit_immediate(instructions, 1ull << (size - 1));
}

ir_instruction_t* ir_emit_get_shift_mask_left(ir_instruction_list_t* instructions, ir_instruction_t* source, x86_size_e size_e)
{
    ir_instruction_t* one = ir_emit_immediate(instructions, 1);
    ir_instruction_t* shiftMax = ir_emit_get_size(instructions, size_e);
    ir_instruction_t* shift = ir_emit_sub(instructions, shiftMax, source);
    ir_instruction_t* mask = ir_emit_shift_left(instructions, one, shift);
    return mask;
}

ir_instruction_t* ir_emit_get_shift_mask_right(ir_instruction_list_t* instructions, ir_instruction_t* source)
{
    ir_instruction_t* zero = ir_emit_immediate(instructions, 0);
    ir_instruction_t* is_zero = ir_emit_equal(instructions, source, zero);
    ir_instruction_t* one = ir_emit_immediate(instructions, 1);
    ir_instruction_t* shift = ir_emit_sub(instructions, source, one);
    ir_instruction_t* mask = ir_emit_shift_left(instructions, one, shift);
    return ir_emit_select(instructions, is_zero, zero, mask);
}

ir_instruction_t* ir_emit_get_mask(ir_instruction_list_t* instructions, x86_size_e size_e)
{
    u16 size = get_bit_size(size_e);
    return ir_emit_immediate(instructions, (1ull << size) - 1);
}

ir_instruction_t* ir_emit_get_sign(ir_instruction_list_t* instructions, ir_instruction_t* source, x86_size_e size_e)
{
    ir_instruction_t* mask = ir_emit_get_sign_mask(instructions, size_e);
    ir_instruction_t* masked = ir_emit_and(instructions, source, mask);
    ir_instruction_t* instruction = ir_emit_equal(instructions, masked, mask);

    return instruction;
}

ir_instruction_t* ir_emit_get_overflow_add(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2, ir_instruction_t* result, x86_size_e size_e)
{
    ir_instruction_t* mask = ir_emit_get_sign_mask(instructions, size_e);

    // for x + y = z, overflow occurs if ((z ^ x) & (z ^ y) & mask) == mask
    // which essentially checks if the sign bits of x and y are equal, but the sign bit of z is different
    ir_instruction_t* xor1 = ir_emit_xor(instructions, result, source1);
    ir_instruction_t* xor2 = ir_emit_xor(instructions, result, source2);
    ir_instruction_t* and = ir_emit_and(instructions, xor1, xor2);
    ir_instruction_t* masked = ir_emit_and(instructions, and, mask);

    return ir_emit_equal(instructions, masked, mask);
}

ir_instruction_t* ir_emit_get_overflow_sub(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2, ir_instruction_t* result, x86_size_e size_e)
{
    ir_instruction_t* mask = ir_emit_get_sign_mask(instructions, size_e);

    // for x - y = z, overflow occurs if ((x ^ y) & (x ^ z) & mask) == mask
    ir_instruction_t* xor1 = ir_emit_xor(instructions, source1, source2);
    ir_instruction_t* xor2 = ir_emit_xor(instructions, source1, result);
    ir_instruction_t* and = ir_emit_and(instructions, xor1, xor2);
    ir_instruction_t* masked = ir_emit_and(instructions, and, mask);

    return ir_emit_equal(instructions, masked, mask);
}

ir_instruction_t* ir_emit_get_carry_add(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2, ir_instruction_t* result, x86_size_e size_e)
{
    (void)source2; // dont need, just keeping for consistency

    // CF = result < source1, as that means that the result overflowed
    ir_instruction_t* mask = ir_emit_get_mask(instructions, size_e);
    ir_instruction_t* masked_result = ir_emit_and(instructions, result, mask);
    return ir_emit_less_than_unsigned(instructions, masked_result, source1);
}

ir_instruction_t* ir_emit_get_carry_adc(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2, x86_size_e size_e)
{
    ir_instruction_t* carry_in = ir_emit_get_flag(instructions, X86_REF_CF);
    ir_instruction_t* sum = ir_emit_add(instructions, source1, source2);
    ir_instruction_t* sum_with_carry = ir_emit_add(instructions, sum, carry_in);

    ir_instruction_t* carry1 = ir_emit_get_carry_add(instructions, source1, source2, sum, size_e);
    ir_instruction_t* carry2 = ir_emit_get_carry_add(instructions, sum, carry_in, sum_with_carry, size_e);

    return ir_emit_or(instructions, carry1, carry2);
}

ir_instruction_t* ir_emit_get_carry_sub(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2, ir_instruction_t* result, x86_size_e size_e)
{
    (void)result; // dont need, just keeping for consistency

    // CF = source1 < source2, as that means that the result would underflow
    return ir_emit_less_than_unsigned(instructions, source1, source2);
}

ir_instruction_t* ir_emit_get_carry_sbb(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2, x86_size_e size_e)
{
    ir_instruction_t* carry_in = ir_emit_get_flag(instructions, X86_REF_CF);
    ir_instruction_t* sum = ir_emit_sub(instructions, source1, source2);
    ir_instruction_t* sum_with_carry = ir_emit_sub(instructions, sum, carry_in);

    ir_instruction_t* carry1 = ir_emit_get_carry_sub(instructions, source1, source2, sum, size_e);
    ir_instruction_t* carry2 = ir_emit_get_carry_sub(instructions, sum, carry_in, sum_with_carry, size_e);

    return ir_emit_or(instructions, carry1, carry2);
}

ir_instruction_t* ir_emit_get_aux_add(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2)
{
    ir_instruction_t* mask = ir_emit_immediate(instructions, 0xF);
    ir_instruction_t* and1 = ir_emit_and(instructions, source1, mask);
    ir_instruction_t* and2 = ir_emit_and(instructions, source2, mask);
    ir_instruction_t* result = ir_emit_add(instructions, and1, and2);

    return ir_emit_greater_than_unsigned(instructions, result, mask);
}

ir_instruction_t* ir_emit_get_aux_sub(ir_instruction_list_t* instructions, ir_instruction_t* source1, ir_instruction_t* source2)
{
    ir_instruction_t* mask = ir_emit_immediate(instructions, 0xF);
    ir_instruction_t* and1 = ir_emit_and(instructions, source1, mask);
    ir_instruction_t* and2 = ir_emit_and(instructions, source2, mask);

    return ir_emit_less_than_unsigned(instructions, and1, and2);
}

ir_instruction_t* ir_emit_set_cpazso(ir_instruction_list_t* instructions, ir_instruction_t* c, ir_instruction_t* p, ir_instruction_t* a, ir_instruction_t* z, ir_instruction_t* s, ir_instruction_t* o)
{
    if (c) ir_emit_set_flag(instructions, X86_REF_CF, c);
    if (p) ir_emit_set_flag(instructions, X86_REF_PF, p);
    if (a) ir_emit_set_flag(instructions, X86_REF_AF, a);
    if (z) ir_emit_set_flag(instructions, X86_REF_ZF, z);
    if (s) ir_emit_set_flag(instructions, X86_REF_SF, s);
    if (o) ir_emit_set_flag(instructions, X86_REF_OF, o);

    return NULL;
}

void ir_emit_group1_imm(ir_instruction_list_t* instructions, x86_instruction_t* inst) {
    x86_group1_e opcode = inst->operand_reg.reg.ref - X86_REF_RAX;

    ir_instruction_t* rm = ir_emit_get_rm(instructions, &inst->operand_rm);
    ir_instruction_t* imm = ir_emit_immediate_sext(instructions, &inst->operand_imm);
    ir_instruction_t* result = NULL;
    ir_instruction_t* zero = ir_emit_immediate(instructions, 0);
    ir_instruction_t* c = zero;
    ir_instruction_t* o = zero;
    ir_instruction_t* a = NULL;

    switch (opcode) {
        case X86_GROUP1_ADD: {
            result = ir_emit_add(instructions, rm, imm);
            c = ir_emit_get_carry_add(instructions, rm, imm, result, inst->operand_rm.size);
            o = ir_emit_get_overflow_add(instructions, rm, imm, result, inst->operand_rm.size);
            a = ir_emit_get_aux_add(instructions, rm, imm);
            break;
        }
        case X86_GROUP1_ADC: {
            ir_instruction_t* carry_in = ir_emit_get_flag(instructions, X86_REF_CF);
            ir_instruction_t* imm_carry = ir_emit_add(instructions, imm, carry_in);
            result = ir_emit_add(instructions, rm, imm_carry);
            c = ir_emit_get_carry_adc(instructions, rm, imm_carry, inst->operand_rm.size);
            o = ir_emit_get_overflow_add(instructions, rm, imm_carry, result, inst->operand_rm.size);
            a = ir_emit_get_aux_add(instructions, rm, imm_carry);
            break;
        }
        case X86_GROUP1_SBB: {
            ir_instruction_t* carry_in = ir_emit_get_flag(instructions, X86_REF_CF);
            ir_instruction_t* imm_carry = ir_emit_add(instructions, imm, carry_in);
            result = ir_emit_sub(instructions, rm, imm_carry);
            c = ir_emit_get_carry_sbb(instructions, rm, imm_carry, inst->operand_rm.size);
            o = ir_emit_get_overflow_sub(instructions, rm, imm_carry, result, inst->operand_rm.size);
            a = ir_emit_get_aux_sub(instructions, rm, imm_carry);
            break;
        }
        case X86_GROUP1_OR: {
            result = ir_emit_or(instructions, rm, imm);
            break;
        }
        case X86_GROUP1_AND: {
            result = ir_emit_and(instructions, rm, imm);
            break;
        }
        case X86_GROUP1_SUB: {
            result = ir_emit_sub(instructions, rm, imm);
            c = ir_emit_get_carry_sub(instructions, rm, imm, result, inst->operand_rm.size);
            o = ir_emit_get_overflow_sub(instructions, rm, imm, result, inst->operand_rm.size);
            a = ir_emit_get_aux_sub(instructions, rm, imm);
            break;
        }
        case X86_GROUP1_XOR: {
            result = ir_emit_xor(instructions, rm, imm);
            break;
        }
        case X86_GROUP1_CMP: {
            result = ir_emit_sub(instructions, rm, imm);
            c = ir_emit_get_carry_sub(instructions, rm, imm, result, inst->operand_rm.size);
            o = ir_emit_get_overflow_sub(instructions, rm, imm, result, inst->operand_rm.size);
            a = ir_emit_get_aux_sub(instructions, rm, imm);
            break;
        }
    }

    ir_instruction_t* p = ir_emit_get_parity(instructions, result);
    ir_instruction_t* z = ir_emit_get_zero(instructions, result);
    ir_instruction_t* s = ir_emit_get_sign(instructions, result, inst->operand_rm.size);

    ir_emit_set_cpazso(instructions, c, p, a, z, s, o);

    if (opcode != X86_GROUP1_CMP) {
        ir_emit_set_rm(instructions, &inst->operand_rm, result);
    }
}

void ir_emit_group2_imm(ir_instruction_list_t* instructions, x86_instruction_t* inst) {
    x86_group2_e opcode = inst->operand_reg.reg.ref - X86_REF_RAX;

    x86_size_e size_e = inst->operand_rm.size;
    u8 shift_mask = size_e == X86_SIZE_QWORD ? 0x3F : 0x1F;
    u8 shift_amount = inst->operand_imm.immediate.data & shift_mask;
    ir_instruction_t* rm = ir_emit_get_rm(instructions, &inst->operand_rm);
    ir_instruction_t* shift_imm = ir_emit_immediate(instructions, shift_amount);
    ir_instruction_t* result = NULL;
    ir_instruction_t* c = NULL;
    ir_instruction_t* p = NULL;
    ir_instruction_t* a = NULL;
    ir_instruction_t* z = NULL;
    ir_instruction_t* s = NULL;
    ir_instruction_t* o = NULL;

    bool update_pzs = false;

    switch (opcode) {
        case X86_GROUP2_ROL: {
            ir_instruction_t* size = ir_emit_get_size(instructions, size_e);
            ir_instruction_t* shift_mask = ir_emit_sub(instructions, size, ir_emit_immediate(instructions, 1));
            ir_instruction_t* shift_masked = ir_emit_and(instructions, shift_imm, shift_mask);
            result = ir_emit_rotate(instructions, rm, shift_masked, size_e, false);
            c = ir_emit_and(instructions, result, ir_emit_immediate(instructions, 1));

            if (shift_amount == 1) {
                ir_instruction_t* msb = ir_emit_get_sign(instructions, result, size_e);
                o = ir_emit_xor(instructions, c, msb);
            }
            break;
        }
        case X86_GROUP2_ROR: {
            ERROR("Unimplemented");
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
            ir_instruction_t* msb_mask = ir_emit_get_shift_mask_left(instructions, shift_imm, size_e);
            result = ir_emit_shift_left(instructions, rm, shift_imm);
            c = ir_emit_equal(instructions, ir_emit_and(instructions, rm, msb_mask), msb_mask);
            ir_instruction_t* sign = ir_emit_get_sign(instructions, result, size_e);
            o = ir_emit_xor(instructions, c, sign);

            if (shift_amount != 0) {
                update_pzs = true;
            }
            break;
        }
        case X86_GROUP2_SHR: {
            ir_instruction_t* msb_mask = ir_emit_get_shift_mask_right(instructions, shift_imm);
            result = ir_emit_shift_right(instructions, rm, shift_imm);
            c = ir_emit_equal(instructions, ir_emit_and(instructions, rm, msb_mask), msb_mask);
            o = ir_emit_get_sign(instructions, rm, size_e);

            if (shift_amount != 0) {
                update_pzs = true;
            }
            break;
        }
        case X86_GROUP2_SAR: {
            // Shift left to place MSB to bit 63
            ir_instruction_t* shift_left_count = ir_emit_immediate(instructions, 64 - get_bit_size(size_e));
            ir_instruction_t* shifted_left = ir_emit_shift_left(instructions, rm, shift_left_count);
            ir_instruction_t* shift_right = ir_emit_add(instructions, shift_left_count, shift_imm);
            result = ir_emit_shift_right_arithmetic(instructions, shifted_left, shift_right);
            o = ir_emit_immediate(instructions, 0);
            ir_instruction_t* msb_mask = ir_emit_get_shift_mask_right(instructions, shift_imm);
            c = ir_emit_equal(instructions, ir_emit_and(instructions, rm, msb_mask), msb_mask);

            if (shift_amount != 0) {
                update_pzs = true;
            }
            break;
        }
    }

    if (update_pzs) {
        p = ir_emit_get_parity(instructions, result);
        z = ir_emit_get_zero(instructions, result);
        s = ir_emit_get_sign(instructions, result, size_e);
    }

    ir_emit_set_cpazso(instructions, c, p, a, z, s, o);

    ir_emit_set_rm(instructions, &inst->operand_rm, result);
}

void ir_emit_group3(ir_instruction_list_t* instructions, x86_instruction_t* inst) {
    x86_group3_e opcode = inst->operand_reg.reg.ref - X86_REF_RAX;

    x86_size_e size_e = inst->operand_rm.size;
    ir_instruction_t* rm = ir_emit_get_rm(instructions, &inst->operand_rm);
    ir_instruction_t* result = NULL;
    ir_instruction_t* c = NULL;
    ir_instruction_t* p = NULL;
    ir_instruction_t* a = NULL;
    ir_instruction_t* z = NULL;
    ir_instruction_t* s = NULL;
    ir_instruction_t* o = NULL;

    switch (opcode) {
        case X86_GROUP3_TEST:
        case X86_GROUP3_TEST_: {
            ir_instruction_t* imm = ir_emit_immediate_sext(instructions, &inst->operand_imm);
            ir_instruction_t* masked = ir_emit_and(instructions, rm, imm);
            s = ir_emit_get_sign(instructions, masked, size_e);
            z = ir_emit_get_zero(instructions, masked);
            p = ir_emit_get_parity(instructions, masked);
            break;
        }
        case X86_GROUP3_NOT: {
            result = ir_emit_not(instructions, rm);
            break;
        }
        case X86_GROUP3_NEG: {
            ir_instruction_t* zero = ir_emit_immediate(instructions, 0);
            result = ir_emit_sub(instructions, zero, rm);
            z = ir_emit_get_zero(instructions, result);
            c = ir_emit_not(instructions, z);
            s = ir_emit_get_sign(instructions, result, size_e);
            o = ir_emit_get_overflow_sub(instructions, zero, rm, result, size_e);
            a = ir_emit_get_aux_sub(instructions, zero, rm);
            p = ir_emit_get_parity(instructions, result);
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
            x86_ref_e inputs[] = {X86_REF_RAX, X86_REF_RDX};
            x86_ref_e outputs[] = {X86_REF_RAX, X86_REF_RDX};
            ir_emit_hint_inputs(instructions, inputs, 2);
            ir_emit_udiv(instructions, rm);
            ir_emit_hint_outputs(instructions, outputs, 2);
            break;
        }
        case X86_GROUP3_IDIV: {
            ERROR("Unimplemented");
            break;
        }
    }

    ir_emit_set_cpazso(instructions, c, p, a, z, s, o);

    if (result) {
        ir_emit_set_rm(instructions, &inst->operand_rm, result);
    }
}

ir_instruction_t* ir_emit_get_cc(ir_instruction_list_t* instructions, u8 opcode) {
    switch (opcode & 0xF) {
        case 0: return ir_emit_get_flag(instructions, X86_REF_OF);
        case 1: return ir_emit_get_flag_not(instructions, X86_REF_OF);
        case 2: return ir_emit_get_flag(instructions, X86_REF_CF);
        case 3: return ir_emit_get_flag_not(instructions, X86_REF_CF);
        case 4: return ir_emit_get_flag(instructions, X86_REF_ZF);
        case 5: return ir_emit_get_flag_not(instructions, X86_REF_ZF);
        case 6: return ir_emit_or(instructions, ir_emit_get_flag(instructions, X86_REF_CF), ir_emit_get_flag(instructions, X86_REF_ZF));
        case 7: return ir_emit_and(instructions, ir_emit_get_flag_not(instructions, X86_REF_CF), ir_emit_get_flag_not(instructions, X86_REF_ZF));
        case 8: return ir_emit_get_flag(instructions, X86_REF_SF);
        case 9: return ir_emit_get_flag_not(instructions, X86_REF_SF);
        case 10: return ir_emit_get_flag(instructions, X86_REF_PF);
        case 11: return ir_emit_get_flag_not(instructions, X86_REF_PF);
        case 12: return ir_emit_not_equal(instructions, ir_emit_get_flag(instructions, X86_REF_SF), ir_emit_get_flag(instructions, X86_REF_OF));
        case 13: return ir_emit_equal(instructions, ir_emit_get_flag(instructions, X86_REF_SF), ir_emit_get_flag(instructions, X86_REF_OF));
        case 14: return ir_emit_or(instructions, ir_emit_equal(instructions, ir_emit_get_flag(instructions, X86_REF_ZF), ir_emit_immediate(instructions, 1)), ir_emit_not_equal(instructions, ir_emit_get_flag(instructions, X86_REF_SF), ir_emit_get_flag(instructions, X86_REF_OF)));
        case 15: return ir_emit_and(instructions, ir_emit_equal(instructions, ir_emit_get_flag(instructions, X86_REF_ZF), ir_emit_immediate(instructions, 0)), ir_emit_equal(instructions, ir_emit_get_flag(instructions, X86_REF_SF), ir_emit_get_flag(instructions, X86_REF_OF)));
    }

    ERROR("Invalid condition code");
}

void ir_emit_setcc(ir_instruction_list_t* instructions, x86_instruction_t* inst) {
    ir_emit_set_rm(instructions, &inst->operand_rm, ir_emit_get_cc(instructions, inst->opcode));
}

// void ir_emit_rep_start(ir_instruction_list_t* instructions, x86_size_e size_e) {
//     state->exit = true;

//     x86_operand_t rcx = get_full_reg(X86_REF_RCX);
//     ir_instruction_t* condition = ir_emit_equal(instructions, ir_emit_get_reg(instructions, &rcx), ir_emit_immediate(instructions, 0));
//     ir_instruction_t* jump_address = ir_emit_immediate(instructions, state->current_address + state->current_instruction_length);

//     ir_emit_jump_if_true(instructions, condition, jump_address);
// }

// void ir_emit_rep_end(ir_instruction_list_t* instructions, bool is_nz, x86_size_e size_e) {
//     x86_operand_t rcx_reg = get_full_reg(X86_REF_RCX);
//     rcx_reg.size = size_e;

//     ir_instruction_t* rcx = ir_emit_get_reg(instructions, &rcx_reg);
//     ir_instruction_t* rcx_sub = ir_emit_sub(instructions, rcx, ir_emit_immediate(instructions, 1));
//     ir_emit_set_reg(instructions, &rcx_reg, rcx_sub);

//     ir_instruction_t* zero = ir_emit_immediate(instructions, 0);
//     ir_instruction_t* zf = ir_emit_get_flag(instructions, X86_REF_ZF);
//     ir_instruction_t* condition_exit_rep = is_nz ? ir_emit_not_equal(instructions, zf, zero) : ir_emit_equal(instructions, zf, zero);
//     ir_instruction_t* condition_exit_c = ir_emit_equal(instructions, rcx, zero);
//     ir_instruction_t* condition = ir_emit_or(instructions, condition_exit_rep, condition_exit_c);
//     ir_instruction_t* jump_address_false = ir_emit_immediate(instructions, state->current_address); // repeat
//     ir_instruction_t* jump_address_true = ir_emit_immediate(instructions, state->current_address + state->current_instruction_length); // exit
//     ir_instruction_t* jump = ir_emit_ternary(instructions, condition, jump_address_true, jump_address_false);

//     ir_emit_jump(instructions, jump);
// }