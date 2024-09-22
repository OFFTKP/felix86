#include "felix86/common/global.h"
#include "felix86/common/log.h"
#include "felix86/common/state.h"
#include "felix86/ir/emitter.h"
#include "felix86/ir/handlers.h"
#include "felix86/ir/instruction.h"

u64 sext_if_64(u64 value, x86_size_e size_e) {
    switch (size_e) {
    case X86_SIZE_BYTE:
    case X86_SIZE_WORD:
    case X86_SIZE_DWORD:
        return value;
    case X86_SIZE_QWORD:
        return (i64)(i32)value;
    default:
        ERROR("Invalid immediate size");
    }
}

#define IR_HANDLE(name) void ir_handle_##name(frontend_state_t* state, x86_instruction_t* inst)

IR_HANDLE(error) {
    u64 address = state->current_address - g_base_address;
    if (address & (1ull << 63)) {
        address = state->current_address - g_interpreter_address;
    }
    ERROR("Hit error instruction during: %016lx - Opcode: %02x",
          state->current_address - g_base_address, inst->opcode);
}

// ██████  ██████  ██ ███    ███  █████  ██████  ██    ██
// ██   ██ ██   ██ ██ ████  ████ ██   ██ ██   ██  ██  ██
// ██████  ██████  ██ ██ ████ ██ ███████ ██████    ████
// ██      ██   ██ ██ ██  ██  ██ ██   ██ ██   ██    ██
// ██      ██   ██ ██ ██      ██ ██   ██ ██   ██    ██

IR_HANDLE(add_rm_reg) { // add rm8, r8 - 0x00
    x86_size_e size_e = inst->operand_reg.size;
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* result = ir_emit_add(INSTS, rm, reg);
    ir_emit_set_rm(INSTS, &inst->operand_rm, result);

    ir_instruction_t* c = ir_emit_get_carry_add(INSTS, rm, reg, result, size_e);
    ir_instruction_t* p = ir_emit_get_parity(INSTS, result);
    ir_instruction_t* a = ir_emit_get_aux_add(INSTS, rm, reg);
    ir_instruction_t* z = ir_emit_get_zero(INSTS, result, size_e);
    ir_instruction_t* s = ir_emit_get_sign(INSTS, result, size_e);
    ir_instruction_t* o = ir_emit_get_overflow_add(INSTS, rm, reg, result, size_e);

    ir_emit_set_cpazso(INSTS, c, p, a, z, s, o);
}

IR_HANDLE(add_reg_rm) { // add r16/32/64, rm16/32/64 - 0x03
    x86_size_e size_e = inst->operand_reg.size;
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* result = ir_emit_add(INSTS, reg, rm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, result);

    ir_instruction_t* c = ir_emit_get_carry_add(INSTS, reg, rm, result, size_e);
    ir_instruction_t* p = ir_emit_get_parity(INSTS, result);
    ir_instruction_t* a = ir_emit_get_aux_add(INSTS, reg, rm);
    ir_instruction_t* z = ir_emit_get_zero(INSTS, result, size_e);
    ir_instruction_t* s = ir_emit_get_sign(INSTS, result, size_e);
    ir_instruction_t* o = ir_emit_get_overflow_add(INSTS, reg, rm, result, size_e);

    ir_emit_set_cpazso(INSTS, c, p, a, z, s, o);
}

IR_HANDLE(add_eax_imm) { // add ax/eax/rax, imm16/32/64 - 0x05
    x86_size_e size_e = inst->operand_reg.size;
    ir_instruction_t* eax = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* imm =
        ir_emit_immediate(INSTS, sext_if_64(inst->operand_imm.immediate.data, size_e));
    ir_instruction_t* result = ir_emit_add(INSTS, eax, imm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, result);

    ir_instruction_t* c = ir_emit_get_carry_add(INSTS, eax, imm, result, size_e);
    ir_instruction_t* p = ir_emit_get_parity(INSTS, result);
    ir_instruction_t* a = ir_emit_get_aux_add(INSTS, eax, imm);
    ir_instruction_t* z = ir_emit_get_zero(INSTS, result, size_e);
    ir_instruction_t* s = ir_emit_get_sign(INSTS, result, size_e);
    ir_instruction_t* o = ir_emit_get_overflow_add(INSTS, eax, imm, result, size_e);

    ir_emit_set_cpazso(INSTS, c, p, a, z, s, o);
}

IR_HANDLE(or_rm_reg) { // or rm16/32/64, r16/32/64 - 0x09
    x86_size_e size_e = inst->operand_reg.size;
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* result = ir_emit_or(INSTS, rm, reg);
    ir_emit_set_rm(INSTS, &inst->operand_rm, result);

    ir_instruction_t* zero = ir_emit_immediate(INSTS, 0);
    ir_instruction_t* p = ir_emit_get_parity(INSTS, result);
    ir_instruction_t* z = ir_emit_get_zero(INSTS, result, size_e);
    ir_instruction_t* s = ir_emit_get_sign(INSTS, result, size_e);

    ir_emit_set_cpazso(INSTS, zero, p, NULL, z, s, zero);
}

IR_HANDLE(or_reg_rm) { // or r16/32/64, rm16/32/64 - 0x0B
    x86_size_e size_e = inst->operand_reg.size;
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* result = ir_emit_or(INSTS, reg, rm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, result);

    ir_instruction_t* zero = ir_emit_immediate(INSTS, 0);
    ir_instruction_t* p = ir_emit_get_parity(INSTS, result);
    ir_instruction_t* z = ir_emit_get_zero(INSTS, result, size_e);
    ir_instruction_t* s = ir_emit_get_sign(INSTS, result, size_e);

    ir_emit_set_cpazso(INSTS, zero, p, NULL, z, s, zero);
}

IR_HANDLE(or_eax_imm) { // add ax/eax/rax, imm16/32/64 - 0x0D
    x86_size_e size_e = inst->operand_reg.size;
    ir_instruction_t* eax = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* imm =
        ir_emit_immediate(INSTS, sext_if_64(inst->operand_imm.immediate.data, size_e));
    ir_instruction_t* result = ir_emit_or(INSTS, eax, imm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, result);

    ir_instruction_t* zero = ir_emit_immediate(INSTS, 0);
    ir_instruction_t* p = ir_emit_get_parity(INSTS, result);
    ir_instruction_t* z = ir_emit_get_zero(INSTS, result, size_e);
    ir_instruction_t* s = ir_emit_get_sign(INSTS, result, size_e);

    ir_emit_set_cpazso(INSTS, zero, p, NULL, z, s, zero);
}

IR_HANDLE(and_rm_reg) { // and rm16/32/64, r16/32/64 - 0x21
    x86_size_e size_e = inst->operand_reg.size;
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* result = ir_emit_and(INSTS, rm, reg);
    ir_emit_set_rm(INSTS, &inst->operand_rm, result);

    ir_instruction_t* zero = ir_emit_immediate(INSTS, 0);
    ir_instruction_t* p = ir_emit_get_parity(INSTS, result);
    ir_instruction_t* z = ir_emit_get_zero(INSTS, result, size_e);
    ir_instruction_t* s = ir_emit_get_sign(INSTS, result, size_e);

    ir_emit_set_cpazso(INSTS, zero, p, NULL, z, s, zero);
}

