#include <Zydis/Zydis.h>
#include "felix86/v2/recompiler.hpp"

#define FAST_HANDLE(name)                                                                                                                            \
    void fast_##name(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands)

FAST_HANDLE(FLD) {
    if (operands[0].size == 80) {
        rec.writebackDirtyState();
        rec.invalidStateUntilJump();
        biscuit::GPR address = rec.leaAddBase(&operands[0]);
        as.MV(a0, address);
        rec.call((u64)f80_to_64);
        biscuit::GPR top = rec.getTOP();
        rec.pushST(top, fa0); // push return value
    } else {
        biscuit::GPR top = rec.getTOP();
        biscuit::FPR st = rec.getST(top, &operands[0]);
        rec.pushST(top, st);
    }
}

FAST_HANDLE(FILD) {
    biscuit::GPR top = rec.getTOP();
    biscuit::FPR ftemp = rec.scratchFPR();
    biscuit::GPR value = rec.getOperandGPR(&operands[0]);
    as.FCVT_D_L(ftemp, value);
    rec.pushST(top, ftemp);
}

void OP(void (Assembler::*func)(FPR, FPR, FPR, RMode), Recompiler& rec, Assembler& as, ZydisDecodedInstruction& instruction,
        ZydisDecodedOperand* operands, bool pop) {
    biscuit::GPR top = rec.getTOP();
    biscuit::FPR lhs = rec.getST(top, &operands[0]);
    biscuit::FPR rhs = rec.getST(top, &operands[1]);

    biscuit::FPR result = rec.scratchFPR();
    (as.*func)(result, lhs, rhs, RMode::DYN);
    rec.setST(top, 0, result);

    if (pop) {
        rec.popST(top);
    }
}

FAST_HANDLE(FDIV) {
    OP(&Assembler::FDIV_D, rec, as, instruction, operands, false);
}

FAST_HANDLE(FDIVP) {
    OP(&Assembler::FDIV_D, rec, as, instruction, operands, true);
}

FAST_HANDLE(FST) {
    biscuit::GPR top = rec.getTOP();
    biscuit::FPR st0 = rec.getST(top, 0);
    rec.setST(top, &operands[0], st0);
}

FAST_HANDLE(FXCH) {
    u8 index = operands[0].reg.value - ZYDIS_REGISTER_ST0;
    ASSERT(index >= 1 && index <= 7);
    biscuit::GPR top = rec.getTOP();
    biscuit::FPR st0 = rec.getST(top, 0);
    biscuit::FPR sti = rec.getST(top, index);
    rec.setST(top, 0, sti);
    rec.setST(top, index, st0);
}

FAST_HANDLE(FSTP) {
    biscuit::GPR top = rec.getTOP();
    biscuit::FPR st0 = rec.getST(top, 0);
    rec.setST(top, &operands[0], st0);
    rec.popST(top);
}

FAST_HANDLE(FADD) {
    OP(&Assembler::FADD_D, rec, as, instruction, operands, false);
}

FAST_HANDLE(FADDP) {
    OP(&Assembler::FADD_D, rec, as, instruction, operands, true);
}

FAST_HANDLE(FSUB) {
    OP(&Assembler::FSUB_D, rec, as, instruction, operands, false);
}

FAST_HANDLE(FSUBP) {
    OP(&Assembler::FSUB_D, rec, as, instruction, operands, true);
}

FAST_HANDLE(FSQRT) {
    biscuit::GPR top = rec.getTOP();
    biscuit::FPR st0 = rec.getST(top, 0);
    as.FSQRT_D(st0, st0);
    rec.setST(top, 0, st0);
}

FAST_HANDLE(FSIN) {
    rec.writebackDirtyState();
    rec.invalidStateUntilJump();

    as.MV(a0, rec.threadStatePointer());
    rec.call((u64)felix86_fsin);
}

FAST_HANDLE(FCOS) {
    rec.writebackDirtyState();
    rec.invalidStateUntilJump();

    as.MV(a0, rec.threadStatePointer());
    rec.call((u64)felix86_fcos);
}

FAST_HANDLE(FWAIT) {
    WARN("FWAIT encountered, treating as NOP");
}

FAST_HANDLE(FPREM) {
    WARN("Unhandled instruction FPREM, no operation");
}

FAST_HANDLE(FNSTENV) {
    WARN("Unhandled instruction FNSTENV, no operation");
}

FAST_HANDLE(FNSTSW) {
    WARN("Unhandled instruction FNSTSW, no operation");
}

FAST_HANDLE(FLDENV) {
    WARN("Unhandled instruction FLDENV, no operation");
}

void FCOM(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedOperand* operands, bool pop) {
    u8 index = operands[1].reg.value - ZYDIS_REGISTER_ST0;
    ASSERT(index >= 1 && index <= 7);
    biscuit::GPR top = rec.getTOP();
    biscuit::GPR cond = rec.scratch();
    biscuit::GPR cond2 = rec.scratch();
    biscuit::FPR st0 = rec.getST(top, 0);
    biscuit::FPR sti = rec.getST(top, index);

    biscuit::GPR zf = rec.flagW(X86_REF_ZF);
    biscuit::GPR cf = rec.flagW(X86_REF_CF);

    Label less_than, equal, greater_than, unordered, end;

    // Most likely result - not unordered
    as.SB(x0, offsetof(ThreadState, pf), rec.threadStatePointer());

    as.FEQ_D(cond, st0, st0);
    as.FEQ_D(cond2, sti, sti);
    as.AND(cond, cond, cond2);
    as.BEQZ(cond, &unordered);

    as.FLT_D(cond, st0, sti);
    as.BNEZ(cond, &less_than);

    as.FLT_D(cond, sti, st0);
    as.BNEZ(cond, &greater_than);

    // Implicit fallthrough for when comparison is equal (not less than, not greater than, not unordered)
    as.LI(zf, 1);
    as.LI(cf, 0);
    as.J(&end);

    as.Bind(&greater_than);
    as.LI(zf, 0);
    as.LI(cf, 0);
    as.J(&end);

    as.Bind(&unordered);
    as.LI(zf, 1);
    as.LI(cf, 1);
    as.SB(cf, offsetof(ThreadState, pf), rec.threadStatePointer());
    as.J(&end);

    as.Bind(&less_than);
    as.LI(zf, 0);
    as.LI(cf, 1);

    as.Bind(&end);

    if (pop) {
        rec.popST(top);
    }
}

// We don't support exceptions ATM, same as FUCOMI
FAST_HANDLE(FCOMI) {
    FCOM(rec, meta, as, operands, false);
}

FAST_HANDLE(FUCOMI) {
    FCOM(rec, meta, as, operands, false);
}

FAST_HANDLE(FCOMIP) {
    FCOM(rec, meta, as, operands, true);
}

FAST_HANDLE(FUCOMIP) {
    FCOM(rec, meta, as, operands, true);
}

FAST_HANDLE(FRNDINT) {
    biscuit::GPR top = rec.getTOP();
    biscuit::FPR st0 = rec.getST(top, 0);

    if (Extensions::Zfa) {
        as.FROUND_D(st0, st0);
    } else {
        biscuit::GPR temp = rec.scratch();
        as.FCVT_L_D(temp, st0);
        as.FCVT_D_L(st0, temp);
    }

    rec.setST(top, 0, st0);
}