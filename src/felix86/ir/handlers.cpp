#include "Zydis/Disassembler.h"
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/x86.hpp"
#include "felix86/frontend/frontend.hpp"
#include "felix86/ir/emitter.hpp"
#include "felix86/ir/instruction.hpp"

namespace {
u64 ImmSext(u64 imm, x86_size_e size) {
    i64 value = imm;
    switch (size) {
    case X86_SIZE_BYTE:
        value = (i8)value;
        break;
    case X86_SIZE_WORD:
        value = (i16)value;
        break;
    case X86_SIZE_DWORD:
        value = (i32)value;
        break;
    case X86_SIZE_QWORD:
        break;
    default:
        ERROR("Invalid immediate size");
    }
    return value;
}
} // namespace

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
        return 0;
    }
}

u64 sext(u64 value, x86_size_e size_e) {
    switch (size_e) {
    case X86_SIZE_BYTE:
        return (i64)(i8)value;
    case X86_SIZE_WORD:
        return (i64)(i16)value;
    case X86_SIZE_DWORD:
        return (i64)(i32)value;
    case X86_SIZE_QWORD:
        return value;
    default:
        ERROR("Invalid immediate size");
        return 0;
    }
}

x86_size_e sizedown(x86_size_e size_e) {
    switch (size_e) {
    case X86_SIZE_WORD:
        return X86_SIZE_BYTE;
    case X86_SIZE_DWORD:
        return X86_SIZE_WORD;
    case X86_SIZE_QWORD:
        return X86_SIZE_DWORD;
    default:
        UNREACHABLE();
        return X86_SIZE_BYTE;
    }
}

#define IS_LOCK (inst->operand_rm.type == X86_OP_TYPE_MEMORY && inst->operand_rm.memory.lock)
#define IR_HANDLE(name) void ir_handle_##name(FrontendState* state, IREmitter& ir, x86_instruction_t* inst)

IR_HANDLE(error) {
    ZydisDisassembledInstruction zydis_inst;
    if (ZYAN_SUCCESS(ZydisDisassembleIntel(
            /* machine_mode:    */ ZYDIS_MACHINE_MODE_LONG_64,
            /* runtime_address: */ ir.GetCurrentAddress(),
            /* buffer:          */ (void*)ir.GetCurrentAddress(),
            /* length:          */ 15,
            /* instruction:     */ &zydis_inst))) {
        std::string buffer = fmt::format("{}", zydis_inst.text);
        ERROR("Hit error instruction: %s", buffer.c_str());
    } else {
        ERROR("Hit error instruction and couldn't even disassemble it. Opcode: %02x", inst->opcode);
    }
}

// ██████  ██████  ██ ███    ███  █████  ██████  ██    ██
// ██   ██ ██   ██ ██ ████  ████ ██   ██ ██   ██  ██  ██
// ██████  ██████  ██ ██ ████ ██ ███████ ██████    ████
// ██      ██   ██ ██ ██  ██  ██ ██   ██ ██   ██    ██
// ██      ██   ██ ██ ██      ██ ██   ██ ██   ██    ██

IR_HANDLE(add_rm_reg) { // add rm8, r8 - 0x00
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction *rm, *result;
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);

    if (IS_LOCK) {
        SSAInstruction* address = ir.Lea(inst->operand_rm);
        rm = ir.AmoAdd(address, reg, MemoryOrdering::AqRl, size_e);
        result = ir.Add(rm, reg);
    } else {
        rm = ir.GetRm(inst->operand_rm);
        result = ir.Add(rm, reg);
        ir.SetRm(inst->operand_rm, result);
    }

    SSAInstruction* c = ir.IsCarryAdd(rm, result, size_e);
    SSAInstruction* p = ir.Parity(result);
    SSAInstruction* a = ir.IsAuxAdd(rm, reg);
    SSAInstruction* z = ir.IsZero(result, size_e);
    SSAInstruction* s = ir.IsNegative(result, size_e);
    SSAInstruction* o = ir.IsOverflowAdd(rm, reg, result, size_e);

    ir.SetCPAZSO(c, p, a, z, s, o);
}

IR_HANDLE(add_reg_rm) { // add r16/32/64, rm16/32/64 - 0x03
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* result = ir.Add(reg, rm);
    ir.SetReg(inst->operand_reg, result);

    SSAInstruction* c = ir.IsCarryAdd(reg, result, size_e);
    SSAInstruction* p = ir.Parity(result);
    SSAInstruction* a = ir.IsAuxAdd(reg, rm);
    SSAInstruction* z = ir.IsZero(result, size_e);
    SSAInstruction* s = ir.IsNegative(result, size_e);
    SSAInstruction* o = ir.IsOverflowAdd(reg, rm, result, size_e);

    ir.SetCPAZSO(c, p, a, z, s, o);
}

IR_HANDLE(add_eax_imm) { // add ax/eax/rax, imm16/32/64 - 0x05
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction* eax = ir.GetReg(inst->operand_reg);
    SSAInstruction* imm = ir.Imm(sext_if_64(inst->operand_imm.immediate.data, size_e));
    SSAInstruction* result = ir.Add(eax, imm);
    ir.SetReg(inst->operand_reg, result);

    SSAInstruction* c = ir.IsCarryAdd(eax, result, size_e);
    SSAInstruction* p = ir.Parity(result);
    SSAInstruction* a = ir.IsAuxAdd(eax, imm);
    SSAInstruction* z = ir.IsZero(result, size_e);
    SSAInstruction* s = ir.IsNegative(result, size_e);
    SSAInstruction* o = ir.IsOverflowAdd(eax, imm, result, size_e);

    ir.SetCPAZSO(c, p, a, z, s, o);
}

IR_HANDLE(or_rm_reg) { // or rm16/32/64, r16/32/64 - 0x09
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction *rm, *result;
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);

    if (IS_LOCK) {
        SSAInstruction* address = ir.Lea(inst->operand_rm);
        rm = ir.AmoOr(address, reg, MemoryOrdering::AqRl, size_e);
        result = ir.Or(rm, reg);
    } else {
        rm = ir.GetRm(inst->operand_rm);
        result = ir.Or(rm, reg);
        ir.SetRm(inst->operand_rm, result);
    }

    SSAInstruction* zero = ir.Imm(0);
    SSAInstruction* p = ir.Parity(result);
    SSAInstruction* z = ir.IsZero(result, size_e);
    SSAInstruction* s = ir.IsNegative(result, size_e);

    ir.SetCPAZSO(zero, p, nullptr, z, s, zero);
}

IR_HANDLE(or_reg_rm) { // or r16/32/64, rm16/32/64 - 0x0B
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* result = ir.Or(reg, rm);
    ir.SetReg(inst->operand_reg, result);

    SSAInstruction* zero = ir.Imm(0);
    SSAInstruction* p = ir.Parity(result);
    SSAInstruction* z = ir.IsZero(result, size_e);
    SSAInstruction* s = ir.IsNegative(result, size_e);

    ir.SetCPAZSO(zero, p, nullptr, z, s, zero);
}