IR_HANDLE(and_eax_imm) { // and ax/eax/rax, imm16/32/64 - 0x25
    x86_size_e size_e = inst->operand_reg.size;
    ir_instruction_t* eax = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* imm =
        ir_emit_immediate(INSTS, sext_if_64(inst->operand_imm.immediate.data, size_e));
    ir_instruction_t* result = ir_emit_and(INSTS, eax, imm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, result);

    ir_instruction_t* zero = ir_emit_immediate(INSTS, 0);
    ir_instruction_t* p = ir_emit_get_parity(INSTS, result);
    ir_instruction_t* z = ir_emit_get_zero(INSTS, result, size_e);
    ir_instruction_t* s = ir_emit_get_sign(INSTS, result, size_e);

    ir_emit_set_cpazso(INSTS, zero, p, NULL, z, s, zero);
}

IR_HANDLE(sub_rm_reg) { // sub rm16/32/64, r16/32/64 - 0x29
    x86_size_e size_e = inst->operand_reg.size;
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* result = ir_emit_sub(INSTS, rm, reg);
    ir_emit_set_rm(INSTS, &inst->operand_rm, result);

    ir_instruction_t* c = ir_emit_get_carry_sub(INSTS, rm, reg, result, size_e);
    ir_instruction_t* p = ir_emit_get_parity(INSTS, result);
    ir_instruction_t* a = ir_emit_get_aux_sub(INSTS, rm, reg);
    ir_instruction_t* z = ir_emit_get_zero(INSTS, result, size_e);
    ir_instruction_t* s = ir_emit_get_sign(INSTS, result, size_e);
    ir_instruction_t* o = ir_emit_get_overflow_sub(INSTS, rm, reg, result, size_e);

    ir_emit_set_cpazso(INSTS, c, p, a, z, s, o);
}

IR_HANDLE(sub_eax_imm) { // sub ax/eax/rax, imm16/32/64 - 0x2d
    x86_size_e size_e = inst->operand_reg.size;
    ir_instruction_t* eax = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* imm =
        ir_emit_immediate(INSTS, sext_if_64(inst->operand_imm.immediate.data, size_e));
    ir_instruction_t* result = ir_emit_sub(INSTS, eax, imm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, result);

    ir_instruction_t* c = ir_emit_get_carry_sub(INSTS, eax, imm, result, size_e);
    ir_instruction_t* p = ir_emit_get_parity(INSTS, result);
    ir_instruction_t* a = ir_emit_get_aux_sub(INSTS, eax, imm);
    ir_instruction_t* z = ir_emit_get_zero(INSTS, result, size_e);
    ir_instruction_t* s = ir_emit_get_sign(INSTS, result, size_e);
    ir_instruction_t* o = ir_emit_get_overflow_sub(INSTS, eax, imm, result, size_e);

    ir_emit_set_cpazso(INSTS, c, p, a, z, s, o);
}

IR_HANDLE(sub_reg_rm) {
    x86_size_e size_e = inst->operand_reg.size;
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* result = ir_emit_sub(INSTS, reg, rm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, result);

    ir_instruction_t* c = ir_emit_get_carry_sub(INSTS, reg, rm, result, size_e);
    ir_instruction_t* p = ir_emit_get_parity(INSTS, result);
    ir_instruction_t* a = ir_emit_get_aux_sub(INSTS, reg, rm);
    ir_instruction_t* z = ir_emit_get_zero(INSTS, result, size_e);
    ir_instruction_t* s = ir_emit_get_sign(INSTS, result, size_e);
    ir_instruction_t* o = ir_emit_get_overflow_sub(INSTS, reg, rm, result, size_e);

    ir_emit_set_cpazso(INSTS, c, p, a, z, s, o);
}

IR_HANDLE(xor_rm_reg) { // xor rm8, r8 - 0x30
    x86_size_e size_e = inst->operand_reg.size;
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* result = ir_emit_xor(INSTS, rm, reg);
    ir_emit_set_rm(INSTS, &inst->operand_rm, result);

    ir_instruction_t* zero = ir_emit_immediate(INSTS, 0);
    ir_instruction_t* p = ir_emit_get_parity(INSTS, result);
    ir_instruction_t* z = ir_emit_get_zero(INSTS, result, size_e);
    ir_instruction_t* s = ir_emit_get_sign(INSTS, result, size_e);

    ir_emit_set_cpazso(INSTS, zero, p, NULL, z, s, zero);
}

IR_HANDLE(xor_reg_rm) { // xor r16/32/64, rm16/32/64 - 0x33
    x86_size_e size_e = inst->operand_reg.size;
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* result = ir_emit_xor(INSTS, reg, rm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, result);

    ir_instruction_t* zero = ir_emit_immediate(INSTS, 0);
    ir_instruction_t* p = ir_emit_get_parity(INSTS, result);
    ir_instruction_t* z = ir_emit_get_zero(INSTS, result, size_e);
    ir_instruction_t* s = ir_emit_get_sign(INSTS, result, size_e);

    ir_emit_set_cpazso(INSTS, zero, p, NULL, z, s, zero);
}

IR_HANDLE(xor_eax_imm) { // xor ax/eax/rax, imm16/32/64 - 0x35
    x86_size_e size_e = inst->operand_reg.size;
    ir_instruction_t* eax = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* imm =
        ir_emit_immediate(INSTS, sext_if_64(inst->operand_imm.immediate.data, size_e));
    ir_instruction_t* result = ir_emit_xor(INSTS, eax, imm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, result);

    ir_instruction_t* zero = ir_emit_immediate(INSTS, 0);
    ir_instruction_t* p = ir_emit_get_parity(INSTS, result);
    ir_instruction_t* z = ir_emit_get_zero(INSTS, result, size_e);
    ir_instruction_t* s = ir_emit_get_sign(INSTS, result, size_e);

    ir_emit_set_cpazso(INSTS, zero, p, NULL, z, s, zero);
}

IR_HANDLE(cmp_rm_reg) { // cmp rm8, r8 - 0x38
    x86_size_e size_e = inst->operand_reg.size;
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* result = ir_emit_sub(INSTS, rm, reg);

    ir_instruction_t* c = ir_emit_get_carry_sub(INSTS, rm, reg, result, size_e);
    ir_instruction_t* p = ir_emit_get_parity(INSTS, result);
    ir_instruction_t* a = ir_emit_get_aux_sub(INSTS, rm, reg);
    ir_instruction_t* z = ir_emit_get_zero(INSTS, result, size_e);
    ir_instruction_t* s = ir_emit_get_sign(INSTS, result, size_e);
    ir_instruction_t* o = ir_emit_get_overflow_sub(INSTS, rm, reg, result, size_e);

    ir_emit_set_cpazso(INSTS, c, p, a, z, s, o);
}

