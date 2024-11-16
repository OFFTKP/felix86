#include "Zydis/Disassembler.h"
#include "felix86/common/log.hpp"
#include "felix86/common/x86.hpp"
#include "felix86/frontend/frontend.hpp"
#include "felix86/ir/emitter.hpp"
#include "felix86/ir/instruction.hpp"

namespace {
SSAInstruction* felix86_sqrt(IREmitter& ir, SSAInstruction*, SSAInstruction* rm, VectorState state) {
    return ir.VFSqrt(rm, state);
}

SSAInstruction* felix86_rcpsqrt(IREmitter& ir, SSAInstruction* reg, SSAInstruction* rm, VectorState state) {
    SSAInstruction* sqrt = ir.VFSqrt(rm, state);
    SSAInstruction* one = ir.VSplat(ir.Imm(0x3f800000), state);
    return ir.VFDiv(one, sqrt, state);
}

SSAInstruction* felix86_rcp(IREmitter& ir, SSAInstruction* reg, SSAInstruction* rm, VectorState state) {
    SSAInstruction* one = ir.VSplat(ir.Imm(0x3f800000), state);
    return ir.VFDiv(one, rm, state);
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
} // namespace

#define IS_LOCK (inst->operand_rm.type == X86_OP_TYPE_MEMORY && inst->operand_rm.memory.lock)
#define IR_HANDLE(name) void ir_handle_##name(IREmitter& ir, x86_instruction_t* inst)

IR_HANDLE(error) {
    ZydisDisassembledInstruction zydis_inst;
    if (ZYAN_SUCCESS(ZydisDisassembleIntel(
            /* machine_mode:    */ ZYDIS_MACHINE_MODE_LONG_64,
            /* runtime_address: */ ir.GetCurrentAddress(),
            /* buffer:          */ (void*)ir.GetCurrentAddress(),
            /* length:          */ 15,
            /* instruction:     */ &zydis_inst))) {
        std::string buffer = fmt::format("{}", zydis_inst.text);
        ERROR("Hit error instruction: %s (address: %016lx, opcode: %02x)", buffer.c_str(), ir.GetCurrentAddress(), inst->opcode);
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
        rm = ir.AmoAdd(address, reg, size_e);
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
        rm = ir.AmoOr(address, reg, size_e);
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

IR_HANDLE(sbb_rm_reg) { // sbb rm16/32/64, r16/32/64 - 0x19
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction *rm, *result;
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* carry_in = ir.GetFlag(X86_REF_CF);

    if (IS_LOCK) {
        ERROR("Why :(");
        return;
    } else {
        rm = ir.GetRm(inst->operand_rm);
        result = ir.Sub(ir.Sub(rm, reg), carry_in);
        ir.SetRm(inst->operand_rm, result);
    }

    SSAInstruction* c = ir.IsCarrySbb(rm, reg, carry_in, size_e);
    SSAInstruction* p = ir.Parity(result);
    SSAInstruction* a = ir.IsAuxSbb(rm, reg, carry_in);
    SSAInstruction* z = ir.IsZero(result, size_e);
    SSAInstruction* s = ir.IsNegative(result, size_e);
    SSAInstruction* o = ir.IsOverflowSub(rm, reg, result, size_e);

    ir.SetCPAZSO(c, p, a, z, s, o);
}

IR_HANDLE(and_rm_reg) { // and rm16/32/64, r16/32/64 - 0x21
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction *rm, *result;
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);

    if (IS_LOCK) {
        SSAInstruction* address = ir.Lea(inst->operand_rm);
        rm = ir.AmoAnd(address, reg, size_e);
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
        rm = ir.AmoAdd(address, neg_reg, size_e);
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
        rm = ir.AmoXor(address, reg, size_e);
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

IR_HANDLE(movsxb) { // movsx r16/32/64, rm8 - 0x0f be
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* serm = ir.Sext(rm, X86_SIZE_BYTE);
    ir.SetReg(inst->operand_reg, serm);
}

IR_HANDLE(movsxw) {
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* serm = ir.Sext(rm, X86_SIZE_WORD);
    ir.SetReg(inst->operand_reg, serm);
}

IR_HANDLE(push_imm) {
    bool is_word = inst->operand_reg.size == X86_SIZE_WORD;
    SSAInstruction* imm = ir.Imm(sext(inst->operand_imm.immediate.data, inst->operand_imm.size));
    SSAInstruction* rsp = ir.GetReg(X86_REF_RSP);
    SSAInstruction* rsp_sub = ir.Addi(rsp, is_word ? -2 : -8);
    ir.WriteMemory(rsp_sub, imm, is_word ? X86_SIZE_WORD : X86_SIZE_QWORD);
    ir.SetReg(rsp_sub, X86_REF_RSP);
}

IR_HANDLE(imul_r_rm_imm) {
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* imm = ir.Imm(sext(inst->operand_imm.immediate.data, inst->operand_imm.size));
    SSAInstruction* result = ir.Mul(rm, imm);
    ir.SetReg(inst->operand_reg, result);
}

IR_HANDLE(jcc_rel) { // jcc rel8 - 0x70-0x7f
    x86_size_e size_e = inst->operand_imm.size;
    i64 immediate = sext(inst->operand_imm.immediate.data, size_e);
    SSAInstruction* condition = ir.GetCC(inst->opcode);
    SSAInstruction* condition_mov = ir.Snez(condition);
    u64 jump_address_false = ir.GetNextAddress();
    u64 jump_address_true = ir.GetNextAddress() + immediate;

    IRBlock* block_true = ir.CreateBlockAt(jump_address_true);
    IRBlock* block_false = ir.CreateBlockAt(jump_address_false);
    ir.TerminateJumpConditional(condition_mov, block_true, block_false);
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
    if (inst->operand_rm.type == X86_OP_TYPE_MEMORY) {
        SSAInstruction* address = ir.Lea(inst->operand_rm);
        SSAInstruction* swapped_reg = ir.AmoSwap(address, reg, inst->operand_reg.size);
        ir.SetReg(inst->operand_reg, swapped_reg);
    } else {
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
    SSAInstruction* eax = ir.GetReg(X86_REF_RAX, size_e, false);
    ir.SetReg(inst->operand_reg, eax);
    ir.SetReg(reg, X86_REF_RAX, size_e, false);
}

IR_HANDLE(cwde) { // cbw/cwde/cdqe - 0x98
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* sexted = ir.Sext(reg, sizedown(size_e));
    ir.SetReg(sexted, inst->operand_reg.reg.ref, size_e, false);
}

IR_HANDLE(cdq) { // cwd/cdq/cqo - 0x99
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* sign = ir.IsNegative(reg, size_e);
    SSAInstruction* condition = ir.Snez(sign);

    // if condition bit is 1, set rdx to all ones, else 0
    SSAInstruction* mask = ir.Sub(ir.Imm(0), condition);
    ir.SetReg(mask, X86_REF_RDX, size_e, false);
}

IR_HANDLE(pushfq) { // pushfq - 0x9c
    bool is_word = inst->operand_reg.size == X86_SIZE_WORD;
    SSAInstruction* flags = ir.GetFlags();
    SSAInstruction* rsp = ir.GetReg(X86_REF_RSP);
    SSAInstruction* rsp_sub = ir.Addi(rsp, is_word ? -2 : -8);
    ir.WriteMemory(rsp_sub, flags, is_word ? X86_SIZE_WORD : X86_SIZE_QWORD);
    ir.SetReg(rsp_sub, X86_REF_RSP, X86_SIZE_QWORD, false);
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

IR_HANDLE(movs) { // movsb - 0xa4
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction* rsi = ir.GetReg(X86_REF_RSI);
    SSAInstruction* rdi = ir.GetReg(X86_REF_RDI);
    SSAInstruction* rsi_val = ir.ReadMemory(rsi, size_e);
    ir.WriteMemory(rdi, rsi_val, size_e);

    int bit_size = ir.GetBitSize(size_e) / 8;
    SSAInstruction* imm = ir.Select(ir.GetFlag(X86_REF_DF), ir.Imm(-bit_size), ir.Imm(bit_size));
    SSAInstruction* rsi_add = ir.Add(rsi, imm);
    SSAInstruction* rdi_add = ir.Add(rdi, imm);
    ir.SetReg(rsi_add, X86_REF_RSI);
    ir.SetReg(rdi_add, X86_REF_RDI);
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

    SSAInstruction* rdi = ir.GetReg(X86_REF_RDI, address_size, false);
    SSAInstruction* rax = ir.GetReg(X86_REF_RAX, size_e, false);
    ir.WriteMemory(rdi, rax, size_e);

    int bit_size = ir.GetBitSize(size_e) / 8;
    SSAInstruction* imm = ir.Select(ir.GetFlag(X86_REF_DF), ir.Imm(-bit_size), ir.Imm(bit_size));
    SSAInstruction* rdi_add = ir.Add(rdi, imm);
    ir.SetReg(rdi_add, X86_REF_RDI, address_size, false);
}

IR_HANDLE(mov_r8_imm8) { // mov r8, imm8 - 0xb0-0xb7
    SSAInstruction* imm = ir.Imm(inst->operand_imm.immediate.data);
    ir.SetReg(inst->operand_reg, imm);
}

IR_HANDLE(mov_r32_imm32) { // mov r16/32/64, imm16/32/64 - 0xb8-0xbf
    SSAInstruction* imm = ir.Imm(inst->operand_imm.immediate.data);
    ir.SetReg(inst->operand_reg, imm);
}

IR_HANDLE(group2_rm_imm) { // rol/ror/rcl/rcr/shl/shr/sal/sar rm8, imm8 - 0xc0
    ir.Group2(inst, ir.Imm(inst->operand_imm.immediate.data));
}

IR_HANDLE(group2_rm_1) { // rol/ror/rcl/rcr/shl/shr/sal/sar rm16/32/64, 1 - 0xc1
    ir.Group2(inst, ir.Imm(1));
}

IR_HANDLE(group2_rm_cl) { // rol/ror/rcl/rcr/shl/shr/sal/sar rm16/32/64, cl - 0xc1
    SSAInstruction* cl = ir.GetReg(X86_REF_RCX, X86_SIZE_BYTE, false);
    ir.Group2(inst, cl);
}

IR_HANDLE(ret_imm) {
    SSAInstruction* imm = ir.Imm(inst->operand_imm.immediate.data + 8);
    SSAInstruction* rsp = ir.GetReg(X86_REF_RSP);
    SSAInstruction* rip = ir.ReadMemory(rsp, X86_SIZE_QWORD);
    SSAInstruction* rsp_add = ir.Add(rsp, imm);
    ir.SetReg(rsp_add, X86_REF_RSP);
    ir.SetReg(rip, X86_REF_RIP);
    ir.TerminateJump(ir.GetExit());
}

IR_HANDLE(ret) { // ret - 0xc3
    SSAInstruction* rsp = ir.GetReg(X86_REF_RSP);
    SSAInstruction* rip = ir.ReadMemory(rsp, X86_SIZE_QWORD);
    SSAInstruction* rsp_add = ir.Addi(rsp, 8);
    ir.SetReg(rsp_add, X86_REF_RSP);
    ir.SetReg(rip, X86_REF_RIP);
    ir.TerminateJump(ir.GetExit());
}

IR_HANDLE(mov_rm_imm) { // mov rm16/32/64, imm16/32/64 - 0xc7
    SSAInstruction* imm = ir.Imm(sext(inst->operand_imm.immediate.data, inst->operand_imm.size));
    ir.SetRm(inst->operand_rm, imm);
}

IR_HANDLE(leave) { // leave - 0xc9
    x86_size_e size_e = inst->operand_reg.size;
    ASSERT(size_e != X86_SIZE_DWORD); // todo: can this happen? fixme
    SSAInstruction* rbp = ir.GetReg(X86_REF_RBP, size_e, false);
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
    SSAInstruction* cl = ir.GetReg(X86_REF_RCX, X86_SIZE_BYTE, false);
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
    ir.TerminateJump(ir.GetExit());
}

IR_HANDLE(jmp_rel32) { // jmp rel32 - 0xe9
    u64 displacement = (i64)(i32)inst->operand_imm.immediate.data;
    u64 jump_address = ir.GetCurrentAddress() + inst->length + displacement;

    IRBlock* target = ir.CreateBlockAt(jump_address);
    ir.TerminateJump(target);
}

IR_HANDLE(jmp_rel8) { // jmp rel8 - 0xeb
    u64 displacement = (i64)(i8)inst->operand_imm.immediate.data;
    u64 jump_address = ir.GetCurrentAddress() + inst->length + displacement;

    IRBlock* target = ir.CreateBlockAt(jump_address);
    ir.TerminateJump(target);
}

IR_HANDLE(hlt) { // hlt - 0xf4
    ir.SetExitReason(EXIT_REASON_HLT);
    ir.TerminateJump(ir.GetExit());
}

IR_HANDLE(ud2) { // ud2 - 0x0f 0x0b
    ir.SetExitReason(EXIT_REASON_UD2);
    ir.TerminateJump(ir.GetExit());
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
    WARN("Direction flag set to 1");
    ir.SetFlag(ir.Imm(1), X86_REF_DF);
}

IR_HANDLE(group4) { // inc/dec rm8 - 0xfe
    x86_size_e size_e = inst->operand_rm.size;
    Group4 opcode = (Group4)(inst->operand_reg.reg.ref - X86_REF_RAX);

    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* one = ir.Imm(1);
    SSAInstruction* result = nullptr;
    SSAInstruction* o = nullptr;
    SSAInstruction* a = nullptr;

    switch (opcode) {
    case Group4::Inc: {
        result = ir.Addi(rm, 1);
        o = ir.IsOverflowAdd(rm, one, result, size_e);
        a = ir.IsAuxAdd(rm, one);
        break;
    }
    case Group4::Dec: {
        result = ir.Addi(rm, -1);
        o = ir.IsOverflowSub(rm, one, result, size_e);
        a = ir.IsAuxSub(rm, one);
        break;
    }
    default: {
        ERROR("Unknown opcode for group4: %02x", (int)opcode);
        break;
    }
    }

    SSAInstruction* p = ir.Parity(result);
    SSAInstruction* z = ir.IsZero(result, size_e);
    SSAInstruction* s = ir.IsNegative(result, size_e);

    ir.SetCPAZSO(nullptr, p, a, z, s, o);
    ir.SetRm(inst->operand_rm, result);
}

IR_HANDLE(group5) { // inc/dec/call/jmp/push rm32 - 0xff
    Group5 opcode = (Group5)(inst->operand_reg.reg.ref - X86_REF_RAX);
    switch (opcode) {
    case Group5::Inc: {
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
    case Group5::Dec: {
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
    case Group5::Call: {
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
        ir.TerminateJump(ir.GetExit());
        break;
    }
    case Group5::Jmp: {
        x86_operand_t rm_op = inst->operand_rm;
        rm_op.size = X86_SIZE_QWORD;
        SSAInstruction* rm = ir.GetRm(rm_op);
        ir.SetReg(rm, X86_REF_RIP);
        ir.TerminateJump(ir.GetExit());
        break;
    }
    case Group5::Push: {
        bool is_word = inst->operand_rm.size == X86_SIZE_WORD;
        inst->operand_rm.size = is_word ? X86_SIZE_WORD : X86_SIZE_QWORD;
        SSAInstruction* rm = ir.GetRm(inst->operand_rm);
        SSAInstruction* rsp = ir.GetReg(X86_REF_RSP);
        SSAInstruction* rsp_sub = ir.Addi(rsp, is_word ? -2 : -8);
        ir.WriteMemory(rsp_sub, rm, is_word ? X86_SIZE_WORD : X86_SIZE_QWORD);
        ir.SetReg(rsp_sub, X86_REF_RSP);
        break;
    }
    default: {
        ERROR("Unimplemented group 5 opcode: %02x during %016lx", (int)opcode, ir.GetCurrentAddress());
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
            ir.SetReg(ir.Imm(0b11), X86_REF_RAX);
            ir.SetReg(ir.Imm(0), X86_REF_RDX);
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

IR_HANDLE(mov_xmm128_xmm) { // movups/movaps xmm128, xmm - 0x0f 0x29
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    ir.SetRm(inst->operand_rm, reg, VectorState::PackedDWord);
}

IR_HANDLE(mov_xmm_m64) { // movlpd xmm, m64 - 0x0f 0x12
    // Just load a double from memory directly into an xmm - thus using vector loads
    // instead of gpr loads and then moving to vector
    SSAInstruction* old = ir.GetReg(inst->operand_reg);
    SSAInstruction* rm = ir.GetRm(inst->operand_rm, VectorState::Double);
    SSAInstruction* mask = ir.VSplati(0b10, VectorState::PackedQWord);
    ir.SetVMask(mask);
    SSAInstruction* result = ir.VMerge(old, rm, VectorState::PackedQWord);
    ir.SetReg(inst->operand_reg, result);
}

IR_HANDLE(movh_m64_xmm) {
    ASSERT(inst->operand_rm.type == X86_OP_TYPE_MEMORY);
    SSAInstruction* xmm = ir.GetReg(inst->operand_reg);
    SSAInstruction* slide = ir.VSlideDowni(xmm, 1, VectorState::PackedQWord);
    ir.SetRm(inst->operand_rm, slide, VectorState::Double);
}

IR_HANDLE(movh_xmm_m64) { // movhpd xmm, m64 - 0x0f 0x16
    SSAInstruction* rm = ir.GetRm(inst->operand_rm, VectorState::Double);
    SSAInstruction* shifted = ir.VSlideUpi(rm, 1, VectorState::PackedQWord);
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    ir.SetVMask(ir.VSplati(0b10, VectorState::PackedQWord));
    SSAInstruction* result = ir.VMerge(shifted, reg, VectorState::PackedQWord);
    ir.SetReg(inst->operand_reg, result);
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
    // TODO: use B extension bit test whenever possible
    if (inst->operand_rm.type == X86_OP_TYPE_REGISTER) {
        SSAInstruction* rm = ir.GetRm(inst->operand_rm);
        SSAInstruction* reg = ir.GetReg(inst->operand_reg);
        SSAInstruction* shift = ir.Andi(reg, ir.GetBitSize(inst->operand_reg.size) - 1);
        SSAInstruction* bit = ir.Shr(rm, shift);
        SSAInstruction* result = ir.Andi(bit, 1);
        ir.SetFlag(result, X86_REF_CF);
    } else {
        UNREACHABLE();
    }
}

IR_HANDLE(imul_r32_rm32) { // imul r32/64, rm32/64 - 0x0f 0xaf
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* result = ir.Mul(ir.Sext(reg, size_e), ir.Sext(rm, size_e));
    ir.SetReg(inst->operand_reg, result);

    // Check if top bits are not sign extension of bottom bits
    switch (size_e) {
    case X86_SIZE_WORD: {
        SSAInstruction* result_high = ir.Shri(result, 16);
        SSAInstruction* sext = ir.Sext(result, X86_SIZE_WORD);
        SSAInstruction* masked = ir.And(result_high, ir.Imm(0xffff));
        SSAInstruction* sext_masked = ir.And(sext, ir.Imm(0xffff));
        SSAInstruction* not_equal = ir.NotEqual(masked, sext_masked);
        ir.SetFlag(not_equal, X86_REF_OF);
        ir.SetFlag(not_equal, X86_REF_CF);
        break;
    }
    case X86_SIZE_DWORD: {
        SSAInstruction* result_high = ir.Shri(result, 32);
        SSAInstruction* sext = ir.Sext(result, X86_SIZE_DWORD);
        SSAInstruction* masked = ir.And(result_high, ir.Imm(0xffffffff));
        SSAInstruction* sext_masked = ir.And(sext, ir.Imm(0xffffffff));
        SSAInstruction* not_equal = ir.NotEqual(masked, sext_masked);
        ir.SetFlag(not_equal, X86_REF_OF);
        ir.SetFlag(not_equal, X86_REF_CF);
        break;
    }
    case X86_SIZE_QWORD: {
        SSAInstruction* result_high = ir.Mulh(reg, rm);
        SSAInstruction* sext = ir.Sari(result, 63);
        SSAInstruction* not_equal = ir.NotEqual(result_high, sext);
        ir.SetFlag(not_equal, X86_REF_OF);
        ir.SetFlag(not_equal, X86_REF_CF);
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }
}

IR_HANDLE(cmpxchg) { // cmpxchg - 0x0f 0xb0-0xb1
    x86_size_e size_e = inst->operand_reg.size;

    if (inst->operand_rm.type == X86_OP_TYPE_MEMORY) {
        IRBlock* next_instruction_target = ir.CreateBlockAt(ir.GetNextAddress());
        IRBlock* equal_block = ir.CreateBlock();

        SSAInstruction* eax = ir.GetReg(X86_REF_RAX, size_e, false);
        SSAInstruction* address = ir.Lea(inst->operand_rm);
        SSAInstruction* reg = ir.GetReg(inst->operand_reg);
        SSAInstruction* actual = ir.AmoCAS(address, eax, reg, size_e);

        SSAInstruction* equal = ir.Equal(actual, eax);
        ir.SetFlag(equal, X86_REF_ZF);

        ir.TerminateJumpConditional(equal, next_instruction_target, equal_block);
        ir.SetBlock(equal_block);

        ir.SetReg(actual, X86_REF_RAX, size_e, false);
        ir.TerminateJump(next_instruction_target);
    } else {
        IRBlock* equal_block = ir.CreateBlock();
        IRBlock* not_equal_block = ir.CreateBlock();
        IRBlock* next_instruction_target = ir.CreateBlockAt(ir.GetNextAddress());

        SSAInstruction* eax = ir.GetReg(X86_REF_RAX, size_e, false);
        SSAInstruction* rm = ir.GetRm(inst->operand_rm);
        SSAInstruction* equal = ir.Equal(eax, rm);
        ir.SetFlag(equal, X86_REF_ZF);

        ir.TerminateJumpConditional(equal, equal_block, not_equal_block);
        ir.SetBlock(equal_block);

        SSAInstruction* reg = ir.GetReg(inst->operand_reg);
        ir.SetRm(inst->operand_rm, reg);

        ir.TerminateJump(next_instruction_target);
        ir.SetBlock(not_equal_block);

        ir.SetReg(rm, X86_REF_RAX, size_e, false);
        ir.TerminateJump(next_instruction_target);
    }
}

IR_HANDLE(movzx) { // movzx r32/64, rm16 - 0x0f 0xb7
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    ir.SetReg(inst->operand_reg, rm);
}

IR_HANDLE(bsr) { // bsr - 0x0f 0xbd
    IRBlock* next_instruction_target = ir.CreateBlockAt(ir.GetNextAddress());
    IRBlock* not_zero_target = ir.CreateBlock();
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* is_zero = ir.IsZero(rm, size_e);
    ir.SetFlag(is_zero, X86_REF_ZF);

    SSAInstruction* not_zero = ir.IsNotZero(rm, size_e);

    // We wanna leave reg untouched if it's zero
    ir.TerminateJumpConditional(not_zero, not_zero_target, next_instruction_target);
    ir.SetBlock(not_zero_target);

    SSAInstruction* clz = ir.Clz(rm);
    SSAInstruction* sub = ir.Sub(ir.Imm(63), clz);
    ir.SetReg(inst->operand_reg, sub);

    ir.TerminateJump(next_instruction_target);
}

IR_HANDLE(bsf) { // bsf - 0x0f 0xbc
    IRBlock* next_instruction_target = ir.CreateBlockAt(ir.GetNextAddress());
    IRBlock* not_zero_target = ir.CreateBlock();
    x86_size_e size_e = inst->operand_reg.size;
    SSAInstruction* rm = ir.GetRm(inst->operand_rm);
    SSAInstruction* is_zero = ir.IsZero(rm, size_e);
    ir.SetFlag(is_zero, X86_REF_ZF);

    SSAInstruction* not_zero = ir.IsNotZero(rm, size_e);

    // We wanna leave reg untouched if it's zero
    ir.TerminateJumpConditional(not_zero, not_zero_target, next_instruction_target);
    ir.SetBlock(not_zero_target);

    SSAInstruction* ctz = ir.Ctz(rm);
    ir.SetReg(inst->operand_reg, ctz);

    ir.TerminateJump(next_instruction_target);
}

// ███████ ███████  ██████  ██████  ███    ██ ██████   █████  ██████  ██    ██      ██████   ██████
// ██      ██      ██      ██    ██ ████   ██ ██   ██ ██   ██ ██   ██  ██  ██      ██       ██
// ███████ █████   ██      ██    ██ ██ ██  ██ ██   ██ ███████ ██████    ████       ███████  ███████
//      ██ ██      ██      ██    ██ ██  ██ ██ ██   ██ ██   ██ ██   ██    ██        ██    ██ ██    ██
// ███████ ███████  ██████  ██████  ██   ████ ██████  ██   ██ ██   ██    ██         ██████   ██████

IR_HANDLE(mov_xmm_xmm128) {
    SSAInstruction* rm = ir.GetRm(inst->operand_rm, VectorState::PackedByte);
    ir.SetReg(inst->operand_reg, rm);
}

IR_HANDLE(punpcklbw) { // punpcklbw xmm, xmm/m128 - 0x66 0x0f 0x60
    ir.Punpckl(inst, VectorState::PackedByte);
}

IR_HANDLE(punpcklwd) { // punpcklwd xmm, xmm/m128 - 0x66 0x0f 0x61
    ir.Punpckl(inst, VectorState::PackedWord);
}

IR_HANDLE(punpckldq) { // punpckldq xmm, xmm/m128 - 0x66 0x0f 0x62
    ir.Punpckl(inst, VectorState::PackedDWord);
}

IR_HANDLE(punpcklqdq) { // punpcklqdq xmm, xmm/m128 - 0x66 0x0f 0x6c
    ir.Punpckl(inst, VectorState::PackedQWord);
}

IR_HANDLE(punpckhbw) { // punpckhbw xmm, xmm/m128 - 0x66 0x0f 0x68
    ir.Punpckh(inst, VectorState::PackedByte);
}

IR_HANDLE(punpckhwd) { // punpckhwd xmm, xmm/m128 - 0x66 0x0f 0x69
    ir.Punpckh(inst, VectorState::PackedWord);
}

IR_HANDLE(punpckhdq) { // punpckhdq xmm, xmm/m128 - 0x66 0x0f 0x6a
    ir.Punpckh(inst, VectorState::PackedDWord);
}

IR_HANDLE(punpckhqdq) { // punpckhqdq xmm, xmm/m128 - 0x66 0x0f 0x6d
    ir.Punpckh(inst, VectorState::PackedQWord);
}

IR_HANDLE(pshufd) { // pshufd xmm, xmm/m128, imm8 - 0x66 0x0f 0x70
    ASSERT(inst->operand_rm.size == X86_SIZE_XMM);
    u8 imm = inst->operand_imm.immediate.data;
    u8 el0 = imm & 0b11;
    u8 el1 = (imm >> 2) & 0b11;
    u8 el2 = (imm >> 4) & 0b11;
    u8 el3 = (imm >> 6) & 0b11;
    SSAInstruction* first = ir.VSplati(el3, VectorState::PackedDWord);
    SSAInstruction* second = ir.VSlide1Up(ir.Imm(el2), first, VectorState::PackedDWord);
    SSAInstruction* third = ir.VSlide1Up(ir.Imm(el1), second, VectorState::PackedDWord);
    SSAInstruction* fourth = ir.VSlide1Up(ir.Imm(el0), third, VectorState::PackedDWord);
    SSAInstruction* source = ir.GetRm(inst->operand_rm, VectorState::PackedDWord);
    SSAInstruction* result = ir.VGather(ir.VZero(VectorState::PackedDWord), source, fourth, VectorState::PackedDWord);
    ir.SetReg(inst->operand_reg, result);
}

// These names lmao
IR_HANDLE(shufpd) {
    u8 imm = inst->operand_imm.immediate.data;
    SSAInstruction *src1, *src2;

    if ((imm & 0b01) == 0) {
        src1 = ir.VToI(ir.GetReg(inst->operand_reg), VectorState::PackedQWord);
    } else {
        SSAInstruction* reg = ir.GetReg(inst->operand_reg);
        SSAInstruction* slide = ir.VSlideDowni(reg, 1, VectorState::PackedQWord);
        src1 = ir.VToI(slide, VectorState::PackedQWord);
    }

    if ((imm & 0b10) == 0) {
        src2 = ir.GetRm(inst->operand_rm, VectorState::PackedQWord);
    } else {
        SSAInstruction* rm = ir.GetRm(inst->operand_rm, VectorState::PackedQWord);
        src2 = ir.VSlideDowni(rm, 1, VectorState::PackedQWord);
    }

    // Slide it up and insert src1
    SSAInstruction* result = ir.VSlide1Up(src1, src2, VectorState::PackedQWord);
    ir.SetReg(inst->operand_reg, result);
}

IR_HANDLE(group14) {
    ir.Group14(inst);
}

IR_HANDLE(group15) {
    ir.Group15(inst);
}

IR_HANDLE(pmovmskb) {
    SSAInstruction* rm = ir.GetRm(inst->operand_rm, VectorState::PackedByte);
    SSAInstruction* mask = ir.VMSlt(rm, ir.Imm(0), VectorState::PackedByte);
    static_assert(SUPPORTED_VLEN == 128); // if vlen changes, change the zext below
    ir.SetReg(inst->operand_reg, ir.Zext(ir.VToI(mask, VectorState::PackedWord), X86_SIZE_WORD));
}

IR_HANDLE(movq_xmm_rm32) { // movq xmm, rm32 - 0x66 0x0f 0x6e
    x86_size_e size_e = inst->operand_rm.size;
    VectorState vector_state = VectorState::Null;
    switch (size_e) {
    case X86_SIZE_DWORD: {
        vector_state = VectorState::PackedDWord;
        break;
    }
    case X86_SIZE_QWORD: {
        vector_state = VectorState::PackedQWord;
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }
    SSAInstruction* rm = ir.GetRm(inst->operand_rm, vector_state);
    SSAInstruction* vector = ir.VZext(ir.IToV(rm, vector_state), size_e);
    ir.SetReg(inst->operand_reg, vector);
}

IR_HANDLE(pcmpeqb) { // pcmpeqb xmm, xmm/m128 - 0x66 0x0f 0x74
    ir.Pcmpeq(inst, VectorState::PackedByte);
}

IR_HANDLE(pcmpeqw) { // pcmpeqw xmm, xmm/m128 - 0x66 0x0f 0x75
    ir.Pcmpeq(inst, VectorState::PackedWord);
}

IR_HANDLE(pcmpeqd) { // pcmpeqd xmm, xmm/m128 - 0x66 0x0f 0x76
    ir.Pcmpeq(inst, VectorState::PackedDWord);
}

IR_HANDLE(movq_rm32_xmm) { // movq rm32, xmm - 0x66 0x0f 0x7e
    x86_size_e size_e = inst->operand_rm.size;
    VectorState vector_state = VectorState::Null;
    switch (size_e) {
    case X86_SIZE_DWORD: {
        vector_state = VectorState::PackedDWord;
        break;
    }
    case X86_SIZE_QWORD: {
        vector_state = VectorState::PackedQWord;
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }
    SSAInstruction* xmm = ir.GetReg(inst->operand_reg);
    SSAInstruction* rm = ir.VToI(xmm, vector_state);
    ir.SetRm(inst->operand_rm, rm, vector_state);
}

IR_HANDLE(movq_xmm64_xmm) { // movq xmm64, xmm - 0x66 0x0f 0xd6
    SSAInstruction* xmm = ir.GetReg(inst->operand_reg);
    if (inst->operand_rm.type == X86_OP_TYPE_MEMORY) {
        SSAInstruction* rm = ir.VToI(xmm, VectorState::PackedQWord);
        inst->operand_rm.size = X86_SIZE_QWORD;
        ir.SetRm(inst->operand_rm, rm);
    } else {
        ir.SetReg(inst->operand_rm, ir.VZext(xmm, X86_SIZE_QWORD));
    }
}

IR_HANDLE(pminub) { // pminub xmm, xmm/m128 - 0x66 0x0f 0xda
    ir.PackedRegRm(inst, IROpcode::VMinu, VectorState::PackedByte);
}

IR_HANDLE(pand) { // pand xmm, xmm/m128 - 0x66 0x0f 0xdb
    ir.PackedRegRm(inst, IROpcode::VAnd, VectorState::AnyPacked);
}

IR_HANDLE(paddb) { // paddb xmm, xmm/m128 - 0x66 0x0f 0xfc
    ir.PackedRegRm(inst, IROpcode::VAdd, VectorState::PackedByte);
}

IR_HANDLE(paddw) { // paddw xmm, xmm/m128 - 0x66 0x0f 0xfd
    ir.PackedRegRm(inst, IROpcode::VAdd, VectorState::PackedWord);
}

IR_HANDLE(paddd) { // paddd xmm, xmm/m128 - 0x66 0x0f 0xfe
    ir.PackedRegRm(inst, IROpcode::VAdd, VectorState::PackedDWord);
}

IR_HANDLE(paddq) { // paddq xmm, xmm/m128 - 0x66 0x0f 0xd4
    ir.PackedRegRm(inst, IROpcode::VAdd, VectorState::PackedQWord);
}

IR_HANDLE(pandn) {
    SSAInstruction* rm = ir.GetRm(inst->operand_rm, VectorState::AnyPacked);
    SSAInstruction* reg = ir.GetReg(inst->operand_reg);
    SSAInstruction* reg_not = ir.VXori(reg, -1, VectorState::AnyPacked);
    SSAInstruction* result = ir.VAnd(reg_not, rm, VectorState::AnyPacked);
    ir.SetReg(inst->operand_reg, result);
}

IR_HANDLE(por) { // por xmm, xmm/m128 - 0x66 0x0f 0xeb
    ir.PackedRegRm(inst, IROpcode::VOr, VectorState::AnyPacked);
}

IR_HANDLE(pxor) { // pxor xmm, xmm/m128 - 0x66 0x0f 0xef
    ir.PackedRegRm(inst, IROpcode::VXor, VectorState::AnyPacked);
}

IR_HANDLE(psubb) { // psubb xmm, xmm/m128 - 0x66 0x0f 0xf8
    ir.PackedRegRm(inst, IROpcode::VSub, VectorState::PackedByte);
}

IR_HANDLE(psubw) { // psubw xmm, xmm/m128 - 0x66 0x0f 0xf9
    ir.PackedRegRm(inst, IROpcode::VSub, VectorState::PackedWord);
}

IR_HANDLE(psubd) { // psubd xmm, xmm/m128 - 0x66 0x0f 0xfa
    ir.PackedRegRm(inst, IROpcode::VSub, VectorState::PackedDWord);
}

IR_HANDLE(psubq) { // psubq xmm, xmm/m128 - 0x66 0x0f 0xfb
    ir.PackedRegRm(inst, IROpcode::VSub, VectorState::PackedQWord);
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

IR_HANDLE(addss) { // addss xmm, xmm32 - 0xf3 0x0f 0x58
    ir.ScalarRegRm(inst, IROpcode::VFAdd, VectorState::Float);
}

IR_HANDLE(addsd) {
    ir.ScalarRegRm(inst, IROpcode::VFAdd, VectorState::Double);
}

IR_HANDLE(addps) {
    ir.PackedRegRm(inst, IROpcode::VFAdd, VectorState::PackedDWord);
}

IR_HANDLE(addpd) {
    ir.PackedRegRm(inst, IROpcode::VFAdd, VectorState::PackedQWord);
}

IR_HANDLE(subss) { // subss xmm, xmm32 - 0xf3 0x0f 0x5c
    ir.ScalarRegRm(inst, IROpcode::VFSub, VectorState::Float);
}

IR_HANDLE(subsd) {
    ir.ScalarRegRm(inst, IROpcode::VFSub, VectorState::Double);
}

IR_HANDLE(subps) {
    ir.PackedRegRm(inst, IROpcode::VFSub, VectorState::PackedDWord);
}

IR_HANDLE(subpd) {
    ir.PackedRegRm(inst, IROpcode::VFSub, VectorState::PackedQWord);
}

IR_HANDLE(mulss) { // mulss xmm, xmm32 - 0xf3 0x0f 0x59
    ir.ScalarRegRm(inst, IROpcode::VFMul, VectorState::Float);
}

IR_HANDLE(mulsd) {
    ir.ScalarRegRm(inst, IROpcode::VFMul, VectorState::Double);
}

IR_HANDLE(mulps) {
    ir.PackedRegRm(inst, IROpcode::VFMul, VectorState::PackedDWord);
}

IR_HANDLE(mulpd) {
    ir.PackedRegRm(inst, IROpcode::VFMul, VectorState::PackedQWord);
}

IR_HANDLE(divss) { // divss xmm, xmm32 - 0xf3 0x0f 0x5e
    ir.ScalarRegRm(inst, IROpcode::VFDiv, VectorState::Float);
}

IR_HANDLE(divsd) {
    ir.ScalarRegRm(inst, IROpcode::VFDiv, VectorState::Double);
}

IR_HANDLE(divps) {
    ir.PackedRegRm(inst, IROpcode::VFDiv, VectorState::PackedDWord);
}

IR_HANDLE(divpd) {
    ir.PackedRegRm(inst, IROpcode::VFDiv, VectorState::PackedQWord);
}

IR_HANDLE(sqrtss) { // sqrtss xmm, xmm32 - 0xf3 0x0f 0x51
    ir.ScalarRegRm(inst, felix86_sqrt, VectorState::Float);
}

IR_HANDLE(sqrtsd) {
    ir.ScalarRegRm(inst, felix86_sqrt, VectorState::Double);
}

IR_HANDLE(sqrtps) {
    ir.PackedRegRm(inst, felix86_sqrt, VectorState::PackedDWord);
}

IR_HANDLE(sqrtpd) {
    ir.PackedRegRm(inst, felix86_sqrt, VectorState::PackedQWord);
}

IR_HANDLE(minss) { // minss xmm, xmm32 - 0xf3 0x0f 0x5d
    ir.ScalarRegRm(inst, IROpcode::VFMin, VectorState::Float);
}

IR_HANDLE(minsd) {
    ir.ScalarRegRm(inst, IROpcode::VFMin, VectorState::Double);
}

IR_HANDLE(minps) {
    ir.PackedRegRm(inst, IROpcode::VFMin, VectorState::PackedDWord);
}

IR_HANDLE(minpd) {
    ir.PackedRegRm(inst, IROpcode::VFMin, VectorState::PackedQWord);
}

IR_HANDLE(maxss) { // maxss xmm, xmm32 - 0xf3 0x0f 0x5f
    ir.ScalarRegRm(inst, IROpcode::VFMax, VectorState::Float);
}

IR_HANDLE(maxsd) {
    ir.ScalarRegRm(inst, IROpcode::VFMax, VectorState::Double);
}

IR_HANDLE(maxps) {
    ir.PackedRegRm(inst, IROpcode::VFMax, VectorState::PackedDWord);
}

IR_HANDLE(maxpd) {
    ir.PackedRegRm(inst, IROpcode::VFMax, VectorState::PackedQWord);
}

IR_HANDLE(rsqrtss) { // rsqrtss xmm, xmm32 - 0xf3 0x0f 0x52
    ir.ScalarRegRm(inst, felix86_rcpsqrt, VectorState::Float);
}

IR_HANDLE(rsqrtps) {
    ir.PackedRegRm(inst, felix86_rcpsqrt, VectorState::PackedDWord);
}

IR_HANDLE(rcpss) { // rcpss xmm, xmm32 - 0xf3 0x0f 0x53
    ir.ScalarRegRm(inst, felix86_rcp, VectorState::Float);
}

IR_HANDLE(rcpps) {
    ir.PackedRegRm(inst, felix86_rcp, VectorState::PackedDWord);
}

IR_HANDLE(movq_xmm_xmm64) { // movq xmm, xmm64 - 0xf3 0x0f 0x7e
    x86_operand_t rm_op = inst->operand_rm;
    SSAInstruction* result;
    if (rm_op.type == X86_OP_TYPE_MEMORY) {
        rm_op.size = X86_SIZE_QWORD;
        result = ir.VZext(ir.IToV(ir.GetRm(rm_op, VectorState::PackedQWord), VectorState::PackedQWord), X86_SIZE_QWORD);
    } else {
        result = ir.VZext(ir.GetReg(rm_op), X86_SIZE_QWORD);
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