IR_HANDLE(or_eax_imm) { // add ax/eax/rax, imm16/32/64 - 0x0D
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction* eax = ir.GetReg(inst->operand_reg);
    SSAInstruction* imm = ir.Imm(sext_if_64(inst->operand_imm.immediate.data, size_e));
    SSAInstruction* result = ir.Or(eax, imm);
    ir.SetReg(inst->operand_reg, result);

    SSAInstruction* zero = ir.Imm(0);
    SSAInstruction* p = ir.Parity(result);
    SSAInstruction* z = ir.IsZero(result, size_e);
    SSAInstruction* s = ir.IsNegative(result, size_e);

    ir.SetCPAZSO(zero, p, nullptr, z, s, zero);
}

IR_HANDLE(and_rm_reg) { // and rm16/32/64, r16/32/64 - 0x21
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction *rm, *result;
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);

    if (IS_LOCK) {
        SSAInstruction* address = ir.Lea(inst->operand_rm);
        rm = ir.AmoAnd(address, reg, MemoryOrdering::AqRl, size_e);
        result = ir.And(rm, reg);
    } else {
        rm = ir.GetRm(inst->operand_rm);
        result = ir.And(rm, reg);
        ir.SetRm(inst->operand_rm, result);
    }

    SSAInstruction* zero = ir.Imm(0);
    SSAInstruction* p = ir.Parity(result);
    SSAInstruction* z = ir.IsZero(result, size_e);
    SSAInstruction* s = ir.IsNegative(result, size_e);

    ir.SetCPAZSO(zero, p, nullptr, z, s, zero);
}

IR_HANDLE(and_reg_rm) { // and r16/32/64, rm16/32/64 - 0x23
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* result = ir.And(reg, rm);
    ir.SetReg(inst->operand_reg, result);

    SSAInstruction* zero = ir.Imm(0);
    SSAInstruction* p = ir.Parity(result);
    SSAInstruction* z = ir.IsZero(result, size_e);
    SSAInstruction* s = ir.IsNegative(result, size_e);

    ir.SetCPAZSO(zero, p, nullptr, z, s, zero);
}

IR_HANDLE(and_eax_imm) { // and ax/eax/rax, imm16/32/64 - 0x25
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction* eax = ir.GetReg(inst->operand_reg);
    SSAInstruction* imm = ir.Imm(sext_if_64(inst->operand_imm.immediate.data, size_e));
    SSAInstruction* result = ir.And(eax, imm);
    ir.SetReg(inst->operand_reg, result);

    SSAInstruction* zero = ir.Imm(0);
    SSAInstruction* p = ir.Parity(result);
    SSAInstruction* z = ir.IsZero(result, size_e);
    SSAInstruction* s = ir.IsNegative(result, size_e);

    ir.SetCPAZSO(zero, p, nullptr, z, s, zero);
}

IR_HANDLE(sub_rm_reg) { // sub rm16/32/64, r16/32/64 - 0x29
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction *rm, *result;
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);

    if (IS_LOCK) {
        SSAInstruction* address = ir.Lea(inst->operand_rm);
        SSAInstruction* neg_reg = ir.Neg(reg);
        rm = ir.AmoAdd(address, neg_reg, MemoryOrdering::AqRl, size_e);
        result = ir.Sub(rm, reg);
    } else {
        rm = ir.GetRm(inst->operand_rm);
        result = ir.Sub(rm, reg);
        ir.SetRm(inst->operand_rm, result);
    }

    SSAInstruction* c = ir.IsCarrySub(rm, reg);
    SSAInstruction* p = ir.Parity(result);
    SSAInstruction* a = ir.IsAuxSub(rm, reg);
    SSAInstruction* z = ir.IsZero(result, size_e);
    SSAInstruction* s = ir.IsNegative(result, size_e);
    SSAInstruction* o = ir.IsOverflowSub(rm, reg, result, size_e);

    ir.SetCPAZSO(c, p, a, z, s, o);
}

IR_HANDLE(sub_reg_rm) {
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* result = ir.Sub(reg, rm);
    ir.SetReg(inst->operand_reg, result);

    SSAInstruction* c = ir.IsCarrySub(reg, rm);
    SSAInstruction* p = ir.Parity(result);
    SSAInstruction* a = ir.IsAuxSub(reg, rm);
    SSAInstruction* z = ir.IsZero(result, size_e);
    SSAInstruction* s = ir.IsNegative(result, size_e);
    SSAInstruction* o = ir.IsOverflowSub(reg, rm, result, size_e);

    ir.SetCPAZSO(c, p, a, z, s, o);
}

IR_HANDLE(sub_eax_imm) { // sub ax/eax/rax, imm16/32/64 - 0x2d
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction* eax = ir.GetReg(inst->operand_reg);
    SSAInstruction* imm = ir.Imm(sext_if_64(inst->operand_imm.immediate.data, size_e));
    SSAInstruction* result = ir.Sub(eax, imm);
    ir.SetReg(inst->operand_reg, result);

    SSAInstruction* c = ir.IsCarrySub(eax, imm);
    SSAInstruction* p = ir.Parity(result);
    SSAInstruction* a = ir.IsAuxSub(eax, imm);
    SSAInstruction* z = ir.IsZero(result, size_e);
    SSAInstruction* s = ir.IsNegative(result, size_e);
    SSAInstruction* o = ir.IsOverflowSub(eax, imm, result, size_e);

    ir.SetCPAZSO(c, p, a, z, s, o);
}

IR_HANDLE(xor_rm_reg) { // xor rm8, r8 - 0x30
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction *rm, *result;
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);

    if (IS_LOCK) {
        SSAInstruction* address = ir.Lea(inst->operand_rm);
        rm = ir.AmoXor(address, reg, MemoryOrdering::AqRl, size_e);
        result = ir.Xor(rm, reg);
    } else {
        rm = ir.GetRm(inst->operand_rm);
        result = ir.Xor(rm, reg);
        ir.SetRm(inst->operand_rm, result);
    }

    SSAInstruction* zero = ir.Imm(0);
    SSAInstruction* p = ir.Parity(result);
    SSAInstruction* z = ir.IsZero(result, size_e);
    SSAInstruction* s = ir.IsNegative(result, size_e);

    ir.SetCPAZSO(zero, p, nullptr, z, s, zero);
}

IR_HANDLE(xor_reg_rm) { // xor r16/32/64, rm16/32/64 - 0x33
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* result = ir.Xor(reg, rm);
    ir.SetReg(inst->operand_reg, result);

    SSAInstruction* zero = ir.Imm(0);
    SSAInstruction* p = ir.Parity(result);
    SSAInstruction* z = ir.IsZero(result, size_e);
    SSAInstruction* s = ir.IsNegative(result, size_e);

    ir.SetCPAZSO(zero, p, nullptr, z, s, zero);
}

IR_HANDLE(xor_eax_imm) { // xor ax/eax/rax, imm16/32/64 - 0x35
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction* eax = ir.GetReg(inst->operand_reg);
    SSAInstruction* imm = ir.Imm(sext_if_64(inst->operand_imm.immediate.data, size_e));
    SSAInstruction* result = ir.Xor(eax, imm);
    ir.SetReg(inst->operand_reg, result);

    SSAInstruction* zero = ir.Imm(0);
    SSAInstruction* p = ir.Parity(result);
    SSAInstruction* z = ir.IsZero(result, size_e);
    SSAInstruction* s = ir.IsNegative(result, size_e);

    ir.SetCPAZSO(zero, p, nullptr, z, s, zero);
}