IR_HANDLE(cmp_reg_rm) { // cmp r8, rm8 - 0x3a
    x86_size_e size_e = inst->operand_reg.size;
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* result = ir_emit_sub(INSTS, reg, rm);

    ir_instruction_t* c = ir_emit_get_carry_sub(INSTS, reg, rm, result, size_e);
    ir_instruction_t* p = ir_emit_get_parity(INSTS, result);
    ir_instruction_t* a = ir_emit_get_aux_sub(INSTS, reg, rm);
    ir_instruction_t* z = ir_emit_get_zero(INSTS, result, size_e);
    ir_instruction_t* s = ir_emit_get_sign(INSTS, result, size_e);
    ir_instruction_t* o = ir_emit_get_overflow_sub(INSTS, reg, rm, result, size_e);

    ir_emit_set_cpazso(INSTS, c, p, a, z, s, o);
}

IR_HANDLE(cmp_eax_imm) { // cmp eax, imm32 - 0x3d
    x86_size_e size_e = inst->operand_reg.size;
    ir_instruction_t* eax = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* imm =
        ir_emit_immediate(INSTS, sext_if_64(inst->operand_imm.immediate.data, size_e));
    ir_instruction_t* result = ir_emit_sub(INSTS, eax, imm);

    ir_instruction_t* c = ir_emit_get_carry_sub(INSTS, eax, imm, result, size_e);
    ir_instruction_t* p = ir_emit_get_parity(INSTS, result);
    ir_instruction_t* a = ir_emit_get_aux_sub(INSTS, eax, imm);
    ir_instruction_t* z = ir_emit_get_zero(INSTS, result, size_e);
    ir_instruction_t* s = ir_emit_get_sign(INSTS, result, size_e);
    ir_instruction_t* o = ir_emit_get_overflow_sub(INSTS, eax, imm, result, size_e);

    ir_emit_set_cpazso(INSTS, c, p, a, z, s, o);
}

IR_HANDLE(push_r64) { // push r16/64 - 0x50-0x57
    bool is_word = inst->operand_reg.size == X86_SIZE_WORD;
    x86_operand_t rsp_reg = get_full_reg(X86_REF_RSP);
    ir_instruction_t* rsp = ir_emit_get_reg(INSTS, &rsp_reg);
    ir_instruction_t* size = ir_emit_immediate(INSTS, is_word ? 2 : 8);
    ir_instruction_t* rsp_sub = ir_emit_sub(INSTS, rsp, size);
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    if (is_word == X86_SIZE_WORD) {
        ir_emit_write_word(INSTS, rsp_sub, reg);
    } else {
        ir_emit_write_qword(INSTS, rsp_sub, reg);
    }
    ir_emit_set_reg(INSTS, &rsp_reg, rsp_sub);
}

IR_HANDLE(pop_r64) { // pop r16/64 - 0x58-0x5f
    bool is_word = inst->operand_reg.size == X86_SIZE_WORD;
    x86_operand_t rsp_reg = get_full_reg(X86_REF_RSP);
    ir_instruction_t* rsp = ir_emit_get_reg(INSTS, &rsp_reg);
    ir_instruction_t* reg;
    if (is_word) {
        reg = ir_emit_read_word(INSTS, rsp);
    } else {
        reg = ir_emit_read_qword(INSTS, rsp);
    }
    ir_instruction_t* size = ir_emit_immediate(INSTS, is_word ? 2 : 8);
    ir_instruction_t* rsp_add = ir_emit_add(INSTS, rsp, size);
    ir_emit_set_reg(INSTS, &inst->operand_reg, reg);
    ir_emit_set_reg(INSTS, &rsp_reg, rsp_add);
}

IR_HANDLE(movsxd) { // movsxd r32/64, rm32/64 - 0x63
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* serm = ir_emit_sext32(INSTS, rm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, serm);
}

IR_HANDLE(push_imm8) { // push imm8 - 0x6a
    ir_instruction_t* imm = ir_emit_immediate_sext(INSTS, &inst->operand_imm);
    x86_operand_t rsp_reg = get_full_reg(X86_REF_RSP);
    ir_instruction_t* rsp = ir_emit_get_reg(INSTS, &rsp_reg);
    ir_instruction_t* rsp_sub = ir_emit_sub(INSTS, rsp, ir_emit_immediate(INSTS, 8));
    ir_emit_write_byte(INSTS, rsp_sub, imm);
    ir_emit_set_reg(INSTS, &rsp_reg, rsp_sub);
}

IR_HANDLE(jcc_rel) { // jcc rel8 - 0x70-0x7f
    u8 inst_length = inst->length;
    ir_instruction_t* imm = ir_emit_immediate_sext(INSTS, &inst->operand_imm);
    ir_instruction_t* condition = ir_emit_get_cc(INSTS, inst->opcode);
    u64 jump_address_false = state->current_address + inst_length;
    u64 jump_address_true = state->current_address + inst_length + imm->load_immediate.immediate;

    ir_block_t* block_true =
        ir_function_get_block(state->function, state->current_block, jump_address_true);
    ir_block_t* block_false =
        ir_function_get_block(state->function, state->current_block, jump_address_false);
    ir_emit_jump_conditional(INSTS, condition, block_true, block_false);
    state->exit = true;

    frontend_compile_block(state->function, block_true);
    frontend_compile_block(state->function, block_false);
}

IR_HANDLE(group1_rm8_imm8) { // add/or/adc/sbb/and/sub/xor/cmp rm8, imm8 - 0x80
    ir_emit_group1_imm(INSTS, inst);
}

IR_HANDLE(group1_rm32_imm32) { // add/or/adc/sbb/and/sub/xor/cmp rm16/32/64,
                               // imm16/32/64 - 0x81
    ir_emit_group1_imm(INSTS, inst);
}

IR_HANDLE(group1_rm32_imm8) { // add/or/adc/sbb/and/sub/xor/cmp rm16/32/64, imm8
                              // - 0x83
    ir_emit_group1_imm(INSTS, inst);
}

IR_HANDLE(test_rm_reg) { // test rm8, r8 - 0x84
    x86_size_e size_e = inst->operand_reg.size;
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* result = ir_emit_and(INSTS, rm, reg);

    ir_instruction_t* zero = ir_emit_immediate(INSTS, 0);
    ir_instruction_t* p = ir_emit_get_parity(INSTS, result);
    ir_instruction_t* z = ir_emit_get_zero(INSTS, result, size_e);
    ir_instruction_t* s = ir_emit_get_sign(INSTS, result, size_e);

    ir_emit_set_cpazso(INSTS, zero, p, NULL, z, s, zero);
}

IR_HANDLE(xchg_rm_reg) { // xchg rm8, r8 - 0x86
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_emit_set_rm(INSTS, &inst->operand_rm, reg);
    ir_emit_set_reg(INSTS, &inst->operand_reg, rm);
}

IR_HANDLE(mov_rm_reg) { // mov rm8/16/32/64, r8/16/32/64 - 0x88
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_emit_set_rm(INSTS, &inst->operand_rm, reg);
}

IR_HANDLE(mov_reg_rm) { // mov r8/16/32/64, rm8/16/32/64 - 0x8a
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, rm);
}

IR_HANDLE(mov_r32_rm32) { // mov r16/32/64, rm16/32/64 - 0x8b
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, rm);
}

IR_HANDLE(lea) { // lea r32/64, m - 0x8d
    ir_instruction_t* address = ir_emit_lea(INSTS, &inst->operand_rm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, address);
}

IR_HANDLE(nop) {} // nop - 0x90

