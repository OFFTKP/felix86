#include "felix86/backend/allocated_register.hpp"
#include "felix86/backend/backend.hpp"
#include "felix86/backend/emitter.hpp"

#define AS (backend.GetAssembler())

#define MAYBE_C(operation)                                                                                                                           \
    if (Rd == Rs1) {                                                                                                                                 \
        AS.C_##operation(Rd, Rs2);                                                                                                                   \
    } else if (Rd == Rs2) {                                                                                                                          \
        AS.C_##operation(Rd, Rs1);                                                                                                                   \
    } else {                                                                                                                                         \
        AS.operation(Rd, Rs1, Rs2);                                                                                                                  \
    }

namespace {

void SoftwareCtz(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs, u32 size) {
    WARN("Untested CTZ implementation");
    biscuit::GPR mask = backend.AcquireScratchGPR();
    biscuit::GPR counter = backend.AcquireScratchGPR();
    AS.LI(mask, 1);

    Label loop, end;
    AS.Bind(&loop);
    AS.AND(Rd, Rs, mask);
    AS.BNEZ(Rd, &end);
    AS.C_ADDI(counter, 1);
    AS.C_SLLI(mask, 1);
    AS.LI(Rd, size);
    AS.SLTU(Rd, counter, Rd);
    AS.BNEZ(Rd, &loop);

    AS.Bind(&end);
    AS.MV(Rd, counter);
}

[[nodiscard]] constexpr bool IsValidSigned12BitImm(ptrdiff_t value) {
    return value >= -2048 && value <= 2047;
}

} // namespace

void Emitter::EmitJump(Backend& backend, void* target) {
    auto my_abs = [](u64 x) -> u64 { return x < 0 ? -x : x; };

    // Check if target is in one MB range
    void* cursor = AS.GetCursorPointer();
    if (my_abs((u64)cursor - (u64)target) > 0x100000) {
        biscuit::GPR scratch = backend.AcquireScratchGPR();
        AS.LI(scratch, (u64)target);
        AS.JR(scratch);
        backend.ReleaseScratchRegs();
    } else {
        AS.J((u64)target - (u64)cursor);
    }
}

void Emitter::EmitJumpConditional(Backend& backend, const IRInstruction& condition, void* target_true, void* target_false) {
    biscuit::GPR address_true = backend.AcquireScratchGPR();
    biscuit::GPR address_false = backend.AcquireScratchGPR();
    biscuit::GPR condition_reg = _RegRO_(&condition);
    Label false_label;

    // TODO: emit relative jumps if possible
    AS.BEQZ(condition_reg, &false_label);
    AS.LI(address_true, (u64)target_true);
    AS.JR(address_true);
    AS.Bind(&false_label);
    AS.LI(address_false, (u64)target_false);
    AS.JR(address_false);
}

void Emitter::EmitLoadGuestFromMemory(Backend& backend, const IRInstruction& inst) {
    biscuit::GPR address = backend.GetRegisters().ThreadStatePointer();

    auto Rd = _RegWO_(&inst);

    const GetGuest& get_guest = inst.AsGetGuest();
    x86_ref_e ref = get_guest.ref;
    switch (ref) {
    case X86_REF_RAX ... X86_REF_R15: {
        u64 offset = offsetof(ThreadState, gprs) + (ref - X86_REF_RAX) * sizeof(u64);
        AS.LD(Rd, offset, address);
        break;
    }
    case X86_REF_CF ... X86_REF_OF: {
        u64 offset = offsetof(ThreadState, cf) + (ref - X86_REF_CF) * sizeof(bool);
        AS.LB(Rd, offset, address);
        break;
    }
    case X86_REF_RIP: {
        u64 offset = offsetof(ThreadState, rip);
        AS.LD(Rd, offset, address);
        break;
    }
    case X86_REF_FS: {
        u64 offset = offsetof(ThreadState, fsbase);
        AS.LD(Rd, offset, address);
        break;
    }
    case X86_REF_GS: {
        u64 offset = offsetof(ThreadState, gsbase);
        AS.LD(Rd, offset, address);
        break;
    }
    default: {
        UNREACHABLE();
    }
    }
}