IR_HANDLE(cmp_rm_reg) { // cmp rm8, r8 - 0x38
    if (IS_LOCK) {
        UNIMPLEMENTED();
    }

    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* result = ir.Sub(rm, reg);

    SSAInstruction* c = ir.IsCarrySub(rm, reg);
    SSAInstruction* p = ir.Parity(result);
    SSAInstruction* a = ir.IsAuxSub(rm, reg);
    SSAInstruction* z = ir.IsZero(result, size_e);
    SSAInstruction* s = ir.IsNegative(result, size_e);
    SSAInstruction* o = ir.IsOverflowSub(rm, reg, result, size_e);

    ir.SetCPAZSO(c, p, a, z, s, o);
}

IR_HANDLE(cmp_reg_rm) { // cmp r8, rm8 - 0x3a
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* result = ir.Sub(reg, rm);

    SSAInstruction* c = ir.IsCarrySub(reg, rm);
    SSAInstruction* p = ir.Parity(result);
    SSAInstruction* a = ir.IsAuxSub(reg, rm);
    SSAInstruction* z = ir.IsZero(result, size_e);
    SSAInstruction* s = ir.IsNegative(result, size_e);
    SSAInstruction* o = ir.IsOverflowSub(reg, rm, result, size_e);

    ir.SetCPAZSO(c, p, a, z, s, o);
}

IR_HANDLE(cmp_eax_imm) { // cmp eax, imm32 - 0x3d
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction* eax = ir.GetReg(inst->operand_reg);
    SSAInstruction* imm = ir.Imm(sext_if_64(inst->operand_imm.immediate.data, size_e));
    SSAInstruction* result = ir.Sub(eax, imm);

    SSAInstruction* c = ir.IsCarrySub(eax, imm);
    SSAInstruction* p = ir.Parity(result);
    SSAInstruction* a = ir.IsAuxSub(eax, imm);
    SSAInstruction* z = ir.IsZero(result, size_e);
    SSAInstruction* s = ir.IsNegative(result, size_e);
    SSAInstruction* o = ir.IsOverflowSub(eax, imm, result, size_e);

    ir.SetCPAZSO(c, p, a, z, s, o);
}

IR_HANDLE(push_r64) { // push r16/64 - 0x50-0x57
    bool is_word = inst->operand_reg.size == X86_SIZE_WORD;
    SSAInstruction* rsp = ir.GetReg(X86_REF_RSP);
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* rsp_sub = ir.Addi(rsp, is_word ? -2 : -8);
    ir.WriteMemory(rsp_sub, reg, is_word ? X86_SIZE_WORD : X86_SIZE_QWORD);
    ir.SetReg(rsp_sub, X86_REF_RSP);
}

IR_HANDLE(pop_r64) { // pop r16/64 - 0x58-0x5f
    bool is_word = inst->operand_reg.size == X86_SIZE_WORD;
    SSAInstruction* rsp = ir.GetReg(X86_REF_RSP);
    SSAInstruction* reg = ir.ReadMemory(rsp, is_word ? X86_SIZE_WORD : X86_SIZE_QWORD);
    SSAInstruction* rsp_add = ir.Addi(rsp, is_word ? 2 : 8);
    ir.SetReg(inst->operand_reg, reg);
    ir.SetReg(rsp_add, X86_REF_RSP);
}

IR_HANDLE(movsxd) { // movsxd r32/64, rm32/64 - 0x63
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* serm = ir.Sext(rm, X86_SIZE_DWORD);
    ir.SetReg(inst->operand_reg, serm);
}

IR_HANDLE(push_imm) {
    bool is_word = inst->operand_reg.size == X86_SIZE_WORD;
    SSAInstruction* imm = ir.Imm(ImmSext(inst->operand_imm.immediate.data, inst->operand_imm.size));
    SSAInstruction* rsp = ir.GetReg(X86_REF_RSP);
    SSAInstruction* rsp_sub = ir.Addi(rsp, is_word ? -2 : -8);
    ir.WriteMemory(rsp_sub, imm, is_word ? X86_SIZE_WORD : X86_SIZE_QWORD);
    ir.SetReg(rsp_sub, X86_REF_RSP);
}

IR_HANDLE(imul_r_rm_imm) {
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* imm = ir.Imm(ImmSext(inst->operand_imm.immediate.data, inst->operand_imm.size));
    SSAInstruction* result = ir.Mul(rm, imm);
    ir.SetReg(inst->operand_reg, result);
}

IR_HANDLE(jcc_rel) { // jcc rel8 - 0x70-0x7f
    x86_size_e size_e = inst->operand_imm.size;
    i64 immediate = sext(inst->operand_imm.immediate.data, size_e);
    SSAInstruction* condition = ir.GetCC(inst->opcode);
    SSAInstruction* condition_mov = ir.Snez(condition);
    u64 jump_address_false = ir.GetCurrentAddress() + inst->length;
    u64 jump_address_true = ir.GetCurrentAddress() + inst->length + immediate;

    IRBlock* block_true = state->function->CreateBlockAt(jump_address_true);
    IRBlock* block_false = state->function->CreateBlockAt(jump_address_false);
    ir.TerminateJumpConditional(condition_mov, block_true, block_false);
    ir.Exit();

    frontend_compile_block(*state->emulator, state->function, block_false);
    frontend_compile_block(*state->emulator, state->function, block_true);
}

IR_HANDLE(group1) { // add/or/adc/sbb/and/sub/xor/cmp
    ir.Group1(inst);
}

IR_HANDLE(test_rm_reg) { // test rm8, r8 - 0x84
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* result = ir.And(rm, reg);

    SSAInstruction* zero = ir.Imm(0);
    SSAInstruction* p = ir.Parity(result);
    SSAInstruction* z = ir.IsZero(result, size_e);
    SSAInstruction* s = ir.IsNegative(result, size_e);

    ir.SetCPAZSO(zero, p, nullptr, z, s, zero);
}

IR_HANDLE(xchg_rm_reg) { // xchg rm8, r8 - 0x86
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    if (inst->operand_rm.type == X86_OP_TYPE_MEMORY && false) {
        SSAInstruction* address = ir.Lea(inst->operand_rm);
        SSAInstruction* swapped_reg = ir.AmoSwap(address, reg, MemoryOrdering::AqRl, inst->operand_reg.size);
        ir.SetReg(inst->operand_reg, swapped_reg);
    } else {
        WARN("Hardcoded non atomic path for xchg, fix me");
        SSAInstruction* rm = ir.GetRm(inst->operand_rm);
        ir.SetRm(inst->operand_rm, reg);
        ir.SetReg(inst->operand_reg, rm);
    }
}

IR_HANDLE(mov_rm_reg) { // mov rm8/16/32/64, r8/16/32/64 - 0x88
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    ir.SetRm(inst->operand_rm, reg);
}

IR_HANDLE(mov_reg_rm) { // mov r8/16/32/64, rm8/16/32/64 - 0x8a
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    ir.SetReg(inst->operand_reg, rm);
}