IR_HANDLE(xchg_reg_eax) { // xchg reg, eax - 0x91-0x97
    x86_size_e size_e = inst->operand_reg.size;
    x86_operand_t eax_reg = get_full_reg(X86_REF_RAX);
    eax_reg.size = size_e;

    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* eax = ir_emit_get_reg(INSTS, &eax_reg);
    ir_emit_set_reg(INSTS, &inst->operand_reg, eax);
    ir_emit_set_reg(INSTS, &eax_reg, reg);
}

IR_HANDLE(cwde) { // cbw/cwde/cdqe - 0x98
    x86_size_e size_e = inst->operand_reg.size;
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* sexted = ir_emit_sext(INSTS, reg, size_e);
    ir_emit_set_reg(INSTS, &inst->operand_reg, sexted);
}

IR_HANDLE(cdq) { // cwd/cdq/cqo - 0x99
    x86_size_e size_e = inst->operand_reg.size;
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* sign = ir_emit_get_sign(INSTS, reg, size_e);
    ir_instruction_t* zero = ir_emit_immediate(INSTS, 0);
    ir_instruction_t* condition = ir_emit_not_equal(INSTS, sign, zero);

    // if condition bit is 1, set rdx to all ones, else 0
    ir_instruction_t* mask = ir_emit_sub(INSTS, zero, condition);
    x86_operand_t rdx_reg = get_full_reg(X86_REF_RDX);
    rdx_reg.size = size_e;
    ir_emit_set_reg(INSTS, &rdx_reg, mask);
}

IR_HANDLE(test_eax_imm) { // test eax, imm32 - 0xa9
    x86_size_e size_e = inst->operand_reg.size;
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* imm =
        ir_emit_immediate(INSTS, sext_if_64(inst->operand_imm.immediate.data, size_e));
    ir_instruction_t* result = ir_emit_and(INSTS, reg, imm);

    ir_instruction_t* zero = ir_emit_immediate(INSTS, 0);
    ir_instruction_t* p = ir_emit_get_parity(INSTS, result);
    ir_instruction_t* z = ir_emit_get_zero(INSTS, result, size_e);
    ir_instruction_t* s = ir_emit_get_sign(INSTS, result, size_e);

    ir_emit_set_cpazso(INSTS, zero, p, NULL, z, s, zero);
}

IR_HANDLE(stosd) { // stosd - 0xab
    x86_size_e size_e = inst->operand_reg.size;

    x86_operand_t rdi_reg = get_full_reg(X86_REF_RDI);
    rdi_reg.size = inst->operand_rm.memory.address_override ? X86_SIZE_DWORD : X86_SIZE_QWORD;

    x86_operand_t rax_reg = get_full_reg(X86_REF_RAX);
    rax_reg.size = size_e;

    ir_instruction_t* rdi = ir_emit_get_reg(INSTS, &rdi_reg);
    ir_instruction_t* rax = ir_emit_get_reg(INSTS, &rax_reg);

    ir_emit_write_memory(INSTS, rdi, rax, size_e);

    // Assume DF is 0 for now
    ir_instruction_t* rdi_add =
        ir_emit_add(INSTS, rdi, ir_emit_immediate(INSTS, get_bit_size(size_e) / 8));
    ir_emit_set_reg(INSTS, &rdi_reg, rdi_add);
}

IR_HANDLE(mov_r8_imm8) { // mov r8, imm8 - 0xb0-0xb7
    ir_instruction_t* imm = ir_emit_immediate(INSTS, inst->operand_imm.immediate.data);
    ir_emit_set_reg(INSTS, &inst->operand_reg, imm);
}

IR_HANDLE(mov_r32_imm32) { // mov r16/32/64, imm16/32/64 - 0xb8-0xbf
    ir_instruction_t* imm = ir_emit_immediate(INSTS, inst->operand_imm.immediate.data);
    ir_emit_set_reg(INSTS, &inst->operand_reg, imm);
}

IR_HANDLE(group2_rm8_imm8) { // rol/ror/rcl/rcr/shl/shr/sal/sar rm8, imm8 - 0xc0
    ir_emit_group2(INSTS, inst, ir_emit_immediate(INSTS, inst->operand_imm.immediate.data));
}

IR_HANDLE(group2_rm32_imm8) { // rol/ror/rcl/rcr/shl/shr/sal/sar rm16/32/64,
                              // imm8 - 0xc1
    ir_emit_group2(INSTS, inst, ir_emit_immediate(INSTS, inst->operand_imm.immediate.data));
}

IR_HANDLE(ret) { // ret - 0xc3
    ir_instruction_t* rsp = ir_emit_get_guest(INSTS, X86_REF_RSP);
    ir_instruction_t* size = ir_emit_immediate(INSTS, 8);
    ir_instruction_t* rip = ir_emit_read_qword(INSTS, rsp);
    ir_instruction_t* rsp_add = ir_emit_add(INSTS, rsp, size);
    ir_emit_set_guest(INSTS, X86_REF_RSP, rsp_add);
    ir_emit_set_guest(INSTS, X86_REF_RIP, rip);
    ir_emit_jump(INSTS, state->function->exit);
    ir_add_successor(state->current_block, state->function->exit);
    state->exit = true;
}

IR_HANDLE(mov_rm8_imm8) { // mov rm8, imm8 - 0xc6
    ir_instruction_t* imm = ir_emit_immediate(INSTS, inst->operand_imm.immediate.data);
    ir_emit_set_rm(INSTS, &inst->operand_rm, imm);
}

IR_HANDLE(mov_rm32_imm32) { // mov rm16/32/64, imm16/32/64 - 0xc7
    ir_instruction_t* imm = ir_emit_immediate_sext(INSTS, &inst->operand_imm);
    ir_emit_set_rm(INSTS, &inst->operand_rm, imm);
}

IR_HANDLE(leave) { // leave - 0xc9
    x86_size_e size = inst->operand_reg.size;
    x86_operand_t rbp_reg = get_full_reg(X86_REF_RBP);
    rbp_reg.size = size;

    x86_operand_t rsp_reg = get_full_reg(X86_REF_RSP);
    rsp_reg.size = size;

    ir_instruction_t* rbp = ir_emit_get_reg(INSTS, &rbp_reg);

    ir_instruction_t* popped_value =
        size == X86_SIZE_WORD ? ir_emit_read_word(INSTS, rbp) : ir_emit_read_qword(INSTS, rbp);
    ir_instruction_t* imm = ir_emit_immediate(INSTS, size == X86_SIZE_WORD ? 2 : 8);
    ir_instruction_t* rbp_add = ir_emit_add(INSTS, rbp, imm);

    ir_emit_set_reg(INSTS, &rbp_reg, popped_value);
    ir_emit_set_reg(INSTS, &rsp_reg, rbp_add);
}

IR_HANDLE(group2_rm8_1) { // rol/ror/rcl/rcr/shl/shr/sal/sar rm8, 1 - 0xd0
    ir_emit_group2(INSTS, inst, ir_emit_immediate(INSTS, 1));
}

IR_HANDLE(group2_rm32_1) { // rol/ror/rcl/rcr/shl/shr/sal/sar rm16/32/64, 1 - 0xd1
    ir_emit_group2(INSTS, inst, ir_emit_immediate(INSTS, 1));
}