void Emitter::EmitStoreGuestToMemory(Backend& backend, const IRInstruction& inst) {
    biscuit::GPR address = backend.GetRegisters().ThreadStatePointer();

    const SetGuest& set_guest = inst.AsSetGuest();
    x86_ref_e ref = set_guest.ref;
    auto Rs = _RegRO_(set_guest.source);

    switch (ref) {
    case X86_REF_RAX ... X86_REF_R15: {
        u64 offset = offsetof(ThreadState, gprs) + (ref - X86_REF_RAX) * sizeof(u64);
        AS.SD(Rs, offset, address);
        break;
    }
    case X86_REF_CF ... X86_REF_OF: {
        u64 offset = offsetof(ThreadState, cf) + (ref - X86_REF_CF) * sizeof(bool);
        AS.SB(Rs, offset, address);
        break;
    }
    case X86_REF_RIP: {
        u64 offset = offsetof(ThreadState, rip);
        AS.SD(Rs, offset, address);
        break;
    }
    case X86_REF_FS: {
        u64 offset = offsetof(ThreadState, fsbase);
        AS.SD(Rs, offset, address);
        break;
    }
    case X86_REF_GS: {
        u64 offset = offsetof(ThreadState, gsbase);
        AS.SD(Rs, offset, address);
        break;
    }
    default: {
        UNREACHABLE();
    }
    }
}

void Emitter::EmitPushHost(Backend& backend, const IRInstruction& inst) {
    const PushHost& push_host = inst.AsPushHost();
    biscuit::GPR vm_state = backend.GetRegisters().AcquireScratchGPR();
    AS.LI(vm_state, backend.GetVMStatePointer());
    switch (push_host.ref) {
    case RISCV_REF_X0 ... RISCV_REF_X31: {
        u32 index = push_host.ref - RISCV_REF_X0;
        u32 offset = index * sizeof(u64);
        biscuit::GPR to_push = biscuit::GPR(index);
        AS.SD(to_push, offset, vm_state);
        break;
    }
    case RISCV_REF_F0 ... RISCV_REF_F31: {
        u32 index = push_host.ref - RISCV_REF_F0;
        u32 offset = index * sizeof(double) + (32 * sizeof(u64));
        biscuit::FPR to_push = biscuit::FPR(index);
        AS.FSD(to_push, offset, vm_state);
        break;
    }
    case RISCV_REF_VEC0 ... RISCV_REF_VEC31: {
        ERROR("Implme");
        break;
    }
    default: {
        UNREACHABLE();
    }
    }
}

void Emitter::EmitPopHost(Backend& backend, const IRInstruction& inst) {
    const PopHost& pop_host = inst.AsPopHost();
    biscuit::GPR vm_state = backend.GetRegisters().AcquireScratchGPR();
    AS.LI(vm_state, backend.GetVMStatePointer());
    switch (pop_host.ref) {
    case RISCV_REF_X0 ... RISCV_REF_X31: {
        u32 index = pop_host.ref - RISCV_REF_X0;
        u32 offset = index * sizeof(u64);
        biscuit::GPR to_pop = biscuit::GPR(index);
        biscuit::GPR base = vm_state;
        AS.LD(to_pop, offset, base);
        break;
    }
    case RISCV_REF_F0 ... RISCV_REF_F31: {
        u32 index = pop_host.ref - RISCV_REF_F0;
        u32 offset = index * sizeof(double) + (32 * sizeof(u64));
        biscuit::FPR to_pop = biscuit::FPR(index);
        biscuit::GPR base = vm_state;
        AS.FLD(to_pop, offset, base);
        break;
    }
    case RISCV_REF_VEC0 ... RISCV_REF_VEC31: {
        ERROR("Implme");
        break;
    }
    default: {
        UNREACHABLE();
    }
    }
}

void Emitter::EmitImmediate(Backend& backend, biscuit::GPR Rd, u64 immediate) {
    AS.LI(Rd, immediate);
}

void Emitter::EmitRdtsc(Backend& backend) {
    UNREACHABLE();
}

void Emitter::EmitSyscall(Backend& backend) {
    UNREACHABLE();
}

void Emitter::EmitCpuid(Backend& backend) {
    UNREACHABLE();
}

void Emitter::EmitSext8(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    if (Rd == Rs) {
        AS.C_SEXT_B(Rd);
    } else {
        AS.SEXTB(Rd, Rs);
    }
}

void Emitter::EmitSext16(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    if (Rd == Rs) {
        AS.C_SEXT_H(Rd);
    } else {
        AS.SEXTH(Rd, Rs);
    }
}

void Emitter::EmitSext32(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    UNREACHABLE();
}

void Emitter::EmitClz(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    AS.CLZ(Rd, Rs);
}

void Emitter::EmitCtzh(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    SoftwareCtz(backend, Rd, Rs, 16);
}

void Emitter::EmitCtzw(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    if (backend.HasB()) {
        AS.CTZW(Rd, Rs);
    } else {
        SoftwareCtz(backend, Rd, Rs, 32);
    }
}

void Emitter::EmitCtz(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    if (backend.HasB()) {
        AS.CTZ(Rd, Rs);
    } else {
        SoftwareCtz(backend, Rd, Rs, 64);
    }
}

