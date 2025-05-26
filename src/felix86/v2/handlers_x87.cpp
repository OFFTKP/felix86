#include <cmath>
#include <Zydis/Zydis.h>
#include "felix86/v2/recompiler.hpp"

// TODO: optimize all mentions of VFMV + FCVT into VFCVT if possible

#define FAST_HANDLE(name)                                                                                                                            \
    void fast_##name(Recompiler& rec, u64 rip, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands)

FAST_HANDLE(FLD) {
    if (operands[0].size == 80 && operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        biscuit::GPR address = rec.lea(&operands[0]);
        rec.writebackState();
        as.MV(a0, address);
        rec.call((u64)f80_to_64);
        rec.restoreState();
        rec.setVectorState(SEW::E64, 1);
        biscuit::Vec temp = rec.scratchVec();
        as.VFMV_SF(temp, fa0); // push return value
        rec.pushX87(temp);
    } else {
        biscuit::Vec st = rec.getST(&operands[0]);
        biscuit::Vec temp = rec.scratchVec();
        rec.setVectorState(SEW::E64, 1);
        as.VMV1R(temp, st); // move to temp because getST could return allocated Vec
        rec.pushX87(temp);
    }
}

FAST_HANDLE(FILD) {
    biscuit::Vec ftemp = rec.scratchVec();
    biscuit::GPR value = rec.getOperandGPR(&operands[0]);
    switch (operands[0].size) {
    case 16: {
        rec.sext(value, value, X86_SIZE_WORD);
        biscuit::FPR fpr = rec.scratchFPR();
        rec.setVectorState(SEW::E64, 1);
        as.FCVT_D_W(fpr, value);
        as.VFMV_SF(ftemp, fpr);
        break;
    }
    case 32: {
        biscuit::FPR fpr = rec.scratchFPR();
        rec.setVectorState(SEW::E64, 1);
        as.FCVT_D_W(fpr, value);
        as.VFMV_SF(ftemp, fpr);
        break;
    }
    case 64: {
        biscuit::FPR fpr = rec.scratchFPR();
        rec.setVectorState(SEW::E64, 1);
        as.FCVT_D_L(fpr, value);
        as.VFMV_SF(ftemp, fpr);
        break;
    }
    default: {
        UNREACHABLE();
    }
    }
    rec.pushX87(ftemp);
}

void OP(void (Assembler::*func)(Vec, Vec, Vec, VecMask), Recompiler& rec, Assembler& as, ZydisDecodedInstruction& instruction,
        ZydisDecodedOperand* operands, bool pop, bool reverse = false) {
    biscuit::Vec lhs = rec.getST(&operands[0]);
    biscuit::Vec rhs = rec.getST(&operands[1]);

    ZydisDecodedOperand* result_operand = &operands[0];

    if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        // Funnily, when the operand is memory the operation happens rhs op lhs
        std::swap(lhs, rhs);
        result_operand = &operands[1];
    }

    // TODO: don't use a separate Vec here
    biscuit::Vec result;
    rec.setVectorState(SEW::E64, 1);
    if (!reverse) {
        (as.*func)(lhs, lhs, rhs, VecMask::No);
        result = lhs;
    } else {
        (as.*func)(lhs, rhs, lhs, VecMask::No);
        result = lhs;
    }
    rec.setST(result_operand, result);

    if (pop) {
        rec.popX87();
    }
}

FAST_HANDLE(FDIV) {
    OP(&Assembler::VFDIV, rec, as, instruction, operands, false);
}

FAST_HANDLE(FDIVP) {
    OP(&Assembler::VFDIV, rec, as, instruction, operands, true);
}

FAST_HANDLE(FIDIV) {
    biscuit::Vec st0 = rec.getST(0);
    biscuit::GPR integer = rec.getOperandGPR(&operands[0]);
    ASSERT(operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY);
    biscuit::Vec scratch = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();
    biscuit::FPR fpr = rec.scratchFPR();

    if (operands[0].size == 16) {
        rec.sext(integer, integer, X86_SIZE_WORD);
    }

    rec.setVectorState(SEW::E64, 1);
    as.FCVT_D_W(fpr, integer);
    as.VFMV_SF(scratch, fpr);
    as.VFDIV(result, st0, scratch);

    rec.setST(0, result);
}

