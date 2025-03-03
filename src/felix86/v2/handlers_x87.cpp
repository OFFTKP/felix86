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
    biscuit::GPR value = rec.getOperandGPR(&operands[1]);
    as.FCVT_D_L(ftemp, value);
    rec.pushST(top, ftemp);
}

FAST_HANDLE(FSTP) {
    WARN("FSTP, treating as NOP");
}

FAST_HANDLE(FSUBP) {
    WARN("FSUBP, treating as NOP");
}