void Emitter::EmitNot(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    if (Rd == Rs) {
        AS.C_NOT(Rd);
    } else {
        AS.NOT(Rd, Rs);
    }
}

void Emitter::EmitParity(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    if (backend.HasB()) {
        AS.ANDI(Rd, Rs, 0xFF);
        AS.CPOPW(Rd, Rd);
    } else {
        // clang-format off
        static bool bitcount[] = {
            1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
            0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
            0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
            1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
            0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
            1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
            1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
            0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
            0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
            1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
            1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
            0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
            1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
            0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
            0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
            1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
        };
        // clang-format on

        biscuit::GPR scratch = backend.AcquireScratchGPR();
        AS.LI(scratch, (u64)&bitcount);
        AS.ANDI(Rd, Rs, 0xFF);
        AS.C_ADD(Rd, scratch);
        AS.LB(Rd, 0, Rd);
    }
}

void Emitter::EmitReadByte(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    AS.LB(Rd, 0, Rs);
}

void Emitter::EmitReadWord(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    AS.LH(Rd, 0, Rs);
}

void Emitter::EmitReadDWord(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    AS.LW(Rd, 0, Rs);
}

void Emitter::EmitReadQWord(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    AS.LD(Rd, 0, Rs);
}

void Emitter::EmitReadXmmWord(Backend&, biscuit::Vec, biscuit::GPR) {
    UNREACHABLE();
}

void Emitter::EmitDiv128(Backend&, biscuit::GPR Rs) {
    UNREACHABLE();
}

void Emitter::EmitDivu128(Backend&, biscuit::GPR Rs) {
    UNREACHABLE();
}

void Emitter::EmitWriteByte(Backend& backend, biscuit::GPR address, biscuit::GPR Rs) {
    AS.SB(Rs, 0, address);
}

void Emitter::EmitWriteWord(Backend& backend, biscuit::GPR address, biscuit::GPR Rs) {
    AS.SH(Rs, 0, address);
}

void Emitter::EmitWriteDWord(Backend& backend, biscuit::GPR address, biscuit::GPR Rs) {
    AS.SW(Rs, 0, address);
}

void Emitter::EmitWriteQWord(Backend& backend, biscuit::GPR address, biscuit::GPR Rs) {
    AS.SD(Rs, 0, address);
}

void Emitter::EmitWriteXmmWord(Backend&, biscuit::GPR, biscuit::Vec) {
    UNREACHABLE();
}

void Emitter::EmitAddi(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs, u64 immediate) {
    if (IsValidSigned12BitImm((i64)immediate)) {
        AS.ADDI(Rd, Rs, (i64)immediate);
    } else {
        biscuit::GPR scratch = backend.AcquireScratchGPR();
        AS.LI(scratch, immediate);
        AS.ADD(Rd, Rs, scratch);
    }
}

void Emitter::EmitAdd(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    MAYBE_C(ADD);
}

void Emitter::EmitSub(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    if (Rd == Rs1) {
        AS.C_SUB(Rd, Rs2);
    } else {
        AS.SUB(Rd, Rs1, Rs2);
    }
}

void Emitter::EmitAnd(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    MAYBE_C(AND);
}

void Emitter::EmitOr(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    MAYBE_C(OR);
}

void Emitter::EmitXor(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    MAYBE_C(XOR);
}

void Emitter::EmitEqual(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    MAYBE_C(XOR);
    AS.SEQZ(Rd, Rd);
}

void Emitter::EmitNotEqual(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    MAYBE_C(XOR);
    AS.SNEZ(Rd, Rd);
}

void Emitter::EmitIGreaterThan(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.SLT(Rd, Rs2, Rs1);
}

void Emitter::EmitILessThan(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.SLT(Rd, Rs1, Rs2);
}

void Emitter::EmitUGreaterThan(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.SLTU(Rd, Rs2, Rs1);
}

void Emitter::EmitULessThan(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.SLTU(Rd, Rs1, Rs2);
}

void Emitter::EmitShiftLeft(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.SLL(Rd, Rs1, Rs2); // TODO: add more robust shift IR instructions to abuse C_SLLI & co
}

void Emitter::EmitShiftRight(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.SRL(Rd, Rs1, Rs2);
}

void Emitter::EmitShiftRightArithmetic(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.SRA(Rd, Rs1, Rs2);
}

void Emitter::EmitLeftRotate8(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    biscuit::GPR scratch = backend.AcquireScratchGPR();
    AS.ANDI(scratch, Rs2, 0x7);
    AS.SLLW(Rd, Rs1, scratch);
    AS.NEG(scratch, scratch);
    AS.ANDI(scratch, scratch, 0x7);
    AS.SRLW(scratch, Rs1, scratch);
    AS.OR(Rd, Rd, scratch);
    AS.ANDI(Rd, Rd, 0xFF);
}

