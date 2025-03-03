#include <Zydis/Zydis.h>
#include "felix86/v2/recompiler.hpp"

#define FAST_HANDLE(name)                                                                                                                            \
    void fast_##name(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands)

FAST_HANDLE(FLD) {
    biscuit::GPR top = rec.getTOP();
    biscuit::FPR st = rec.getST(top, &operands[0]);
    rec.pushST(top, st);
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
    biscuit::FPR lhs, rhs;
    if (instruction.operand_count == 1) {
        lhs = rec.getST(top, 0);
        rhs = rec.getST(top, &operands[0]);
    } else {
        lhs = rec.getST(top, &operands[0]);
        rhs = rec.getST(top, &operands[1]);
    }

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

FAST_HANDLE(FSTP) {
    biscuit::GPR top = rec.getTOP();
    biscuit::FPR st0 = rec.getST(top, 0);
    rec.setST(top, &operands[0], st0);
    rec.popST(top);
}

FAST_HANDLE(FSUB) {
    OP(&Assembler::FSUB_D, rec, as, instruction, operands, false);
}

FAST_HANDLE(FSUBP) {
    OP(&Assembler::FSUB_D, rec, as, instruction, operands, true);
}