IR_HANDLE(group2_rm32_cl) { // rol/ror/rcl/rcr/shl/shr/sal/sar rm16/32/64, cl - 0xd3
    x86_operand_t cl_reg = get_full_reg(X86_REF_RCX);
    cl_reg.size = X86_SIZE_BYTE;
    ir_instruction_t* cl = ir_emit_get_reg(INSTS, &cl_reg);
    ir_emit_group2(INSTS, inst, cl);
}

IR_HANDLE(call_rel32) { // call rel32 - 0xe8
    u64 displacement = (i64)(i32)inst->operand_imm.immediate.data;
    u64 jump_address = state->current_address + inst->length + displacement;
    u64 return_address = state->current_address + inst->length;
    ir_instruction_t* rip = ir_emit_immediate(INSTS, jump_address);
    ir_instruction_t* return_rip = ir_emit_immediate(INSTS, return_address);
    ir_instruction_t* rsp = ir_emit_get_guest(INSTS, X86_REF_RSP);
    ir_instruction_t* size = ir_emit_immediate(INSTS, 8);
    ir_instruction_t* rsp_sub = ir_emit_sub(INSTS, rsp, size);
    ir_emit_write_qword(INSTS, rsp_sub, return_rip);
    ir_emit_set_guest(INSTS, X86_REF_RSP, rsp_sub);
    ir_emit_set_guest(INSTS, X86_REF_RIP, rip);
    ir_emit_jump(INSTS, state->function->exit);
    ir_add_successor(state->current_block, state->function->exit);
    state->exit = true;
}

IR_HANDLE(jmp_rel32) { // jmp rel32 - 0xe9
    u64 displacement = (i64)(i32)inst->operand_imm.immediate.data;
    u64 jump_address = state->current_address + inst->length + displacement;

    ir_block_t* block = ir_function_get_block(state->function, state->current_block, jump_address);
    ir_emit_jump(INSTS, block);
    state->exit = true;

    frontend_compile_block(state->function, block);
}

IR_HANDLE(jmp_rel8) { // jmp rel8 - 0xeb
    u64 displacement = (i64)(i8)inst->operand_imm.immediate.data;
    u64 jump_address = state->current_address + inst->length + displacement;

    ir_block_t* block = ir_function_get_block(state->function, state->current_block, jump_address);
    ir_emit_jump(INSTS, block);
    state->exit = true;

    frontend_compile_block(state->function, block);
}

IR_HANDLE(hlt) { // hlt - 0xf4
    ir_emit_jump(INSTS, state->function->exit);
    ir_add_successor(state->current_block, state->function->exit);
    state->exit = true;
}

IR_HANDLE(group3_rm8) { // test/not/neg/mul/imul/div/idiv rm8, imm8 - 0xf6
    ir_emit_group3(INSTS, inst);
}

IR_HANDLE(group3_rm32) { // test/not/neg/mul/imul/div/idiv rm16/32/64, imm32 - 0xf7
    ir_emit_group3(INSTS, inst);
}

IR_HANDLE(stc) { // stc - 0xf9
    ir_instruction_t* one = ir_emit_immediate(INSTS, 1);
    ir_emit_set_flag(INSTS, X86_REF_CF, one);
}

IR_HANDLE(group4) { // inc/dec rm8 - 0xfe
    x86_size_e size_e = inst->operand_reg.size;
    x86_group4_e opcode = inst->operand_reg.reg.ref - X86_REF_RAX;

    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* one = ir_emit_immediate(INSTS, 1);
    ir_instruction_t* result = NULL;
    ir_instruction_t* c = NULL;
    ir_instruction_t* o = NULL;
    ir_instruction_t* a = NULL;

    switch (opcode) {
    case X86_GROUP4_INC: {
        result = ir_emit_add(INSTS, rm, one);
        o = ir_emit_get_overflow_add(INSTS, rm, one, result, size_e);
        a = ir_emit_get_aux_add(INSTS, rm, one);
        break;
    }
    case X86_GROUP4_DEC: {
        result = ir_emit_sub(INSTS, rm, one);
        o = ir_emit_get_overflow_sub(INSTS, rm, one, result, size_e);
        a = ir_emit_get_aux_sub(INSTS, rm, one);
        break;
    }
    default: {
        ERROR("Unknown opcode for group4: %02x", opcode);
        break;
    }
    }

    ir_instruction_t* p = ir_emit_get_parity(INSTS, result);
    ir_instruction_t* z = ir_emit_get_zero(INSTS, result, size_e);
    ir_instruction_t* s = ir_emit_get_sign(INSTS, result, size_e);

    ir_emit_set_cpazso(INSTS, c, p, a, z, s, o);
    ir_emit_set_rm(INSTS, &inst->operand_rm, result);
}

IR_HANDLE(group5) { // inc/dec/call/jmp/push rm32
    x86_group3_e opcode = inst->operand_reg.reg.ref - X86_REF_RAX;
    switch (opcode) {
    case X86_GROUP5_INC: {
        x86_size_e size_e = inst->operand_rm.size;
        ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
        ir_instruction_t* one = ir_emit_immediate(INSTS, 1);
        ir_instruction_t* result = ir_emit_add(INSTS, rm, one);
        ir_instruction_t* o = ir_emit_get_overflow_add(INSTS, rm, one, result, size_e);
        ir_instruction_t* a = ir_emit_get_aux_add(INSTS, rm, one);
        ir_instruction_t* p = ir_emit_get_parity(INSTS, result);
        ir_instruction_t* z = ir_emit_get_zero(INSTS, result, size_e);
        ir_instruction_t* s = ir_emit_get_sign(INSTS, result, size_e);
        ir_emit_set_cpazso(INSTS, NULL, p, a, z, s, o);
        ir_emit_set_rm(INSTS, &inst->operand_rm, result);
        break;
    }
    case X86_GROUP5_DEC: {
        x86_size_e size_e = inst->operand_rm.size;
        ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
        ir_instruction_t* one = ir_emit_immediate(INSTS, 1);
        ir_instruction_t* result = ir_emit_sub(INSTS, rm, one);
        ir_instruction_t* o = ir_emit_get_overflow_sub(INSTS, rm, one, result, size_e);
        ir_instruction_t* a = ir_emit_get_aux_sub(INSTS, rm, one);
        ir_instruction_t* p = ir_emit_get_parity(INSTS, result);
        ir_instruction_t* z = ir_emit_get_zero(INSTS, result, size_e);
        ir_instruction_t* s = ir_emit_get_sign(INSTS, result, size_e);
        ir_emit_set_cpazso(INSTS, NULL, p, a, z, s, o);
        ir_emit_set_rm(INSTS, &inst->operand_rm, result);
        break;
    }
    case X86_GROUP5_CALL: {
        x86_operand_t rm_op = inst->operand_rm;
        rm_op.size = X86_SIZE_QWORD;
        u64 return_address = state->current_address + inst->length;
        ir_instruction_t* rip = ir_emit_get_rm(INSTS, &rm_op);
        ir_instruction_t* return_rip = ir_emit_immediate(INSTS, return_address);
        ir_instruction_t* rsp = ir_emit_get_guest(INSTS, X86_REF_RSP);
        ir_instruction_t* size = ir_emit_immediate(INSTS, 8);
        ir_instruction_t* rsp_sub = ir_emit_sub(INSTS, rsp, size);
        ir_emit_write_qword(INSTS, rsp_sub, return_rip);
        ir_emit_set_guest(INSTS, X86_REF_RSP, rsp_sub);
        ir_emit_set_guest(INSTS, X86_REF_RIP, rip);
        ir_emit_jump(INSTS, state->function->exit);
        ir_add_successor(state->current_block, state->function->exit);
        state->exit = true;
        break;
    }
    case X86_GROUP5_JMP: {
        x86_operand_t rm_op = inst->operand_rm;
        rm_op.size = X86_SIZE_QWORD;
        ir_instruction_t* rm = ir_emit_get_rm(INSTS, &rm_op);
        ir_emit_set_guest(INSTS, X86_REF_RIP, rm);
        ir_emit_jump(INSTS, state->function->exit);
        ir_add_successor(state->current_block, state->function->exit);
        state->exit = true;
        break;
    }
    default: {
        ERROR("Unimplemented group 5 opcode: %02x during %016lx", opcode,
              state->current_address - g_base_address);
        break;
    }
    }
}