void Emitter::EmitLeftRotate16(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    biscuit::GPR scratch = backend.AcquireScratchGPR();
    AS.ANDI(scratch, Rs2, 0x1F);
    AS.SLLW(Rd, Rs1, scratch);
    AS.NEG(scratch, scratch);
    AS.ANDI(scratch, scratch, 0x1F);
    AS.SRLW(scratch, Rs1, scratch);
    AS.OR(Rd, Rd, scratch);
    AS.ZEXTH(Rd, Rd);
}

void Emitter::EmitLeftRotate32(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.ROLW(Rd, Rs1, Rs2);
}

void Emitter::EmitLeftRotate64(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.ROL(Rd, Rs1, Rs2);
}

void Emitter::EmitDiv(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.DIV(Rd, Rs1, Rs2);
}

void Emitter::EmitDivu(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.DIVU(Rd, Rs1, Rs2);
}

void Emitter::EmitDivw(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.DIVW(Rd, Rs1, Rs2);
}

void Emitter::EmitDivuw(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.DIVUW(Rd, Rs1, Rs2);
}

void Emitter::EmitRem(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.REM(Rd, Rs1, Rs2);
}

void Emitter::EmitRemu(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.REMU(Rd, Rs1, Rs2);
}

void Emitter::EmitRemw(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.REMW(Rd, Rs1, Rs2);
}

void Emitter::EmitRemuw(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.REMUW(Rd, Rs1, Rs2);
}

void Emitter::EmitMul(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.MUL(Rd, Rs1, Rs2);
}

void Emitter::EmitMulh(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.MULH(Rd, Rs1, Rs2);
}

void Emitter::EmitMulhu(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.MULHU(Rd, Rs1, Rs2);
}

void Emitter::EmitSelect(Backend& backend, biscuit::GPR Rd, biscuit::GPR Condition, biscuit::GPR RsTrue, biscuit::GPR RsFalse) {
    Label true_label, end_label;
    AS.C_BNEZ(Condition, &true_label);
    AS.C_MV(Rd, RsFalse);
    AS.C_J(&end_label);
    AS.Bind(&true_label);
    AS.C_MV(Rd, RsTrue);
    AS.Bind(&end_label);
}

void Emitter::EmitCastIntegerToVector(Backend&, biscuit::Vec, biscuit::GPR) {
    UNREACHABLE();
}

void Emitter::EmitCastVectorToInteger(Backend&, biscuit::GPR, biscuit::Vec) {
    UNREACHABLE();
}

void Emitter::EmitVInsertInteger(Backend&, biscuit::Vec, biscuit::GPR, biscuit::Vec, u64) {
    UNREACHABLE();
}

void Emitter::EmitVExtractInteger(Backend&, biscuit::GPR, biscuit::Vec, u64) {
    UNREACHABLE();
}

void Emitter::EmitVPackedShuffleDWord(Backend&, biscuit::Vec, biscuit::Vec, u64) {
    UNREACHABLE();
}

void Emitter::EmitVMoveByteMask(Backend&, biscuit::Vec, biscuit::Vec) {
    UNREACHABLE();
}

void Emitter::EmitVUnpackByteLow(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec) {
    UNREACHABLE();
}

void Emitter::EmitVUnpackWordLow(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec) {
    UNREACHABLE();
}

void Emitter::EmitVUnpackDWordLow(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec) {
    UNREACHABLE();
}

void Emitter::EmitVUnpackQWordLow(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec) {
    UNREACHABLE();
}

void Emitter::EmitVAnd(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec) {
    UNREACHABLE();
}

void Emitter::EmitVOr(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec) {
    UNREACHABLE();
}

void Emitter::EmitVXor(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec) {
    UNREACHABLE();
}

void Emitter::EmitVShiftRight(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec) {
    UNREACHABLE();
}

void Emitter::EmitVShiftLeft(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec) {
    UNREACHABLE();
}

void Emitter::EmitVPackedSubByte(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec) {
    UNREACHABLE();
}

void Emitter::EmitVPackedAddQWord(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec) {
    UNREACHABLE();
}

void Emitter::EmitVPackedEqualByte(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec) {
    UNREACHABLE();
}

void Emitter::EmitVPackedEqualWord(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec) {
    UNREACHABLE();
}

void Emitter::EmitVPackedEqualDWord(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec) {
    UNREACHABLE();
}

void Emitter::EmitVPackedMinByte(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec) {
    UNREACHABLE();
}

void Emitter::EmitVZext64(Backend&, biscuit::Vec, biscuit::Vec) {
    UNREACHABLE();
}