FAST_HANDLE(FDIVR) {
    OP(&Assembler::VFDIV, rec, as, instruction, operands, false, true);
}

FAST_HANDLE(FDIVRP) {
    OP(&Assembler::VFDIV, rec, as, instruction, operands, true, true);
}

FAST_HANDLE(FIDIVR) {
    biscuit::Vec st0 = rec.getST(0);
    biscuit::GPR integer = rec.getOperandGPR(&operands[0]);
    ASSERT(operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY);
    biscuit::Vec scratch = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();
    biscuit::FPR fpr = rec.scratchFPR();

    if (operands[0].size == 16) {
        rec.sext(integer, integer, X86_SIZE_WORD);
    }

    rec.setVectorState(SEW::E64, 1);
    as.FCVT_D_W(fpr, integer);
    as.VFMV_SF(scratch, fpr);
    as.VFDIV(result, scratch, st0);

    rec.setST(0, result);
}

FAST_HANDLE(FMUL) {
    OP(&Assembler::VFMUL, rec, as, instruction, operands, false);
}

FAST_HANDLE(FMULP) {
    OP(&Assembler::VFMUL, rec, as, instruction, operands, true);
}

FAST_HANDLE(FIMUL) {
    biscuit::Vec st0 = rec.getST(0);
    biscuit::GPR integer = rec.getOperandGPR(&operands[0]);
    ASSERT(operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY);
    biscuit::Vec scratch = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();

    if (operands[0].size == 16) {
        rec.sext(integer, integer, X86_SIZE_WORD);
    }

    biscuit::FPR fpr = rec.scratchFPR();
    rec.setVectorState(SEW::E64, 1);
    as.FCVT_D_W(fpr, integer);
    as.VFMV_SF(scratch, fpr);
    as.VFMUL(result, st0, scratch);

    rec.setST(0, result);
}

FAST_HANDLE(FST) {
    biscuit::Vec st0 = rec.getST(0);
    rec.setST(&operands[0], st0);
}

FAST_HANDLE(FXCH) {
    u8 index = operands[0].reg.value - ZYDIS_REGISTER_ST0;
    ASSERT(index >= 1 && index <= 7);
    biscuit::Vec st0 = rec.getST(0);
    biscuit::Vec sti = rec.getST(index);
    biscuit::Vec temp = rec.scratchVec();
    rec.setVectorState(SEW::E64, 1);
    as.VMV1R(temp, st0);
    rec.setST(0, sti);
    rec.setST(index, temp);
}

FAST_HANDLE(FSTP) {
    biscuit::Vec st0 = rec.getST(0);
    rec.setST(&operands[0], st0);
    rec.popX87();
}

FAST_HANDLE(FADD) {
    OP(&Assembler::VFADD, rec, as, instruction, operands, false);
}

FAST_HANDLE(FADDP) {
    OP(&Assembler::VFADD, rec, as, instruction, operands, true);
}

FAST_HANDLE(FIADD) {
    biscuit::Vec st0 = rec.getST(0);
    biscuit::GPR integer = rec.getOperandGPR(&operands[0]);
    ASSERT(operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY);
    biscuit::Vec scratch = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();

    if (operands[0].size == 16) {
        rec.sext(integer, integer, X86_SIZE_WORD);
    }

    biscuit::FPR fpr = rec.scratchFPR();
    rec.setVectorState(SEW::E64, 1);
    as.FCVT_D_W(fpr, integer);
    as.VFMV_SF(scratch, fpr);
    as.VFADD(result, st0, scratch);

    rec.setST(0, result);
}

FAST_HANDLE(FSUB) {
    OP(&Assembler::VFSUB, rec, as, instruction, operands, false);
}

FAST_HANDLE(FSUBP) {
    OP(&Assembler::VFSUB, rec, as, instruction, operands, true);
}