// ███████ ███████  ██████  ██████  ███    ██ ██████   █████  ██████  ██    ██
// ██      ██      ██      ██    ██ ████   ██ ██   ██ ██   ██ ██   ██  ██  ██
// ███████ █████   ██      ██    ██ ██ ██  ██ ██   ██ ███████ ██████    ████
//      ██ ██      ██      ██    ██ ██  ██ ██ ██   ██ ██   ██ ██   ██    ██
// ███████ ███████  ██████  ██████  ██   ████ ██████  ██   ██ ██   ██    ██

IR_HANDLE(group7) { // group 7 - 0x0f 0x01
    x86_group1_e opcode = inst->operand_imm.immediate.data;
    modrm_t modrm; // we take it in as an immediate instead of as a modrm because
                   // we don't want to potentially get a SIB too
    modrm.raw = opcode;
    switch (modrm.reg) {
    case 2: {
        if (opcode == 0xD0) { // xgetbv
            // That's probably fine for now
            xcr0_reg_t xcr0 = {0};
            xcr0.x87 = 1;
            xcr0.sse = 1;
            xcr0.avx = 1;
            u32 rax = xcr0.raw;
            u32 rdx = xcr0.raw >> 32;
            x86_operand_t rax_reg = get_full_reg(X86_REF_RAX);
            x86_operand_t rdx_reg = get_full_reg(X86_REF_RDX);
            ir_emit_set_reg(INSTS, &rax_reg, ir_emit_immediate(INSTS, rax));
            ir_emit_set_reg(INSTS, &rdx_reg, ir_emit_immediate(INSTS, rdx));
        } else if (opcode == 0xD1) { // xsetbv
            ERROR("XSETBV instruction not implemented");
        } else {
            ERROR("LGDT instruction not implemented");
        }
        break;
    }
    default: {
        ERROR("Unimplemented group 7 opcode: %02x during %016lx", opcode,
              state->current_address - g_base_address);
        break;
    }
    }
}

IR_HANDLE(syscall) { // syscall - 0x0f 0x05
    x86_ref_e inputs[] = {X86_REF_RAX, X86_REF_RDI, X86_REF_RSI, X86_REF_RDX,
                          X86_REF_R10, X86_REF_R8,  X86_REF_R9};
    x86_ref_e outputs[] = {X86_REF_RAX};
    ir_emit_hint_inputs(INSTS, inputs, 7);
    ir_emit_syscall(INSTS);
    ir_emit_hint_outputs(INSTS, outputs, 1);
}

IR_HANDLE(mov_xmm_xmm128) { // movups/movaps xmm, xmm128 - 0x0f 0x11
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, rm);
}

IR_HANDLE(movhps_xmm_m64) {
    if (inst->operand_rm.type != X86_OP_TYPE_MEMORY) {
        ERROR("movhps xmm, m64 but m64 is not a memory operand");
    }

    ir_instruction_t* xmm = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* m64 = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* xmm_dest =
        ir_emit_insert_integer_to_vector(INSTS, xmm, m64, 1, X86_SIZE_QWORD);
    ir_emit_set_reg(INSTS, &inst->operand_reg, xmm_dest);
}

IR_HANDLE(mov_xmm128_xmm) { // movups/movaps xmm128, xmm - 0x0f 0x29
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_emit_set_rm(INSTS, &inst->operand_rm, reg);
}

IR_HANDLE(rdtsc) { // rdtsc - 0x0f 0x31
    x86_ref_e outputs[] = {X86_REF_RAX, X86_REF_RDX};
    ir_emit_rdtsc(INSTS);
    ir_emit_hint_outputs(INSTS, outputs, 2);
}

IR_HANDLE(cmovcc) { // cmovcc - 0x0f 0x40-0x4f
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* condition = ir_emit_get_cc(INSTS, inst->opcode);
    ir_instruction_t* value = ir_emit_select(INSTS, condition, rm, reg);
    ir_emit_set_reg(INSTS, &inst->operand_reg, value);
}

IR_HANDLE(movq_mm_rm32) { // movq mm, rm32 - 0x0f 0x6e
    ERROR("Unimplemented instruction: movq mm, rm32 - 0x0f 0x6e during %016lx",
          state->current_address - g_base_address);
}

IR_HANDLE(setcc) { // setcc - 0x0f 0x90-0x9f
    ir_emit_setcc(INSTS, inst);
}

IR_HANDLE(cpuid) { // cpuid - 0x0f 0xa2
    x86_ref_e inputs[] = {X86_REF_RAX, X86_REF_RCX};
    x86_ref_e outputs[] = {X86_REF_RAX, X86_REF_RBX, X86_REF_RCX, X86_REF_RDX};
    ir_emit_hint_inputs(INSTS, inputs, 2);
    ir_emit_cpuid(INSTS);
    ir_emit_hint_outputs(INSTS, outputs, 4);
}

IR_HANDLE(bt) { // bt - 0x0f 0xa3
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* mask = ir_emit_immediate(INSTS, get_bit_size(inst->operand_reg.size) - 1);
    ir_instruction_t* shift = ir_emit_and(INSTS, reg, mask);
    ir_instruction_t* bit = ir_emit_shift_left(INSTS, ir_emit_immediate(INSTS, 1), shift);
    ir_instruction_t* result = ir_emit_and(INSTS, rm, bit);
    ir_emit_set_flag(INSTS, X86_REF_CF, ir_emit_equal(INSTS, result, mask));
}

IR_HANDLE(imul_r32_rm32) { // imul r32/64, rm32/64 - 0x0f 0xaf
    x86_size_e size_e = inst->operand_reg.size;
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* result =
        ir_emit_imul(INSTS, ir_emit_sext(INSTS, reg, size_e), ir_emit_sext(INSTS, rm, size_e));
    ir_emit_set_reg(INSTS, &inst->operand_reg, result);
}

