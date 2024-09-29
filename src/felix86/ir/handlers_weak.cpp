#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/frontend/frontend.hpp"

void __attribute__((weak)) ir_handle_add_rm_reg(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "add_rm_reg", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_add_reg_rm(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "add_reg_rm", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_add_eax_imm(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "add_eax_imm", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_or_rm_reg(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "or_rm_reg", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_or_reg_rm(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "or_reg_rm", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_or_eax_imm(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "or_eax_imm", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_secondary_table(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "secondary_table", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_adc_rm_reg(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "adc_rm_reg", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_adc_reg_rm(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "adc_reg_rm", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_adc_eax_imm(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "adc_eax_imm", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_sbb_rm_reg(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "sbb_rm_reg", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_sbb_reg_rm(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "sbb_reg_rm", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_sbb_eax_imm(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "sbb_eax_imm", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_and_rm_reg(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "and_rm_reg", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_and_reg_rm(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "and_reg_rm", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_and_eax_imm(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "and_eax_imm", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_sub_rm_reg(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "sub_rm_reg", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_sub_reg_rm(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "sub_reg_rm", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_sub_eax_imm(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "sub_eax_imm", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_xor_rm_reg(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "xor_rm_reg", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_xor_reg_rm(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "xor_reg_rm", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_xor_eax_imm(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "xor_eax_imm", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_cmp_rm_reg(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "cmp_rm_reg", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_cmp_reg_rm(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "cmp_reg_rm", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_cmp_eax_imm(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "cmp_eax_imm", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_push_r64(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "push_r64", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_pop_r64(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "pop_r64", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_movsxd(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "movsxd", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_push_imm32(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "push_imm32", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_imul_r32_rm32_imm32(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "imul_r32_rm32_imm32", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_push_imm8(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "push_imm8", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_imul_r32_rm32_imm8(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "imul_r32_rm32_imm8", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_jcc_rel(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "jcc", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_group1_rm8_imm8(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "group1_rm8_imm8", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_group1_rm32_imm32(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "group1_rm32_imm32", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_group1_rm32_imm8(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "group1_rm32_imm8", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_test_rm_reg(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "test_rm_reg", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_xchg_rm_reg(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "xchg_rm_reg", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_mov_rm_reg(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "mov_rm_reg", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_mov_reg_rm(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "mov_reg_rm", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_mov_r32_rm32(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "mov_r32_rm32", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_mov_from_sreg(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "mov_from_sreg", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_lea(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "lea", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_mov_to_sreg(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "mov_to_sreg", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_pop_rm32(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "pop_rm32", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_xchg_reg_eax(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "ir_handle_xchg_reg_eax", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_cwde(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "cwde", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_cdq(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "cdq", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_push_flags(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "push_flags", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_pop_flags(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "pop_flags", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_sahf(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "sahf", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_lahf(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "lahf", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_mov_al_moffs8(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "mov_al_moffs8", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_mov_eax_moffs32(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "mov_eax_moffs32", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_mov_moffs8_al(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "mov_moffs8_al", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_mov_moffs32_eax(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "mov_moffs32_eax", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_movsb(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "movsb", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_movsd(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "movsd", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_cmpsb(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "cmpsb", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_cmpsd(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "cmpsd", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_test_eax_imm(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "test_eax_imm", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_stosb(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "stosb", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_stosd(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "stosd", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_lodsb(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "lodsb", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_lodsd(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "lodsd", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_scasb(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "scasb", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_scasd(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "scasd", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_mov_r8_imm8(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "mov_r8_imm8", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_mov_r32_imm32(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "mov_r32_imm32", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_group2_rm8_imm8(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "group2_rm8_imm8", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_group2_rm32_imm8(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "group2_rm32_imm8", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_ret_imm16(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "ret_imm16", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_ret(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "ret", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_mov_rm8_imm8(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "mov_rm8_imm8", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_mov_rm32_imm32(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "mov_rm32_imm32", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_enter(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "enter", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_leave(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "leave", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_group2_rm8_1(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "group2_rm8_1", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_group2_rm32_1(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "group2_rm32_1", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_group2_rm8_cl(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "group2_rm8_cl", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_group2_rm32_cl(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "group2_rm32_cl", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_loopnz_rel8(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "loopnz_rel8", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_loopz_rel8(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "loopz_rel8", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_loop_rel8(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "loop_rel8", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_jecxz_rel8(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "jecxz_rel8", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_call_rel32(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "call_rel32", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_jmp_rel32(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "jmp_rel32", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_jmp_rel8(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "jmp_rel8", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_hlt(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "hlt", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_group3_rm8(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "group3_rm8", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_group3_rm32(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "group3_rm32", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_clc(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "clc", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_stc(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "stc", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_cld(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "cld", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_std(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "std", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_inc_dec_rm8(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "inc_dec_rm8", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}

void __attribute__((weak)) ir_handle_group5(FrontendState* state, x86_instruction_t* inst) {
    ERROR("STUB: Unimplemented ir handler: %s (%x) during address: %016llx", "group5", inst->opcode,
          (unsigned long long)state->current_address - g_base_address);
}