IR_HANDLE(lea) { // lea r32/64, m - 0x8d
    SSAInstruction* address = ir.Lea(inst->operand_rm);
    ir.SetReg(inst->operand_reg, address);
}

IR_HANDLE(nop) {} // nop - 0x90

IR_HANDLE(xchg_reg_eax) { // xchg reg, eax - 0x91-0x97
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* eax = ir.GetReg(X86_REF_RAX, size_e);
    ir.SetReg(inst->operand_reg, eax);
    ir.SetReg(reg, X86_REF_RAX, size_e);
}

IR_HANDLE(cwde) { // cbw/cwde/cdqe - 0x98
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* sexted = ir.Sext(reg, sizedown(size_e));
    ir.SetReg(sexted, inst->operand_reg.reg.ref, size_e);
}

IR_HANDLE(cdq) { // cwd/cdq/cqo - 0x99
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* sign = ir.IsNegative(reg, size_e);
    SSAInstruction* condition = ir.Snez(sign);

    // if condition bit is 1, set rdx to all ones, else 0
    SSAInstruction* mask = ir.Sub(ir.Imm(0), condition);
    ir.SetReg(mask, X86_REF_RDX, size_e);
}

IR_HANDLE(pushfq) { // pushfq - 0x9c
    bool is_word = inst->operand_reg.size == X86_SIZE_WORD;
    SSAInstruction* flags = ir.GetFlags();
    SSAInstruction* rsp = ir.GetReg(X86_REF_RSP);
    SSAInstruction* rsp_sub = ir.Addi(rsp, is_word ? -2 : -8);
    ir.WriteMemory(rsp_sub, flags, is_word ? X86_SIZE_WORD : X86_SIZE_QWORD);
    ir.SetReg(rsp_sub, X86_REF_RSP, X86_SIZE_QWORD);
}

IR_HANDLE(popfq) { // popfq - 0x9d
    bool is_word = inst->operand_reg.size == X86_SIZE_WORD;
    SSAInstruction* rsp = ir.GetReg(X86_REF_RSP);
    SSAInstruction* rsp_add = ir.Addi(rsp, is_word ? 2 : 8);
    SSAInstruction* flags = ir.ReadMemory(rsp, is_word ? X86_SIZE_WORD : X86_SIZE_QWORD);
    ir.SetFlags(flags);
    ir.SetReg(rsp_add, X86_REF_RSP);
}

IR_HANDLE(lahf) { // lahf - 0x9f
    SSAInstruction* flags = ir.GetFlags();
    ir.SetReg(flags, X86_REF_RAX, X86_SIZE_BYTE, true);
}

IR_HANDLE(sahf) { // sahf - 0x9e
    SSAInstruction* flags = ir.GetReg(X86_REF_RAX, X86_SIZE_BYTE, true);
    SSAInstruction* c = ir.Andi(flags, 1);
    SSAInstruction* p = ir.Andi(ir.Shri(flags, 2), 1);
    SSAInstruction* a = ir.Andi(ir.Shri(flags, 4), 1);
    SSAInstruction* z = ir.Andi(ir.Shri(flags, 6), 1);
    SSAInstruction* s = ir.Andi(ir.Shri(flags, 7), 1);
    ir.SetCPAZSO(c, p, a, z, s, nullptr);
}

IR_HANDLE(mov_eax_moffs) { // mov eax, moffs32 - 0xa1
    SSAInstruction* moffs = ir.ReadMemory(ir.Imm(inst->operand_imm.immediate.data), inst->operand_reg.size);
    ir.SetReg(inst->operand_reg, moffs);
}

IR_HANDLE(mov_moffs_eax) { // mov moffs32, eax - 0xa3
    SSAInstruction* eax = ir.GetReg(inst->operand_reg);
    ir.WriteMemory(ir.Imm(inst->operand_imm.immediate.data), eax, inst->operand_reg.size);
}

IR_HANDLE(test_eax_imm) { // test eax, imm32 - 0xa9
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* imm = ir.Imm(sext_if_64(inst->operand_imm.immediate.data, size_e));
    SSAInstruction* result = ir.And(reg, imm);

    SSAInstruction* zero = ir.Imm(0);
    SSAInstruction* p = ir.Parity(result);
    SSAInstruction* z = ir.IsZero(result, size_e);
    SSAInstruction* s = ir.IsNegative(result, size_e);

    ir.SetCPAZSO(zero, p, nullptr, z, s, zero);
}

IR_HANDLE(stosd) { // stosd - 0xab
    x86_size_e size_e = inst->operand_reg.size;
    x86_size_e address_size = inst->operand_rm.memory.address_override ? X86_SIZE_DWORD : X86_SIZE_QWORD;

    SSAInstruction* rdi = ir.GetReg(X86_REF_RDI, address_size);
    SSAInstruction* rax = ir.GetReg(X86_REF_RAX, size_e);
    ir.WriteMemory(rdi, rax, size_e);

    // Assume DF is 0 for now
    SSAInstruction* rdi_add = ir.Addi(rdi, ir.GetBitSize(size_e) / 8);
    ir.SetReg(rdi_add, X86_REF_RDI, address_size);
}

IR_HANDLE(mov_r8_imm8) { // mov r8, imm8 - 0xb0-0xb7
    SSAInstruction* imm = ir.Imm(inst->operand_imm.immediate.data);
    ir.SetReg(inst->operand_reg, imm);
}

IR_HANDLE(mov_r32_imm32) { // mov r16/32/64, imm16/32/64 - 0xb8-0xbf
    SSAInstruction* imm = ir.Imm(inst->operand_imm.immediate.data);
    ir.SetReg(inst->operand_reg, imm);
}

IR_HANDLE(group2_rm_imm8) { // rol/ror/rcl/rcr/shl/shr/sal/sar rm8, imm8 - 0xc0
    ir.Group2(inst, ir.Imm(inst->operand_imm.immediate.data));
}

IR_HANDLE(group2_rm_1) { // rol/ror/rcl/rcr/shl/shr/sal/sar rm16/32/64, 1 - 0xc1
    ir.Group2(inst, ir.Imm(1));
}

IR_HANDLE(group2_rm_cl) { // rol/ror/rcl/rcr/shl/shr/sal/sar rm16/32/64, cl - 0xc1
    SSAInstruction* cl = ir.GetReg(X86_REF_RCX, X86_SIZE_BYTE);
    ir.Group2(inst, cl);
}

IR_HANDLE(ret_imm) {
    SSAInstruction* imm = ir.Imm(inst->operand_imm.immediate.data + inst->length);
    SSAInstruction* rsp = ir.GetReg(X86_REF_RSP);
    SSAInstruction* rip = ir.ReadMemory(rsp, X86_SIZE_QWORD);
    SSAInstruction* rsp_add = ir.Add(rsp, imm);
    ir.SetReg(rsp_add, X86_REF_RSP);
    ir.SetReg(rip, X86_REF_RIP);
    ir.TerminateJump(state->function->GetExit());
    ir.Exit();
}