IR_HANDLE(cmpxchg) { // cmpxchg - 0x0f 0xb0-0xb1
    x86_size_e size_e = inst->operand_reg.size;
    x86_operand_t eax_reg = get_full_reg(X86_REF_RAX);
    eax_reg.size = size_e;
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* eax = ir_emit_get_reg(INSTS, &eax_reg);
    ir_instruction_t* equal = ir_emit_equal(INSTS, eax, rm);
    ir_instruction_t* new_rm = ir_emit_select(INSTS, equal, reg, rm);
    ir_instruction_t* new_eax = ir_emit_select(INSTS, equal, rm, eax);

    ir_emit_set_reg(INSTS, &eax_reg, new_eax);
    ir_emit_set_rm(INSTS, &inst->operand_rm, new_rm);
    ir_emit_set_flag(INSTS, X86_REF_ZF, equal);
}

IR_HANDLE(movzx_r32_rm8) { // movzx r32/64, rm8 - 0x0f 0xb6
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_emit_set_gpr64(INSTS, inst->operand_reg.reg.ref, rm);
}

IR_HANDLE(movzx_r32_rm16) { // movzx r32/64, rm16 - 0x0f 0xb7
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_emit_set_gpr64(INSTS, inst->operand_reg.reg.ref, rm);
}

IR_HANDLE(bsr) { // bsr - 0x0f 0xbd
    x86_size_e size_e = inst->operand_reg.size;
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* zero = ir_emit_get_zero(INSTS, rm, size_e);
    ir_instruction_t* clz = ir_emit_clz(INSTS, rm);
    // CLZ always deals on 64-bit values, so we need to subtract the result from
    // 63
    ir_instruction_t* sub = ir_emit_sub(INSTS, ir_emit_immediate(INSTS, 63), clz);
    ir_emit_set_reg(INSTS, &inst->operand_reg, sub);
    ir_emit_set_flag(INSTS, X86_REF_ZF, zero);
}

IR_HANDLE(bsf) { // bsf - 0x0f 0xbc
    x86_size_e size_e = inst->operand_reg.size;
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* zero = ir_emit_get_zero(INSTS, rm, size_e);
    ir_instruction_t* ctz = ir_emit_ctz(INSTS, rm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, ctz);
    ir_emit_set_flag(INSTS, X86_REF_ZF, zero);
}

// ███████ ███████  ██████  ██████  ███    ██ ██████   █████  ██████  ██    ██
// ██████   ██████ ██      ██      ██      ██    ██ ████   ██ ██   ██ ██   ██ ██
// ██  ██  ██      ██       ██ ███████ █████   ██      ██    ██ ██ ██  ██ ██ ██
// ███████ ██████    ████       ███████  ███████
//      ██ ██      ██      ██    ██ ██  ██ ██ ██   ██ ██   ██ ██   ██    ██ ██
//      ██ ██    ██
// ███████ ███████  ██████  ██████  ██   ████ ██████  ██   ██ ██   ██    ██
// ██████   ██████

IR_HANDLE(punpcklbw_xmm_xmm128) { // punpcklbw xmm, xmm/m128 - 0x66 0x0f 0x60
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* result = ir_emit_vector_unpack_byte_low(INSTS, reg, rm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, result);
}

IR_HANDLE(punpcklwd_xmm_xmm128) { // punpcklwd xmm, xmm/m128 - 0x66 0x0f 0x61
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* result = ir_emit_vector_unpack_word_low(INSTS, reg, rm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, result);
}

IR_HANDLE(punpckldq_xmm_xmm128) { // punpckldq xmm, xmm/m128 - 0x66 0x0f 0x62
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* result = ir_emit_vector_unpack_dword_low(INSTS, reg, rm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, result);
}

IR_HANDLE(group14_xmm) { // group14 xmm - 0x66 0x0f 0x73
    x86_group14_e opcode = inst->operand_reg.reg.ref - X86_REF_XMM0;
    switch (opcode) {
    case X86_GROUP14_PSRLDQ: {
        ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
        ir_instruction_t* imm = ir_emit_immediate(INSTS, inst->operand_imm.immediate.data);
        ir_instruction_t* shifted = ir_emit_vector_packed_shift_right(INSTS, reg, imm);
        ir_emit_set_reg(INSTS, &inst->operand_reg, shifted);
        break;
    }
    default: {
        ERROR("Unimplemented group 14 opcode: %02x during %016lx", opcode,
              state->current_address - g_base_address);
        break;
    }
    }
}

IR_HANDLE(punpcklqdq_xmm_xmm128) { // punpcklqdq xmm, xmm/m128 - 0x66 0x0f 0x6c
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* result = ir_emit_vector_unpack_qword_low(INSTS, reg, rm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, result);
}

IR_HANDLE(movq_xmm_rm32) { // movq xmm, rm32 - 0x66 0x0f 0x6e
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* vector = ir_emit_vector_from_integer(INSTS, rm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, vector);
}

IR_HANDLE(movdqa_xmm_xmm128) { // movdqa xmm, xmm128 - 0x66 0x0f 0x6f
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, rm);
}

IR_HANDLE(pshufd_xmm_xmm128_cb) { // pshufd xmm, xmm/m128, imm8 - 0x66 0x0f 0x70
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* result =
        ir_emit_vector_packed_shuffle_dword(INSTS, rm, inst->operand_imm.immediate.data);
    ir_emit_set_reg(INSTS, &inst->operand_reg, result);
}

IR_HANDLE(pcmpeqb_xmm_xmm128) { // pcmpeqb xmm, xmm/m128 - 0x66 0x0f 0x74
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* result = ir_emit_vector_packed_compare_eq_byte(INSTS, reg, rm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, result);
}

IR_HANDLE(pcmpeqw_xmm_xmm128) { // pcmpeqw xmm, xmm/m128 - 0x66 0x0f 0x75
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* result = ir_emit_vector_packed_compare_eq_word(INSTS, reg, rm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, result);
}

IR_HANDLE(pcmpeqd_xmm_xmm128) { // pcmpeqd xmm, xmm/m128 - 0x66 0x0f 0x76
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* result = ir_emit_vector_packed_compare_eq_dword(INSTS, reg, rm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, result);
}

IR_HANDLE(movq_rm32_xmm) { // movq rm32, xmm - 0x66 0x0f 0x7e
    ir_instruction_t* xmm = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* rm = ir_emit_integer_from_vector(INSTS, xmm);
    ir_emit_set_rm(INSTS, &inst->operand_rm, rm);
}

IR_HANDLE(paddq_xmm_xmm128) { // paddq xmm, xmm/m128 - 0x66 0x0f 0xd4
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* result = ir_emit_vector_packed_add_qword(INSTS, reg, rm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, result);
}

IR_HANDLE(movq_xmm64_xmm) { // movq xmm64, xmm - 0x66 0x0f 0xd6
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    if (inst->operand_rm.type == X86_OP_TYPE_MEMORY) {
        inst->operand_rm.size = X86_SIZE_QWORD;
    }
    ir_emit_set_rm(INSTS, &inst->operand_rm, reg);
}

IR_HANDLE(pmovmskb_reg_xmm) { // pmovmskb reg, xmm - 0x66 0x0f 0xd7
    ir_instruction_t* xmm = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* result = ir_emit_vector_packed_move_byte_mask(INSTS, xmm);
    ir_emit_set_reg(INSTS, &inst->operand_rm, result);
}

IR_HANDLE(pminub_xmm_xmm128) { // pminub xmm, xmm/m128 - 0x66 0x0f 0xda
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* result = ir_emit_vector_packed_min_byte(INSTS, reg, rm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, result);
}

