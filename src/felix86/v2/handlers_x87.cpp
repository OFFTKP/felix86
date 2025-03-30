#include <cmath>
#include <Zydis/Zydis.h>
#include "felix86/v2/recompiler.hpp"

#define FAST_HANDLE(name)                                                                                                                            \
    void fast_##name(Recompiler& rec, HostAddress rip, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands)

FAST_HANDLE(FLD) {
    if (operands[0].size == 80) {
        biscuit::GPR address = rec.lea(&operands[0]);
        rec.writebackDirtyState();
        rec.invalidStateUntilJump();
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

    ZydisDecodedOperand* result_operand = &operands[0];

    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        // Funnily, when the operand is memory the operation happens rhs op lhs
        std::swap(lhs, rhs);
        result_operand = &operands[1];
    }

    biscuit::FPR result = rec.scratchFPR();
    (as.*func)(result, lhs, rhs, RMode::DYN);
    rec.setST(top, result_operand, result);

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

FAST_HANDLE(FMUL) {
    OP(&Assembler::FMUL_D, rec, as, instruction, operands, false);
}

FAST_HANDLE(FMULP) {
    OP(&Assembler::FMUL_D, rec, as, instruction, operands, true);
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
    rec.writebackDirtyState();
    rec.invalidStateUntilJump();

    as.MV(a0, rec.threadStatePointer());
    rec.call((u64)felix86_fprem);
}

FAST_HANDLE(FNSTENV) {
    WARN("Unhandled instruction FNSTENV, no operation");
}

FAST_HANDLE(FNSTSW) {
    biscuit::GPR temp = rec.scratch();
    as.LWU(temp, offsetof(ThreadState, fpu_sw), rec.threadStatePointer());
    rec.setOperandGPR(&operands[0], temp);
}

FAST_HANDLE(FLDENV) {
    WARN("Unhandled instruction FLDENV, no operation");
}

void FIST(Recompiler& rec, HostAddress rip, Assembler& as, ZydisDecodedOperand* operands, bool pop, RMode mode = RMode::DYN) {
    biscuit::GPR top = rec.getTOP();
    biscuit::FPR st0 = rec.getST(top, 0);
    biscuit::GPR address = rec.lea(&operands[0]);
    biscuit::GPR integer = rec.scratch();

    if (operands[0].size == 16) {
        as.FCVT_W_D(integer, st0, mode);
        rec.writeMemory(integer, address, 0, X86_SIZE_WORD);
    } else if (operands[0].size == 32) {
        as.FCVT_W_D(integer, st0, mode);
        rec.writeMemory(integer, address, 0, X86_SIZE_DWORD);
    } else if (operands[0].size == 32) {
        as.FCVT_L_D(integer, st0, mode);
        rec.writeMemory(integer, address, 0, X86_SIZE_QWORD);
    }

    if (pop) {
        rec.popST(top);
    }
}

FAST_HANDLE(FIST) {
    FIST(rec, rip, as, operands, false);
}

FAST_HANDLE(FISTP) {
    FIST(rec, rip, as, operands, true);
}

FAST_HANDLE(FISTTP) {
    FIST(rec, rip, as, operands, true, RMode::RTZ);
}

void FCOM(Recompiler& rec, HostAddress rip, Assembler& as, ZydisDecodedOperand* operands, bool pop) {
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
    FCOM(rec, rip, as, operands, false);
}

FAST_HANDLE(FUCOMI) {
    FCOM(rec, rip, as, operands, false);
}

FAST_HANDLE(FCOMIP) {
    FCOM(rec, rip, as, operands, true);
}

FAST_HANDLE(FUCOMIP) {
    FCOM(rec, rip, as, operands, true);
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

FAST_HANDLE(FLD1) {
    biscuit::GPR top = rec.getTOP();
    biscuit::FPR st = rec.scratchFPR();

    if (Extensions::Zfa) {
        as.FLI_D(st, 1.0);
    } else {
        biscuit::GPR temp = rec.scratch();
        as.LI(temp, 0x3FF0000000000000ull);
        as.FMV_D_X(st, temp);
    }

    rec.pushST(top, st);
}

FAST_HANDLE(FLDL2T) {
    constexpr u64 value = 0x400A'934F'0979'A371ull;
    biscuit::GPR top = rec.getTOP();
    biscuit::FPR st = rec.scratchFPR();
    biscuit::GPR temp = rec.scratch();
    as.LI(temp, value);
    as.FMV_D_X(st, temp);
    rec.pushST(top, st);
}

FAST_HANDLE(FLDL2E) {
    constexpr u64 value = 0x3FF7'1547'652B'82FEull;
    biscuit::GPR top = rec.getTOP();
    biscuit::FPR st = rec.scratchFPR();
    biscuit::GPR temp = rec.scratch();
    as.LI(temp, value);
    as.FMV_D_X(st, temp);
    rec.pushST(top, st);
}

FAST_HANDLE(FLDPI) {
    constexpr u64 value = 0x4009'21FB'5444'2D18ull;
    biscuit::GPR top = rec.getTOP();
    biscuit::FPR st = rec.scratchFPR();
    biscuit::GPR temp = rec.scratch();
    as.LI(temp, value);
    as.FMV_D_X(st, temp);
    rec.pushST(top, st);
}

FAST_HANDLE(FLDLG2) {
    constexpr u64 value = 0x3FD3'4413'509F'79FFull;
    biscuit::GPR top = rec.getTOP();
    biscuit::FPR st = rec.scratchFPR();
    biscuit::GPR temp = rec.scratch();
    as.LI(temp, value);
    as.FMV_D_X(st, temp);
    rec.pushST(top, st);
}

FAST_HANDLE(FLDLN2) {
    constexpr u64 value = 0x3FE6'2E42'FEFA'39EFull;
    biscuit::GPR top = rec.getTOP();
    biscuit::FPR st = rec.scratchFPR();
    biscuit::GPR temp = rec.scratch();
    as.LI(temp, value);
    as.FMV_D_X(st, temp);
    rec.pushST(top, st);
}

FAST_HANDLE(FLDZ) {
    biscuit::GPR top = rec.getTOP();
    biscuit::FPR st = rec.scratchFPR();
    as.FMV_D_X(st, x0);
    rec.pushST(top, st);
}

FAST_HANDLE(FNSTCW) {
    WARN("FNSTCW is not implemented, ignoring");
}

FAST_HANDLE(FLDCW) {
    WARN("FLDCW is not implemented, ignoring");
}