IR_HANDLE(ret) { // ret - 0xc3
    SSAInstruction* rsp = ir.GetReg(X86_REF_RSP);
    SSAInstruction* rip = ir.ReadMemory(rsp, X86_SIZE_QWORD);
    SSAInstruction* rsp_add = ir.Addi(rsp, 8);
    ir.SetReg(rsp_add, X86_REF_RSP);
    ir.SetReg(rip, X86_REF_RIP);
    ir.TerminateJump(state->function->GetExit());
    ir.Exit();
}

IR_HANDLE(mov_rm8_imm8) { // mov rm8, imm8 - 0xc6
    SSAInstruction* imm = ir.Imm(inst->operand_imm.immediate.data);
    ir.SetRm(inst->operand_rm, imm);
}

IR_HANDLE(mov_rm32_imm32) { // mov rm16/32/64, imm16/32/64 - 0xc7
    SSAInstruction* imm = ir.Imm(ImmSext(inst->operand_imm.immediate.data, inst->operand_imm.size));
    ir.SetRm(inst->operand_rm, imm);
}

IR_HANDLE(leave) { // leave - 0xc9
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction* rbp = ir.GetReg(X86_REF_RBP, size_e);
    SSAInstruction* popped_value = ir.ReadMemory(rbp, size_e);
    SSAInstruction* rbp_add = ir.Addi(rbp, size_e == X86_SIZE_WORD ? 2 : 8);
    ir.SetReg(popped_value, X86_REF_RBP);
    ir.SetReg(rbp_add, X86_REF_RSP);
}

IR_HANDLE(group2_rm8_1) { // rol/ror/rcl/rcr/shl/shr/sal/sar rm8, 1 - 0xd0
    ir.Group2(inst, ir.Imm(1));
}

IR_HANDLE(group2_rm32_1) { // rol/ror/rcl/rcr/shl/shr/sal/sar rm16/32/64, 1 - 0xd1
    ir.Group2(inst, ir.Imm(1));
}

IR_HANDLE(group2_rm32_cl) { // rol/ror/rcl/rcr/shl/shr/sal/sar rm16/32/64, cl - 0xd3
    SSAInstruction* cl = ir.GetReg(X86_REF_RCX, X86_SIZE_BYTE);
    ir.Group2(inst, cl);
}

IR_HANDLE(call_rel32) { // call rel32 - 0xe8
    u64 displacement = (i64)(i32)inst->operand_imm.immediate.data;
    u64 jump_address = ir.GetCurrentAddress() + inst->length + displacement;
    u64 return_address = ir.GetCurrentAddress() + inst->length;
    SSAInstruction* rip = ir.Imm(jump_address);
    SSAInstruction* return_rip = ir.Imm(return_address);
    SSAInstruction* rsp = ir.GetReg(X86_REF_RSP);
    SSAInstruction* rsp_sub = ir.Addi(rsp, -8);
    ir.WriteMemory(rsp_sub, return_rip, X86_SIZE_QWORD);
    ir.SetReg(rsp_sub, X86_REF_RSP);
    ir.SetReg(rip, X86_REF_RIP);
    ir.TerminateJump(state->function->GetExit());
    ir.Exit();
}

IR_HANDLE(jmp_rel32) { // jmp rel32 - 0xe9
    u64 displacement = (i64)(i32)inst->operand_imm.immediate.data;
    u64 jump_address = ir.GetCurrentAddress() + inst->length + displacement;

    IRBlock* target = state->function->CreateBlockAt(jump_address);
    ir.TerminateJump(target);
    ir.Exit();

    frontend_compile_block(*state->emulator, state->function, target);
}

IR_HANDLE(jmp_rel8) { // jmp rel8 - 0xeb
    u64 displacement = (i64)(i8)inst->operand_imm.immediate.data;
    u64 jump_address = ir.GetCurrentAddress() + inst->length + displacement;

    IRBlock* target = state->function->CreateBlockAt(jump_address);
    ir.TerminateJump(target);
    ir.Exit();

    frontend_compile_block(*state->emulator, state->function, target);
}

IR_HANDLE(hlt) { // hlt - 0xf4
    ir.SetExitReason(EXIT_REASON_HLT);
    ir.TerminateJump(state->function->GetExit());
    ir.Exit();
}

IR_HANDLE(group3) { // test/not/neg/mul/imul/div/idiv rm - 0xf6/0xf7
    ir.Group3(inst);
}

IR_HANDLE(clc) { // clc - 0xf8
    ir.SetFlag(ir.Imm(0), X86_REF_CF);
}

IR_HANDLE(stc) { // stc - 0xf9
    ir.SetFlag(ir.Imm(1), X86_REF_CF);
}

IR_HANDLE(cld) { // cld - 0xfc
    ir.SetFlag(ir.Imm(0), X86_REF_DF);
}

IR_HANDLE(std) { // std - 0xfd
    ir.SetFlag(ir.Imm(1), X86_REF_DF);
}

IR_HANDLE(group4) { // inc/dec rm8 - 0xfe
    x86_size_e size_e = inst->operand_reg.size;
    x86_group4_e opcode = (x86_group4_e)(inst->operand_reg.reg.ref - X86_REF_RAX);

    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* one = ir.Imm(1);
    SSAInstruction* result = nullptr;
    SSAInstruction* c = nullptr;
    SSAInstruction* o = nullptr;
    SSAInstruction* a = nullptr;

    switch (opcode) {
    case X86_GROUP4_INC: {
        result = ir.Addi(rm, 1);
        o = ir.IsOverflowAdd(rm, one, result, size_e);
        a = ir.IsAuxAdd(rm, one);
        break;
    }
    case X86_GROUP4_DEC: {
        result = ir.Addi(rm, -1);
        o = ir.IsOverflowSub(rm, one, result, size_e);
        a = ir.IsAuxSub(rm, one);
        break;
    }
    default: {
        ERROR("Unknown opcode for group4: %02x", opcode);
        break;
    }
    }

    SSAInstruction* p = ir.Parity(result);
    SSAInstruction* z = ir.IsZero(result, size_e);
    SSAInstruction* s = ir.IsNegative(result, size_e);

    ir.SetCPAZSO(c, p, a, z, s, o);
    ir.SetRm(inst->operand_rm, result);
}