IR_HANDLE(pand_xmm_xmm128) { // pand xmm, xmm/m128 - 0x66 0x0f 0xdb
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* result = ir_emit_vector_packed_and(INSTS, reg, rm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, result);
}

IR_HANDLE(por_xmm_xmm128) { // por xmm, xmm/m128 - 0x66 0x0f 0xeb
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* result = ir_emit_vector_packed_or(INSTS, reg, rm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, result);
}

IR_HANDLE(pxor_xmm_xmm128) { // pxor xmm, xmm/m128 - 0x66 0x0f 0xef
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* result = ir_emit_vector_packed_xor(INSTS, reg, rm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, result);
}

IR_HANDLE(psubb_xmm_xmm128) { // psubb xmm, xmm/m128 - 0x66 0x0f 0xf8
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* reg = ir_emit_get_reg(INSTS, &inst->operand_reg);
    ir_instruction_t* result = ir_emit_vector_packed_sub_byte(INSTS, reg, rm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, result);
}

// ███████ ███████  ██████  ██████  ███    ██ ██████   █████  ██████  ██    ██
// ███████ ██████ ██      ██      ██      ██    ██ ████   ██ ██   ██ ██   ██ ██
// ██  ██  ██      ██           ██ ███████ █████   ██      ██    ██ ██ ██  ██ ██
// ██ ███████ ██████    ████       █████    █████
//      ██ ██      ██      ██    ██ ██  ██ ██ ██   ██ ██   ██ ██   ██    ██ ██
//      ██
// ███████ ███████  ██████  ██████  ██   ████ ██████  ██   ██ ██   ██    ██ ██
// ███████

// ███████ ███████  ██████  ██████  ███    ██ ██████   █████  ██████  ██    ██
// ███████ ██████ ██      ██      ██      ██    ██ ████   ██ ██   ██ ██   ██ ██
// ██  ██  ██      ██           ██ ███████ █████   ██      ██    ██ ██ ██  ██ ██
// ██ ███████ ██████    ████       █████    █████
//      ██ ██      ██      ██    ██ ██  ██ ██ ██   ██ ██   ██ ██   ██    ██ ██
//      ██
// ███████ ███████  ██████  ██████  ██   ████ ██████  ██   ██ ██   ██    ██ ██
// ██████

IR_HANDLE(movdqu_xmm_xmm128) { // movdqu xmm, xmm128 - 0xf3 0x0f 0x6f
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, rm);
}

IR_HANDLE(movq_xmm_xmm64) { // movq xmm, xmm64 - 0xf3 0x0f 0x7e
    x86_operand_t rm_op = inst->operand_rm;
    ir_instruction_t* integer;
    if (rm_op.type == X86_OP_TYPE_MEMORY) {
        rm_op.size = X86_SIZE_QWORD;
        integer = ir_emit_get_rm(INSTS, &rm_op);
    } else {
        ir_instruction_t* reg = ir_emit_get_reg(INSTS, &rm_op);
        integer = ir_emit_integer_from_vector(INSTS, reg);
    }

    ir_emit_vector_from_integer(INSTS, integer);
    ir_emit_set_reg(INSTS, &inst->operand_reg, integer);
}

// ████████ ███████ ██████  ████████ ██  █████  ██████  ██    ██     ██████
// █████
//    ██    ██      ██   ██    ██    ██ ██   ██ ██   ██  ██  ██           ██ ██
//    ██ ██    █████   ██████     ██    ██ ███████ ██████    ████        █████
//    █████ ██    ██      ██   ██    ██    ██ ██   ██ ██   ██    ██ ██ ██   ██
//    ██    ███████ ██   ██    ██    ██ ██   ██ ██   ██    ██        ██████
//    █████

// ████████ ███████ ██████  ████████ ██  █████  ██████  ██    ██     ██████
// █████       ██████   ██████
//    ██    ██      ██   ██    ██    ██ ██   ██ ██   ██  ██  ██           ██ ██
//    ██     ██       ██ ██    █████   ██████     ██    ██ ███████ ██████ ████
//    █████   █████      ███████  ███████ ██    ██      ██   ██    ██    ██ ██
//    ██ ██   ██    ██             ██ ██   ██     ██    ██ ██    ██ ██ ███████
//    ██   ██    ██    ██ ██   ██ ██   ██    ██        ██████   █████ ██████
//    ██████

// ████████ ███████ ██████  ████████ ██  █████  ██████  ██    ██     ██████
// █████      ███████ ██████
//    ██    ██      ██   ██    ██    ██ ██   ██ ██   ██  ██  ██           ██ ██
//    ██     ██           ██ ██    █████   ██████     ██    ██ ███████ ██████
//    ████        █████   █████      █████    █████ ██    ██      ██   ██    ██
//    ██ ██   ██ ██   ██    ██             ██ ██   ██     ██      ██ ██ ███████
//    ██   ██    ██    ██ ██   ██ ██   ██    ██        ██████   █████      ██
//    ███████

// ████████ ███████ ██████  ████████ ██  █████  ██████  ██    ██     ██████
// █████
//    ██    ██      ██   ██    ██    ██ ██   ██ ██   ██  ██  ██           ██ ██
//    ██ ██    █████   ██████     ██    ██ ███████ ██████    ████        █████
//    ███████ ██    ██      ██   ██    ██    ██ ██   ██ ██   ██    ██ ██ ██   ██
//    ██    ███████ ██   ██    ██    ██ ██   ██ ██   ██    ██        ██████  ██
//    ██

// ████████ ███████ ██████  ████████ ██  █████  ██████  ██    ██     ██████
// █████       ██████   ██████
//    ██    ██      ██   ██    ██    ██ ██   ██ ██   ██  ██  ██           ██ ██
//    ██     ██       ██ ██    █████   ██████     ██    ██ ███████ ██████ ████
//    █████  ███████     ███████  ███████ ██    ██      ██   ██    ██    ██ ██
//    ██ ██   ██    ██             ██ ██   ██     ██    ██ ██    ██ ██ ███████
//    ██   ██    ██    ██ ██   ██ ██   ██    ██        ██████  ██   ██ ██████
//    ██████

IR_HANDLE(pcmpistri_xmm_xmm128_cb) { // pcmpistri xmm, xmm/m128, imm8 - 0x66
                                     // 0x0f 0x3a 0x63
    ir_instruction_t* rm = ir_emit_get_rm(INSTS, &inst->operand_rm);
    ir_instruction_t* imm = ir_emit_immediate(INSTS, inst->operand_imm.immediate.data);
    ir_instruction_t* result = ir_emit_vector_packed_compare_implicit_string_index(INSTS, rm, imm);
    ir_emit_set_reg(INSTS, &inst->operand_reg, result);

    x86_ref_e outputs[] = {X86_REF_RCX, X86_REF_CF, X86_REF_ZF, X86_REF_SF, X86_REF_OF};
    ir_emit_hint_outputs(INSTS, outputs, 5);

    ir_instruction_t* zero = ir_emit_immediate(INSTS, 0);
    ir_emit_set_flag(INSTS, X86_REF_PF, zero);
    ir_emit_set_flag(INSTS, X86_REF_AF, zero);
}