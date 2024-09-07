#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "felix86/common/utility.h"
#include "felix86/frontend/instruction.h"

typedef enum : u16 {
	IR_NULL,

	IR_START_OF_BLOCK,
	IR_PHI,
	IR_HINT_INPUTS,  // tells the recompiler that the registers listed are used as inputs so they aren't optimized away
	IR_HINT_OUTPUTS, // tells the recompiler that the registers listed are used as outputs so they aren't optimized away

	IR_MOV,
	IR_IMMEDIATE,
	IR_POPCOUNT,
	IR_SEXT_GPR8,
	IR_SEXT_GPR16,
	IR_SEXT_GPR32,
	IR_SYSCALL,
	IR_CPUID,

	IR_GET_GUEST,
	IR_SET_GUEST,

	IR_EXIT,
	IR_JUMP,
	IR_JUMP_CONDITIONAL,
	IR_JUMP_REGISTER,

	IR_ADD,
	IR_SUB,
	IR_UDIV,
	IR_SHIFT_LEFT,
	IR_SHIFT_RIGHT,
	IR_SHIFT_RIGHT_ARITHMETIC,
	IR_LEFT_ROTATE8,
	IR_LEFT_ROTATE16,
	IR_LEFT_ROTATE32,
	IR_LEFT_ROTATE64,
	IR_SELECT,
	IR_AND,
	IR_OR,
	IR_XOR,
	IR_NOT,
	IR_LEA,
	IR_EQUAL,
	IR_NOT_EQUAL,
	IR_GREATER_THAN_SIGNED,
	IR_LESS_THAN_SIGNED,
	IR_GREATER_THAN_UNSIGNED,
	IR_LESS_THAN_UNSIGNED,

	IR_READ_BYTE,
	IR_READ_WORD,
	IR_READ_DWORD,
	IR_READ_QWORD,
	IR_READ_XMMWORD,
	IR_WRITE_BYTE,
	IR_WRITE_WORD,
	IR_WRITE_DWORD,
	IR_WRITE_QWORD,
	IR_WRITE_XMMWORD,

	IR_INSERT_INTEGER_TO_VECTOR,
	IR_EXTRACT_INTEGER_FROM_VECTOR,
	IR_VECTOR_FROM_INTEGER,
	IR_INTEGER_FROM_VECTOR,
	IR_VECTOR_UNPACK_DWORD_LOW,
	IR_VECTOR_PACKED_AND,
} ir_opcode_e;

typedef enum : u8 {
	IR_TYPE_NULL,
	IR_TYPE_LOAD_IMMEDIATE,
	IR_TYPE_NO_OPERANDS,
	IR_TYPE_ONE_OPERAND,
	IR_TYPE_TWO_OPERANDS,
	IR_TYPE_THREE_OPERANDS,
	IR_TYPE_FOUR_OPERANDS,
	IR_TYPE_SIDE_EFFECTS,
	IR_TYPE_GET_GUEST,
	IR_TYPE_SET_GUEST,
	IR_TYPE_JUMP,
	IR_TYPE_JUMP_CONDITIONAL,
	IR_TYPE_PHI,
} ir_type_e;

typedef struct ir_phi_node_s {
	struct ir_block_s* block;
	struct ir_instruction_s* value;
	struct ir_phi_node_s* next;
} ir_phi_node_t;

typedef struct ir_instruction_s {
	union {
		struct {
			struct ir_instruction_s* args[4];
		} operands;

		struct {
			u64 immediate;
		} load_immediate;

		struct {
			x86_ref_e ref;
		} get_guest;

		struct {
			struct ir_instruction_s* source;
			x86_ref_e ref;
		} set_guest;

		struct {
			struct ir_instruction_s* condition;
			struct ir_block_s* target_true;
			struct ir_block_s* target_false;
		} jump_conditional;

		struct {
			struct ir_block_s* target;
		} jump;

		struct {
			ir_phi_node_t* list;
		} phi;

		struct {
			x86_ref_e registers_affected[16];
			u8 count;
		} side_effect;

		u64 raw_data[4];
	};

	u16 uses;
	u16 name;
	ir_type_e type;
	ir_opcode_e opcode;
} ir_instruction_t;

void ir_clear_instruction(ir_instruction_t* instruction);

ir_instruction_t ir_copy_expression(ir_instruction_t* expression);

#ifdef __cplusplus
}
#endif