IR_HANDLE(group5) { // inc/dec/call/jmp/push rm32 - 0xff
    x86_group5_e opcode = (x86_group5_e)(inst->operand_reg.reg.ref - X86_REF_RAX);
    switch (opcode) {
    case X86_GROUP5_INC: {
        x86_size_e size_e = inst->operand_rm.size;
        SSAInstruction* rm = ir.GetRm(inst->operand_rm);
        SSAInstruction* one = ir.Imm(1);
        SSAInstruction* result = ir.Addi(rm, 1);
        SSAInstruction* o = ir.IsOverflowAdd(rm, one, result, size_e);
        SSAInstruction* a = ir.IsAuxAdd(rm, one);
        SSAInstruction* p = ir.Parity(result);
        SSAInstruction* z = ir.IsZero(result, size_e);
        SSAInstruction* s = ir.IsNegative(result, size_e);
        ir.SetCPAZSO(nullptr, p, a, z, s, o);
        ir.SetRm(inst->operand_rm, result);
        break;
    }
    case X86_GROUP5_DEC: {
        x86_size_e size_e = inst->operand_rm.size;
        SSAInstruction* rm = ir.GetRm(inst->operand_rm);
        SSAInstruction* one = ir.Imm(1);
        SSAInstruction* result = ir.Addi(rm, -1);
        SSAInstruction* o = ir.IsOverflowSub(rm, one, result, size_e);
        SSAInstruction* a = ir.IsAuxSub(rm, one);
        SSAInstruction* p = ir.Parity(result);
        SSAInstruction* z = ir.IsZero(result, size_e);
        SSAInstruction* s = ir.IsNegative(result, size_e);
        ir.SetCPAZSO(nullptr, p, a, z, s, o);
        ir.SetRm(inst->operand_rm, result);
        break;
    }
    case X86_GROUP5_CALL: {
        x86_operand_t rm_op = inst->operand_rm;
        rm_op.size = X86_SIZE_QWORD;
        u64 return_address = ir.GetCurrentAddress() + inst->length;
        SSAInstruction* rip = ir.GetRm(rm_op);
        SSAInstruction* rsp = ir.GetReg(X86_REF_RSP);
        SSAInstruction* return_rip = ir.Imm(return_address);
        SSAInstruction* rsp_sub = ir.Addi(rsp, -8);
        ir.WriteMemory(rsp_sub, return_rip, X86_SIZE_QWORD);
        ir.SetReg(rsp_sub, X86_REF_RSP);
        ir.SetReg(rip, X86_REF_RIP);
        ir.TerminateJump(state->function->GetExit());
        ir.Exit();
        break;
    }
    case X86_GROUP5_JMP: {
        SSAInstruction* rm = ir.GetReg(inst->operand_rm.reg.ref);
        ir.SetReg(rm, X86_REF_RIP);
        ir.TerminateJump(state->function->GetExit());
        ir.Exit();
        break;
    }
    default: {
        ERROR("Unimplemented group 5 opcode: %02x during %016lx", opcode, ir.GetCurrentAddress());
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
    // TODO: this is a mess
    // maybe needs its own table ...
    u8 opcode = inst->operand_imm.immediate.data;
    modrm_t modrm; // we take it in as an immediate instead of as a modrm because
                   // we don't want to potentially get a SIB too
    modrm.raw = opcode;
    switch (modrm.reg) {
    case 2: {
        if (opcode == 0xD0) { // xgetbv
            // That's probably fine for now
            xcr0_reg_t xcr0 = {};
            xcr0.x87 = 1;
            xcr0.sse = 1;
            u32 rax = xcr0.raw;
            u32 rdx = xcr0.raw >> 32;
            ir.SetReg(ir.Imm(rax), X86_REF_RAX);
            ir.SetReg(ir.Imm(rdx), X86_REF_RDX);
            WARN("XGETBV");
        } else if (opcode == 0xD1) { // xsetbv
            ERROR("XSETBV instruction not implemented");
        } else {
            ERROR("LGDT instruction not implemented");
        }
        break;
    }
    default: {
        ERROR("Unimplemented group 7 opcode: %02x during %016lx", opcode, ir.GetCurrentAddress());
        break;
    }
    }
}

IR_HANDLE(syscall) { // syscall - 0x0f 0x05
    ir.Syscall();
}

IR_HANDLE(mov_xmm_xmm128) { // movups/movaps xmm, xmm128 - 0x0f 0x11
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    ir.SetReg(inst->operand_reg, rm);
}

IR_HANDLE(movhps_xmm_m64) {
    if (inst->operand_rm.type != X86_OP_TYPE_MEMORY) {
        ERROR("movhps xmm, m64 but m64 is not a memory operand");
    }

    SSAInstruction* xmm = ir.GetReg(inst->operand_reg);
    SSAInstruction* m64 = ir.GetRm(inst->operand_rm);
    SSAInstruction* xmm_dest = ir.VInsertInteger(m64, xmm, 1, X86_SIZE_QWORD);
    ir.SetReg(inst->operand_reg, xmm_dest);
}

IR_HANDLE(mov_xmm128_xmm) { // movups/movaps xmm128, xmm - 0x0f 0x29
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    ir.SetRm(inst->operand_rm, reg);
}

IR_HANDLE(rdtsc) { // rdtsc - 0x0f 0x31
    ir.Rdtsc();
}

IR_HANDLE(cmovcc) { // cmovcc - 0x0f 0x40-0x4f
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* condition = ir.GetCC(inst->opcode);
    SSAInstruction* value = ir.Select(condition, rm, reg);
    ir.SetReg(inst->operand_reg, value);
}

IR_HANDLE(movq_mm_rm32) { // movq mm, rm32 - 0x0f 0x6e
    ERROR("Unimplemented instruction: movq mm, rm32 - 0x0f 0x6e during %016lx", ir.GetCurrentAddress());
}

IR_HANDLE(setcc) { // setcc - 0x0f 0x90-0x9f
    ir.SetCC(inst);
}

IR_HANDLE(cpuid) { // cpuid - 0x0f 0xa2
    ir.Cpuid();
}

IR_HANDLE(bt) { // bt - 0x0f 0xa3
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* mask = ir.Imm(ir.GetBitSize(inst->operand_reg.size) - 1);
    SSAInstruction* shift = ir.And(reg, mask);
    SSAInstruction* bit = ir.Shl(ir.Imm(1), shift);
    SSAInstruction* result = ir.And(rm, bit);
    ir.SetFlag(ir.Equal(result, mask), X86_REF_CF);
}

IR_HANDLE(imul_r32_rm32) { // imul r32/64, rm32/64 - 0x0f 0xaf
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* result = ir.Mul(ir.Sext(reg, size_e), ir.Sext(rm, size_e));
    ir.SetReg(inst->operand_reg, result);
}

IR_HANDLE(cmpxchg) { // cmpxchg - 0x0f 0xb0-0xb1
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction* eax = ir.GetReg(X86_REF_RAX);

    if (IS_LOCK) {
        SSAInstruction* address = ir.Lea(inst->operand_rm);
        SSAInstruction* reg = ir.GetReg(inst->operand_reg);
        SSAInstruction* actual = ir.AmoCAS(address, eax, reg, MemoryOrdering::AqRl, size_e);

        ir.SetReg(actual, X86_REF_RAX);
        ir.SetFlag(ir.Equal(actual, reg), X86_REF_ZF);
    } else {
        SSAInstruction* rm = ir.GetRm(inst->operand_rm);
        SSAInstruction* reg = ir.GetReg(inst->operand_reg);
        SSAInstruction* equal = ir.Equal(eax, rm);
        SSAInstruction* new_rm = ir.Select(equal, reg, rm);

        ir.SetReg(rm, X86_REF_RAX);
        ir.SetRm(inst->operand_rm, new_rm);
        ir.SetFlag(equal, X86_REF_ZF);
    }
}

IR_HANDLE(movzx_r_rm) { // movzx r32/64, rm16 - 0x0f 0xb7
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    ir.SetReg(rm, inst->operand_reg.reg.ref);
}

IR_HANDLE(bsr) { // bsr - 0x0f 0xbd
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* zero = ir.IsZero(rm, size_e);
    SSAInstruction* clz = ir.Clz(rm);
    // CLZ always deals on 64-bit values, so we need to subtract the result from 63
    // TODO: make clzw and clzh instead
    SSAInstruction* sub = ir.Sub(ir.Imm(63), clz);
    ir.SetReg(inst->operand_reg, sub);
    ir.SetFlag(zero, X86_REF_ZF);
}

IR_HANDLE(bsf) { // bsf - 0x0f 0xbc
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* z = ir.IsZero(rm, size_e);
    SSAInstruction* ctz;
    switch (size_e) {
    case X86_SIZE_QWORD: {
        ctz = ir.Ctz(rm);
        break;
    }
    case X86_SIZE_DWORD: {
        ctz = ir.Ctzw(rm);
        break;
    }
    case X86_SIZE_WORD: {
        ctz = ir.Ctzh(rm);
        break;
    }
    default: {
        ERROR("Unknown size for bsf: %d", size_e);
        return;
    }
    }
    ir.SetReg(inst->operand_reg, ctz);
    ir.SetFlag(z, X86_REF_ZF);
}

// ███████ ███████  ██████  ██████  ███    ██ ██████   █████  ██████  ██    ██      ██████   ██████
// ██      ██      ██      ██    ██ ████   ██ ██   ██ ██   ██ ██   ██  ██  ██      ██       ██
// ███████ █████   ██      ██    ██ ██ ██  ██ ██   ██ ███████ ██████    ████       ███████  ███████
//      ██ ██      ██      ██    ██ ██  ██ ██ ██   ██ ██   ██ ██   ██    ██        ██    ██ ██    ██
// ███████ ███████  ██████  ██████  ██   ████ ██████  ██   ██ ██   ██    ██         ██████   ██████

IR_HANDLE(punpcklbw_xmm_xmm128) { // punpcklbw xmm, xmm/m128 - 0x66 0x0f 0x60
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* result = ir.VUnpackByteLow(reg, rm);
    ir.SetReg(inst->operand_reg, result);
}

IR_HANDLE(punpcklwd_xmm_xmm128) { // punpcklwd xmm, xmm/m128 - 0x66 0x0f 0x61
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* result = ir.VUnpackWordLow(reg, rm);
    ir.SetReg(inst->operand_reg, result);
}

IR_HANDLE(punpckldq_xmm_xmm128) { // punpckldq xmm, xmm/m128 - 0x66 0x0f 0x62
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* result = ir.VUnpackDWordLow(reg, rm);
    ir.SetReg(inst->operand_reg, result);
}

IR_HANDLE(group14_xmm) { // group14 xmm - 0x66 0x0f 0x73
    x86_group14_e opcode = (x86_group14_e)(inst->operand_reg.reg.ref - X86_REF_XMM0);
    switch (opcode) {
    case X86_GROUP14_PSRLDQ: {
        SSAInstruction* reg = ir.GetReg(inst->operand_reg);
        SSAInstruction* imm = ir.Imm(inst->operand_imm.immediate.data);
        SSAInstruction* shifted = ir.VPackedShr(reg, imm);
        ir.SetReg(inst->operand_reg, shifted);
        break;
    }
    default: {
        ERROR("Unimplemented group 14 opcode: %02x during %016lx", opcode, ir.GetCurrentAddress());
        break;
    }
    }
}

IR_HANDLE(punpcklqdq_xmm_xmm128) { // punpcklqdq xmm, xmm/m128 - 0x66 0x0f 0x6c
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* result = ir.VUnpackQWordLow(reg, rm);
    ir.SetReg(inst->operand_reg, result);
}

IR_HANDLE(movq_xmm_rm32) { // movq xmm, rm32 - 0x66 0x0f 0x6e
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* vector = ir.IToV(rm);
    ir.SetReg(inst->operand_reg, vector);
}

IR_HANDLE(movdqa_xmm_xmm128) { // movdqa xmm, xmm128 - 0x66 0x0f 0x6f
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    ir.SetReg(inst->operand_reg, rm);
}

IR_HANDLE(pshufd_xmm_xmm128_cb) { // pshufd xmm, xmm/m128, imm8 - 0x66 0x0f 0x70
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* result = ir.VPackedShuffleDWord(rm, inst->operand_imm.immediate.data);
    ir.SetReg(inst->operand_reg, result);
}

IR_HANDLE(pcmpeqb_xmm_xmm128) { // pcmpeqb xmm, xmm/m128 - 0x66 0x0f 0x74
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* result = ir.VPackedEqualByte(reg, rm);
    ir.SetReg(inst->operand_reg, result);
}

IR_HANDLE(pcmpeqw_xmm_xmm128) { // pcmpeqw xmm, xmm/m128 - 0x66 0x0f 0x75
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* result = ir.VPackedEqualWord(reg, rm);
    ir.SetReg(inst->operand_reg, result);
}

IR_HANDLE(pcmpeqd_xmm_xmm128) { // pcmpeqd xmm, xmm/m128 - 0x66 0x0f 0x76
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* result = ir.VPackedEqualDWord(reg, rm);
    ir.SetReg(inst->operand_reg, result);
}

IR_HANDLE(movq_rm32_xmm) { // movq rm32, xmm - 0x66 0x0f 0x7e
    SSAInstruction* xmm = ir.GetReg(inst->operand_reg);
    SSAInstruction* rm = ir.VToI(xmm);
    ir.SetRm(inst->operand_rm, rm);
}

IR_HANDLE(paddq_xmm_xmm128) { // paddq xmm, xmm/m128 - 0x66 0x0f 0xd4
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* result = ir.VPackedAddQWord(reg, rm);
    ir.SetReg(inst->operand_reg, result);
}

IR_HANDLE(movq_xmm64_xmm) { // movq xmm64, xmm - 0x66 0x0f 0xd6
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    if (inst->operand_rm.type == X86_OP_TYPE_MEMORY) {
        inst->operand_rm.size = X86_SIZE_QWORD;
    }
    ir.SetRm(inst->operand_rm, reg);
}

IR_HANDLE(pmovmskb_reg_xmm) { // pmovmskb reg, xmm - 0x66 0x0f 0xd7
    SSAInstruction* xmm = ir.GetReg(inst->operand_reg);
    SSAInstruction* result = ir.VMoveByteMask(xmm);
    ir.SetReg(inst->operand_rm, result);
}

IR_HANDLE(pminub_xmm_xmm128) { // pminub xmm, xmm/m128 - 0x66 0x0f 0xda
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* result = ir.VPackedMinByte(reg, rm);
    ir.SetReg(inst->operand_reg, result);
}

IR_HANDLE(pand_xmm_xmm128) { // pand xmm, xmm/m128 - 0x66 0x0f 0xdb
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* result = ir.VAnd(reg, rm);
    ir.SetReg(inst->operand_reg, result);
}

IR_HANDLE(por_xmm_xmm128) { // por xmm, xmm/m128 - 0x66 0x0f 0xeb
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* result = ir.VOr(reg, rm);
    ir.SetReg(inst->operand_reg, result);
}

IR_HANDLE(pxor_xmm_xmm128) { // pxor xmm, xmm/m128 - 0x66 0x0f 0xef
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* result = ir.VXor(reg, rm);
    ir.SetReg(inst->operand_reg, result);
}

IR_HANDLE(psubb_xmm_xmm128) { // psubb xmm, xmm/m128 - 0x66 0x0f 0xf8
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* result = ir.VPackedSubByte(reg, rm);
    ir.SetReg(inst->operand_reg, result);
}

// ███████ ███████  ██████  ██████  ███    ██ ██████   █████  ██████  ██    ██     ███████ ██████
// ██      ██      ██      ██    ██ ████   ██ ██   ██ ██   ██ ██   ██  ██  ██      ██           ██
// ███████ █████   ██      ██    ██ ██ ██  ██ ██   ██ ███████ ██████    ████       █████    █████
//      ██ ██      ██      ██    ██ ██  ██ ██ ██   ██ ██   ██ ██   ██    ██        ██      ██
// ███████ ███████  ██████  ██████  ██   ████ ██████  ██   ██ ██   ██    ██        ██      ███████

// ███████ ███████  ██████  ██████  ███    ██ ██████   █████  ██████  ██    ██     ███████ ██████
// ██      ██      ██      ██    ██ ████   ██ ██   ██ ██   ██ ██   ██  ██  ██      ██           ██
// ███████ █████   ██      ██    ██ ██ ██  ██ ██   ██ ███████ ██████    ████       █████    █████
//      ██ ██      ██      ██    ██ ██  ██ ██ ██   ██ ██   ██ ██   ██    ██        ██           ██
// ███████ ███████  ██████  ██████  ██   ████ ██████  ██   ██ ██   ██    ██        ██      ██████

IR_HANDLE(movdqu_xmm_xmm128) { // movdqu xmm, xmm128 - 0xf3 0x0f 0x6f
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    ir.SetReg(inst->operand_reg, rm);
}

IR_HANDLE(movq_xmm_xmm64) { // movq xmm, xmm64 - 0xf3 0x0f 0x7e
    x86_operand_t rm_op = inst->operand_rm;
    SSAInstruction* result;
    if (rm_op.type == X86_OP_TYPE_MEMORY) {
        rm_op.size = X86_SIZE_QWORD;
        result = ir.IToV(ir.GetRm(rm_op));
    } else {
        result = ir.GetReg(rm_op);
    }

    ir.SetReg(inst->operand_reg, result);
}

// ████████ ███████ ██████  ████████ ██  █████  ██████  ██    ██     ██████   █████
//    ██    ██      ██   ██    ██    ██ ██   ██ ██   ██  ██  ██           ██ ██   ██
//    ██    █████   ██████     ██    ██ ███████ ██████    ████        █████   █████
//    ██    ██      ██   ██    ██    ██ ██   ██ ██   ██    ██             ██ ██   ██
//    ██    ███████ ██   ██    ██    ██ ██   ██ ██   ██    ██        ██████   █████

// ████████ ███████ ██████  ████████ ██  █████  ██████  ██    ██      ██████   ██████      ██████   █████
//    ██    ██      ██   ██    ██    ██ ██   ██ ██   ██  ██  ██      ██       ██                ██ ██   ██
//    ██    █████   ██████     ██    ██ ███████ ██████    ████       ███████  ███████       █████   █████
//    ██    ██      ██   ██    ██    ██ ██   ██ ██   ██    ██        ██    ██ ██    ██          ██ ██   ██
//    ██    ███████ ██   ██    ██    ██ ██   ██ ██   ██    ██         ██████   ██████      ██████   █████

// ████████ ███████ ██████  ████████ ██  █████  ██████  ██    ██     ███████ ██████      ██████   █████
//    ██    ██      ██   ██    ██    ██ ██   ██ ██   ██  ██  ██      ██           ██          ██ ██   ██
//    ██    █████   ██████     ██    ██ ███████ ██████    ████       █████    █████       █████   █████
//    ██    ██      ██   ██    ██    ██ ██   ██ ██   ██    ██        ██      ██               ██ ██   ██
//    ██    ███████ ██   ██    ██    ██ ██   ██ ██   ██    ██        ██      ███████     ██████   █████

// ████████ ███████ ██████  ████████ ██  █████  ██████  ██    ██     ██████   █████
//    ██    ██      ██   ██    ██    ██ ██   ██ ██   ██  ██  ██           ██ ██   ██
//    ██    █████   ██████     ██    ██ ███████ ██████    ████        █████  ███████
//    ██    ██      ██   ██    ██    ██ ██   ██ ██   ██    ██             ██ ██   ██
//    ██    ███████ ██   ██    ██    ██ ██   ██ ██   ██    ██        ██████  ██   ██

// ████████ ███████ ██████  ████████ ██  █████  ██████  ██    ██      ██████   ██████      ██████   █████
//    ██    ██      ██   ██    ██    ██ ██   ██ ██   ██  ██  ██      ██       ██                ██ ██   ██
//    ██    █████   ██████     ██    ██ ███████ ██████    ████       ███████  ███████       █████  ███████
//    ██    ██      ██   ██    ██    ██ ██   ██ ██   ██    ██        ██    ██ ██    ██          ██ ██   ██
//    ██    ███████ ██   ██    ██    ██ ██   ██ ██   ██    ██         ██████   ██████      ██████  ██   ██

IR_HANDLE(pcmpistri_xmm_xmm128_cb) { // pcmpistri xmm, xmm/m128, imm8 - 0x66 0x0f 0x3a 0x63
    ERROR("Impl me, output xmm + rcx + flags reg?");
    // SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    // SSAInstruction* imm = ir.Imm(inst->operand_imm.immediate.data);
    // SSAInstruction* result = ir_emit_vector_packed_compare_implicit_string_index(BLOCK, rm, imm);
    // ir.SetReg(inst->operand_reg, result);

    // x86_ref_e outputs[] = {X86_REF_RCX, X86_REF_CF, X86_REF_ZF, X86_REF_SF, X86_REF_OF};
    // ir_emit_hint_outputs(BLOCK, outputs, 5);

    // SSAInstruction* zero = ir.Imm(0);
    // ir.SetFlag(X86_REF_PF, zero);
    // ir.SetFlag(X86_REF_AF, zero);
}

// ████████ ███████ ██████  ████████ ██  █████  ██████  ██    ██     ███████ ██████      ██████   █████
//    ██    ██      ██   ██    ██    ██ ██   ██ ██   ██  ██  ██      ██           ██          ██ ██   ██
//    ██    █████   ██████     ██    ██ ███████ ██████    ████       █████    █████       █████  ███████
//    ██    ██      ██   ██    ██    ██ ██   ██ ██   ██    ██        ██      ██               ██ ██   ██
//    ██    ███████ ██   ██    ██    ██ ██   ██ ██   ██    ██        ██      ███████     ██████  ██   ██

// ████████ ███████ ██████  ████████ ██  █████  ██████  ██    ██     ███████ ██████      ██████   █████
//    ██    ██      ██   ██    ██    ██ ██   ██ ██   ██  ██  ██      ██           ██          ██ ██   ██
//    ██    █████   ██████     ██    ██ ███████ ██████    ████       █████    █████       █████  ███████
//    ██    ██      ██   ██    ██    ██ ██   ██ ██   ██    ██        ██           ██          ██ ██   ██
//    ██    ███████ ██   ██    ██    ██ ██   ██ ██   ██    ██        ██      ██████      ██████  ██   ██