FAST_HANDLE(FISUB) {
    biscuit::Vec st0 = rec.getST(0);
    biscuit::GPR integer = rec.getOperandGPR(&operands[0]);
    ASSERT(operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY);
    biscuit::Vec scratch = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();

    if (operands[0].size == 16) {
        rec.sext(integer, integer, X86_SIZE_WORD);
    }

    biscuit::FPR fpr = rec.scratchFPR();
    rec.setVectorState(SEW::E64, 1);
    as.FCVT_D_W(fpr, integer);
    as.VFMV_SF(scratch, fpr);
    as.VFSUB(result, st0, scratch);

    rec.setST(0, result);
}

FAST_HANDLE(FSUBR) {
    OP(&Assembler::VFSUB, rec, as, instruction, operands, false, true);
}

FAST_HANDLE(FSUBRP) {
    OP(&Assembler::VFSUB, rec, as, instruction, operands, true, true);
}

FAST_HANDLE(FISUBR) {
    biscuit::Vec st0 = rec.getST(0);
    biscuit::GPR integer = rec.getOperandGPR(&operands[0]);
    ASSERT(operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY);
    biscuit::Vec scratch = rec.scratchVec();
    biscuit::Vec result = rec.scratchVec();

    if (operands[0].size == 16) {
        rec.sext(integer, integer, X86_SIZE_WORD);
    }

    biscuit::FPR fpr = rec.scratchFPR();
    rec.setVectorState(SEW::E64, 1);
    as.FCVT_D_W(fpr, integer);
    as.VFMV_SF(scratch, fpr);
    as.VFSUB(result, scratch, st0);

    rec.setST(0, result);
}

FAST_HANDLE(FSQRT) {
    biscuit::Vec st0 = rec.getST(0);
    rec.setVectorState(SEW::E64, 1);
    as.VFSQRT(st0, st0);
    rec.setST(0, st0);
}

FAST_HANDLE(FSIN) {
    rec.writebackState();
    as.MV(a0, rec.threadStatePointer());
    rec.call((u64)felix86_fsin);
    rec.restoreState();
}

FAST_HANDLE(FCOS) {
    rec.writebackState();
    as.MV(a0, rec.threadStatePointer());
    rec.call((u64)felix86_fcos);
    rec.restoreState();
}

FAST_HANDLE(FPATAN) {
    rec.writebackState();
    as.MV(a0, rec.threadStatePointer());
    rec.call((u64)felix86_fpatan);
    rec.restoreState();

    // FPATAN also pops the stack
    rec.popX87();
}

FAST_HANDLE(FWAIT) {
    WARN("FWAIT encountered, treating as NOP");
}

FAST_HANDLE(FPREM) {
    rec.writebackState();
    as.MV(a0, rec.threadStatePointer());
    rec.call((u64)felix86_fprem);
    rec.restoreState();
}

FAST_HANDLE(F2XM1) {
    rec.writebackState();
    as.MV(a0, rec.threadStatePointer());
    rec.call((u64)felix86_f2xm1);
    rec.restoreState();
}

FAST_HANDLE(FSCALE) {
    rec.writebackState();
    as.MV(a0, rec.threadStatePointer());
    rec.call((u64)felix86_fscale);
    rec.restoreState();
}

FAST_HANDLE(FYL2X) {
    rec.writebackState();
    as.MV(a0, rec.threadStatePointer());
    rec.call((u64)felix86_fyl2x);
    rec.restoreState();

    // FYL2X also pops the stack
    rec.popX87();
}

FAST_HANDLE(FYL2XP1) {
    rec.writebackState();
    as.MV(a0, rec.threadStatePointer());
    rec.call((u64)felix86_fyl2xp1);
    rec.restoreState();

    // FYL2XP1 also pops the stack
    rec.popX87();
}

FAST_HANDLE(FXAM) {
    rec.writebackState();
    as.MV(a0, rec.threadStatePointer());
    rec.call((u64)felix86_fxam);
    rec.restoreState();
}

