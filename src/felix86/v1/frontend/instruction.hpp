#pragma once

#include "felix86/common/utility.hpp"
#include "felix86/common/x86.hpp"

struct x86_operand_t {
    union {
        struct {
            u64 displacement;
            x86_ref_e base;
            x86_ref_e index;
            u8 scale;

            struct {
                u8 address_override : 1;
                u8 fs_override : 1;
                u8 gs_override : 1;
                u8 lock : 1;
                u8 : 4;
            };
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
};

typedef struct {
    x86_operand_t operand_rm;
    x86_operand_t operand_reg;
    x86_operand_t operand_imm;
    u8 opcode;
    u8 length;
    u8 modrm;
} x86_instruction_t;
