#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "felix86/common/utility.h"

typedef enum : u8 {
	SEGMENT_NONE = 0,
	SEGMENT_FS = 1,
	SEGMENT_GS = 2,
	SEGMENT_FS_GS = 3,
} segment_override_e;

typedef enum : u8 {
	X86_GROUP1_ADD = 0,
	X86_GROUP1_OR = 1,
	X86_GROUP1_ADC = 2,
	X86_GROUP1_SBB = 3,
	X86_GROUP1_AND = 4,
	X86_GROUP1_SUB = 5,
	X86_GROUP1_XOR = 6,
	X86_GROUP1_CMP = 7,
} x86_group1_e;

typedef enum : u8 {
	X86_GROUP2_ROL = 0,
	X86_GROUP2_ROR = 1,
	X86_GROUP2_RCL = 2,
	X86_GROUP2_RCR = 3,
	X86_GROUP2_SHL = 4,
	X86_GROUP2_SHR = 5,
	X86_GROUP2_SAL = 6,
	X86_GROUP2_SAR = 7,
} x86_group2_e;

typedef enum : u8 {
	X86_GROUP3_TEST = 0,
	X86_GROUP3_TEST_ = 1,
	X86_GROUP3_NOT = 2,
	X86_GROUP3_NEG = 3,
	X86_GROUP3_MUL = 4,
	X86_GROUP3_IMUL = 5,
	X86_GROUP3_DIV = 6,
	X86_GROUP3_IDIV = 7,
} x86_group3_e;

typedef enum : u8 {
	X86_REF_RAX,
	X86_REF_RCX,
	X86_REF_RDX,
	X86_REF_RBX,
	X86_REF_RSP,
	X86_REF_RBP,
	X86_REF_RSI,
	X86_REF_RDI,
	X86_REF_R8,
	X86_REF_R9,
	X86_REF_R10,
	X86_REF_R11,
	X86_REF_R12,
	X86_REF_R13,
	X86_REF_R14,
	X86_REF_R15,
	X86_REF_MM0,
	X86_REF_MM1,
	X86_REF_MM2,
	X86_REF_MM3,
	X86_REF_MM4,
	X86_REF_MM5,
	X86_REF_MM6,
	X86_REF_MM7,
	X86_REF_XMM0,
	X86_REF_XMM1,
	X86_REF_XMM2,
	X86_REF_XMM3,
	X86_REF_XMM4,
	X86_REF_XMM5,
	X86_REF_XMM6,
	X86_REF_XMM7,
	X86_REF_XMM8,
	X86_REF_XMM9,
	X86_REF_XMM10,
	X86_REF_XMM11,
	X86_REF_XMM12,
	X86_REF_XMM13,
	X86_REF_XMM14,
	X86_REF_XMM15,
	X86_REF_XMM16,
	X86_REF_XMM17,
	X86_REF_XMM18,
	X86_REF_XMM19,
	X86_REF_XMM20,
	X86_REF_XMM21,
	X86_REF_XMM22,
	X86_REF_XMM23,
	X86_REF_XMM24,
	X86_REF_XMM25,
	X86_REF_XMM26,
	X86_REF_XMM27,
	X86_REF_XMM28,
	X86_REF_XMM29,
	X86_REF_XMM30,
	X86_REF_XMM31,
	X86_REF_RIP,
	X86_REF_FLAGS,
	X86_REF_GS,
	X86_REF_FS,

	X86_REF_COUNT,
} x86_ref_e;

typedef enum : u8 {
	X86_FLAG_CF = 0,
	X86_FLAG_PF = 2,
	X86_FLAG_AF = 4,
	X86_FLAG_ZF = 6,
	X86_FLAG_SF = 7,
	X86_FLAG_OF = 11,
} x86_flag_e;

typedef enum : u8 {
	X86_OP_TYPE_NONE,
	X86_OP_TYPE_MEMORY,
	X86_OP_TYPE_REGISTER,
	X86_OP_TYPE_IMMEDIATE,
} x86_operand_type_e;

typedef enum : u8 {
	X86_SIZE_BYTE,
	X86_SIZE_WORD,
	X86_SIZE_DWORD,
	X86_SIZE_QWORD,
	X86_SIZE_MM,
	X86_SIZE_XMM,
	X86_SIZE_YMM,
	X86_SIZE_ZMM,
} x86_size_e;

typedef union {
	struct {
		u16 rex_w : 1;
		u16 address_override : 1;
		u16 operand_override : 1;
		u16 lock : 1;
		u16 rep_nz_f2 : 1;
		u16 rep_z_f3 : 1;
		u16 segment_override : 2;
		u16 byte_override : 1;
		u16 vex_l : 1;  // 0 for 128-bit, 1 for 256-bit
		u16 vex : 1;    // the presence of vex can mean instructions are treated differently, such
						// as how their top bits are treated
		u16 : 4;
	};

	u16 raw;
} x86_prefixes_t;

typedef struct {
	union {
		struct {
			u64 displacement;
			x86_ref_e base;
			x86_ref_e index;
			u8 scale;
		} memory;

		struct {
			x86_ref_e ref;
			bool high8;
		} reg;

		struct {
			u64 data;
		} immediate;
	};

	x86_size_e size;
	x86_operand_type_e type;
	bool address_override;
} x86_operand_t;

typedef struct {
	x86_operand_t operand_rm;
	x86_operand_t operand_reg;
	x86_operand_t operand_imm;
	x86_operand_t operand_vex;
	u8 opcode;
	u8 length;
} x86_instruction_t;

#ifdef __cplusplus
}
#endif