FAST_HANDLE(FNSTENV) {
    biscuit::GPR address = rec.lea(&operands[0]);
    rec.writebackState();
    as.MV(a1, address);
    as.MV(a0, rec.threadStatePointer());
    if (instruction.attributes & ZYDIS_ATTRIB_HAS_OPERANDSIZE) {
        rec.call((u64)felix86_fstenv_16);
    } else {
        rec.call((u64)felix86_fstenv_32);
    }
    rec.restoreState();
}

FAST_HANDLE(FNSTSW) {
    biscuit::GPR temp = rec.scratch();
    as.LWU(temp, offsetof(ThreadState, fpu_sw), rec.threadStatePointer());
    rec.setOperandGPR(&operands[0], temp);
}

FAST_HANDLE(FLDENV) {
    biscuit::GPR address = rec.lea(&operands[0]);
    rec.writebackState();
    as.MV(a1, address);
    as.MV(a0, rec.threadStatePointer());
    if (instruction.attributes & ZYDIS_ATTRIB_HAS_OPERANDSIZE) {
        rec.call((u64)felix86_fldenv_16);
    } else {
        rec.call((u64)felix86_fldenv_32);
    }
    rec.restoreState();
}

void FIST(Recompiler& rec, u64 rip, Assembler& as, ZydisDecodedOperand* operands, bool pop, RMode mode = RMode::DYN) {
    biscuit::Vec st0 = rec.getST(0);
    biscuit::GPR address = rec.lea(&operands[0]);
    biscuit::GPR integer = rec.scratch();

    if (operands[0].size == 16) {
        biscuit::FPR fpr = rec.scratchFPR();
        rec.setVectorState(SEW::E64, 1);
        as.VFMV_FS(fpr, st0);
        as.FCVT_W_D(integer, fpr, mode);
        rec.writeMemory(integer, address, 0, X86_SIZE_WORD);
    } else if (operands[0].size == 32) {
        biscuit::FPR fpr = rec.scratchFPR();
        rec.setVectorState(SEW::E64, 1);
        as.VFMV_FS(fpr, st0);
        as.FCVT_W_D(integer, fpr, mode);
        rec.writeMemory(integer, address, 0, X86_SIZE_DWORD);
    } else if (operands[0].size == 64) {
        biscuit::FPR fpr = rec.scratchFPR();
        rec.setVectorState(SEW::E64, 1);
        as.VFMV_FS(fpr, st0);
        as.FCVT_L_D(integer, fpr, mode);
        rec.writeMemory(integer, address, 0, X86_SIZE_QWORD);
    } else {
        UNREACHABLE();
    }

    if (pop) {
        rec.popX87();
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

void FCOMI(Recompiler& rec, Assembler& as, ZydisDecodedOperand* operands, bool pop) {
    biscuit::Vec cond = rec.scratchVec();
    biscuit::Vec cond2 = rec.scratchVec();
    biscuit::GPR cond_reg = rec.scratch();
    biscuit::Vec st0 = rec.getST(&operands[0]);
    biscuit::Vec sti = rec.getST(&operands[1]);

    biscuit::GPR zf = rec.flag(X86_REF_ZF);
    biscuit::GPR cf = rec.flag(X86_REF_CF);

    Label less_than, equal, greater_than, unordered, end;

    rec.setVectorState(SEW::E64, 1);

    // Most likely result - not unordered
    as.SB(x0, offsetof(ThreadState, pf), rec.threadStatePointer());

    // TODO: can we do this branchless too? and not move to gprs all the time
    as.VMFEQ(cond, st0, st0);
    as.VMFEQ(cond2, sti, sti);
    as.VAND(cond, cond, cond2);
    as.VMV_XS(cond_reg, cond);
    as.ANDI(cond_reg, cond_reg, 1);
    as.BEQZ(cond_reg, &unordered);

    as.VMFLT(cond, st0, sti);
    as.VMV_XS(cond_reg, cond);
    as.ANDI(cond_reg, cond_reg, 1);
    as.BNEZ(cond_reg, &less_than);

    as.VMFLT(cond, sti, st0);
    as.VMV_XS(cond_reg, cond);
    as.ANDI(cond_reg, cond_reg, 1);
    as.BNEZ(cond_reg, &greater_than);

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
        rec.popX87();
    }
}

// We don't support exceptions ATM, same as FUCOMI
FAST_HANDLE(FCOMI) {
    FCOMI(rec, as, operands, false);
}

FAST_HANDLE(FUCOMI) {
    FCOMI(rec, as, operands, false);
}

FAST_HANDLE(FCOMIP) {
    FCOMI(rec, as, operands, true);
}

FAST_HANDLE(FUCOMIP) {
    FCOMI(rec, as, operands, true);
}

void FCOM(Recompiler& rec, Assembler& as, ZydisDecodedOperand* operands, int pop_count) {
    biscuit::Vec st0, src;
    if (operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER && operands[0].reg.value == ZYDIS_REGISTER_ST0) {
        st0 = rec.getST(&operands[0]);
        src = rec.getST(&operands[1]);
    } else {
        st0 = rec.getST(&operands[1]);
        src = rec.getST(&operands[0]);
    }

    biscuit::Vec c0 = rec.scratchVec();
    biscuit::Vec c2 = rec.scratchVec();
    biscuit::Vec c3 = rec.scratchVec();
    biscuit::Vec nan1 = rec.scratchVec();
    biscuit::Vec nan2 = rec.scratchVec();

    // Branchless way of doing this
    rec.setVectorState(SEW::E64, 1);
    as.VMV(c0, 0);
    as.VMV(c2, 0);
    as.VMV(c3, 0);
    as.VMFNE(nan1, st0, st0);
    as.VMFNE(nan2, src, src);
    as.VOR(nan1, nan1, nan2);

    as.VMFLT(c0, st0, src);
    as.VMFEQ(c3, st0, src);
    // If either is NaN set all to 1s
    as.VOR(c2, c2, nan1);
    as.VOR(c0, c0, nan1);
    as.VOR(c3, c3, nan1);
    as.VAND(c2, c2, 1);
    as.VAND(c0, c0, 1);
    as.VAND(c3, c3, 1);
    as.VSLL(c2, c2, 10);
    as.VSLL(c0, c0, 8);
    as.VSLL(c3, c3, 14);
    as.VOR(c0, c0, c2);
    as.VOR(c0, c0, c3);

    rec.setVectorState(SEW::E32, 1);
    biscuit::GPR address = rec.scratch();
    as.ADDI(address, rec.threadStatePointer(), offsetof(ThreadState, fpu_sw));
    as.VSE32(c0, address);

    if (pop_count == 1) {
        rec.popX87();
    } else if (pop_count == 2) {
        // TODO: optimize me please
        rec.popX87();
        rec.popX87();
    }
}

FAST_HANDLE(FCOM) {
    FCOM(rec, as, operands, 0);
}

FAST_HANDLE(FCOMP) {
    FCOM(rec, as, operands, 1);
}

FAST_HANDLE(FCOMPP) {
    FCOM(rec, as, operands, 2);
}

FAST_HANDLE(FUCOM) {
    FCOM(rec, as, operands, 0);
}

FAST_HANDLE(FUCOMP) {
    FCOM(rec, as, operands, 1);
}

FAST_HANDLE(FUCOMPP) {
    FCOM(rec, as, operands, 2);
}

FAST_HANDLE(FRNDINT) {
    biscuit::Vec st0 = rec.getST(0);
    biscuit::Vec temp = rec.scratchVec();
    rec.setVectorState(SEW::E64, 1);
    as.VFCVT_F_X(temp, st0);
    as.VFCVT_X_F(st0, temp);
    rec.setST(0, st0);
}

FAST_HANDLE(FCHS) {
    biscuit::Vec st0 = rec.getST(0);
    rec.setVectorState(SEW::E64, 1);
    as.VFNEG(st0, st0);
    rec.setST(0, st0);
}

FAST_HANDLE(FLD1) {
    biscuit::Vec st = rec.scratchVec();
    biscuit::GPR temp = rec.scratch();
    rec.setVectorState(SEW::E64, 1);
    as.LI(temp, 0x3FF0000000000000ull);
    as.VMV_SX(st, temp);
    rec.pushX87(st);
}

FAST_HANDLE(FLDL2T) {
    constexpr u64 value = 0x400A'934F'0979'A371ull;
    biscuit::Vec st = rec.scratchVec();
    biscuit::GPR temp = rec.scratch();
    rec.setVectorState(SEW::E64, 1);
    as.LI(temp, value);
    as.VMV_SX(st, temp);
    rec.pushX87(st);
}

FAST_HANDLE(FLDL2E) {
    constexpr u64 value = 0x3FF7'1547'652B'82FEull;
    biscuit::Vec st = rec.scratchVec();
    biscuit::GPR temp = rec.scratch();
    rec.setVectorState(SEW::E64, 1);
    as.LI(temp, value);
    as.VMV_SX(st, temp);
    rec.pushX87(st);
}

FAST_HANDLE(FLDPI) {
    constexpr u64 value = 0x4009'21FB'5444'2D18ull;
    biscuit::Vec st = rec.scratchVec();
    biscuit::GPR temp = rec.scratch();
    rec.setVectorState(SEW::E64, 1);
    as.LI(temp, value);
    as.VMV_SX(st, temp);
    rec.pushX87(st);
}

FAST_HANDLE(FLDLG2) {
    constexpr u64 value = 0x3FD3'4413'509F'79FFull;
    biscuit::Vec st = rec.scratchVec();
    biscuit::GPR temp = rec.scratch();
    rec.setVectorState(SEW::E64, 1);
    as.LI(temp, value);
    as.VMV_SX(st, temp);
    rec.pushX87(st);
}

FAST_HANDLE(FLDLN2) {
    constexpr u64 value = 0x3FE6'2E42'FEFA'39EFull;
    biscuit::Vec st = rec.scratchVec();
    biscuit::GPR temp = rec.scratch();
    rec.setVectorState(SEW::E64, 1);
    as.LI(temp, value);
    as.VMV_SX(st, temp);
    rec.pushX87(st);
}

FAST_HANDLE(FLDZ) {
    biscuit::Vec st = rec.scratchVec();
    rec.setVectorState(SEW::E64, 1);
    as.VMV_SX(st, x0);
    rec.pushX87(st);
}

FAST_HANDLE(FABS) {
    biscuit::Vec st0 = rec.getST(0);
    rec.setVectorState(SEW::E64, 1);
    as.VFABS(st0, st0);
    rec.setST(0, st0);
}

FAST_HANDLE(FNSTCW) {
    biscuit::GPR address = rec.lea(&operands[0]);
    biscuit::GPR temp = rec.scratch();
    as.LHU(temp, offsetof(ThreadState, fpu_cw), Recompiler::threadStatePointer());
    as.SH(temp, 0, address);
}

FAST_HANDLE(FLDCW) {
    biscuit::GPR address = rec.lea(&operands[0]);
    biscuit::GPR temp = rec.scratch();
    as.LHU(temp, 0, address);
    as.SH(temp, offsetof(ThreadState, fpu_cw), Recompiler::threadStatePointer());

    biscuit::GPR rc = rec.scratch();
    // Extract rounding mode from FPU control word
    as.SRLI(rc, temp, 10);
    as.ANDI(rc, rc, 0b11);

    // Here's how the rounding modes match up
    // 00 - Round to nearest (even) x86 -> 00 RISC-V
    // 01 - Round down (towards -inf) x86 -> 10 RISC-V
    // 10 - Round up (towards +inf) x86 -> 11 RISC-V
    // 11 - Round towards zero x86 -> 01 RISC-V
    // So we can shift the following bit sequence to the right and mask it
    // 01111000, shift by the rc * 2 and we get the RISC-V rounding mode
    as.SLLI(rc, rc, 1);
    as.LI(temp, 0b01111000);
    as.SRL(temp, temp, rc);
    as.ANDI(temp, temp, 0b11);
    as.FSRM(x0, temp);

    as.SB(temp, offsetof(ThreadState, rmode_x87), rec.threadStatePointer());

    rec.setFsrmSSE(false);
}

FAST_HANDLE(FNINIT) {
    biscuit::GPR temp = rec.scratch();
    as.LI(temp, 0x037F);
    as.SH(temp, offsetof(ThreadState, fpu_cw), Recompiler::threadStatePointer());

    // FINIT sets it to nearest neighbor which happens to be 0 in both x86 and RISC-V
    as.FSRM(x0);
    rec.setFsrmSSE(false);
}

void FCMOV(Recompiler& rec, Assembler& as, ZydisDecodedOperand* operands, biscuit::GPR cond) {
    biscuit::Label not_true;
    as.BEQZ(cond, &not_true);
    biscuit::Vec sti = rec.getST(&operands[1]);
    rec.setST(0, sti);
    as.Bind(&not_true);
}

FAST_HANDLE(FCMOVB) {
    biscuit::GPR cf = rec.flag(X86_REF_CF);
    FCMOV(rec, as, operands, cf);
}

FAST_HANDLE(FCMOVE) {
    biscuit::GPR zf = rec.flag(X86_REF_ZF);
    FCMOV(rec, as, operands, zf);
}

FAST_HANDLE(FCMOVBE) {
    biscuit::GPR cf = rec.flag(X86_REF_CF);
    biscuit::GPR zf = rec.flag(X86_REF_ZF);
    biscuit::GPR cond = rec.scratch();
    as.OR(cond, cf, zf);
    FCMOV(rec, as, operands, cond);
}

FAST_HANDLE(FCMOVU) {
    biscuit::GPR pf = rec.flag(X86_REF_PF);
    FCMOV(rec, as, operands, pf);
}

FAST_HANDLE(FCMOVNB) {
    biscuit::GPR cf = rec.flag(X86_REF_CF);
    biscuit::GPR cond = rec.scratch();
    as.XORI(cond, cf, 1);
    FCMOV(rec, as, operands, cond);
}

FAST_HANDLE(FCMOVNE) {
    biscuit::GPR zf = rec.flag(X86_REF_ZF);
    biscuit::GPR cond = rec.scratch();
    as.XORI(cond, zf, 1);
    FCMOV(rec, as, operands, cond);
}

FAST_HANDLE(FCMOVNBE) {
    biscuit::GPR cf = rec.flag(X86_REF_CF);
    biscuit::GPR zf = rec.flag(X86_REF_ZF);
    biscuit::GPR cond = rec.scratch();
    as.OR(cond, cf, zf);
    as.XORI(cond, cond, 1);
    FCMOV(rec, as, operands, cond);
}

FAST_HANDLE(FCMOVNU) {
    biscuit::GPR pf = rec.flag(X86_REF_PF);
    biscuit::GPR cond = rec.scratch();
    as.XORI(cond, pf, 1);
    FCMOV(rec, as, operands, cond);
}

FAST_HANDLE(FNSAVE) {
    biscuit::GPR address = rec.lea(&operands[0]);
    rec.writebackState();
    as.MV(a1, address);
    as.MV(a0, rec.threadStatePointer());
    if (instruction.attributes & ZYDIS_ATTRIB_HAS_OPERANDSIZE) {
        rec.call((u64)&felix86_fsave_16);
    } else {
        rec.call((u64)&felix86_fsave_32);
    }
    rec.restoreState();
}

FAST_HANDLE(FRSTOR) {
    biscuit::GPR address = rec.lea(&operands[0]);
    rec.writebackState();
    as.MV(a1, address);
    as.MV(a0, rec.threadStatePointer());
    if (instruction.attributes & ZYDIS_ATTRIB_HAS_OPERANDSIZE) {
        rec.call((u64)&felix86_frstor_16);
    } else {
        rec.call((u64)&felix86_frstor_32);
    }
    rec.restoreState();
}