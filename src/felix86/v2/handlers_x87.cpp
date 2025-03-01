#include <Zydis/Zydis.h>
#include "felix86/v2/recompiler.hpp"

#define FAST_HANDLE(name)                                                                                                                            \
    void fast_##name(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands)

FAST_HANDLE(FLD) {
    biscuit::GPR top = rec.getTOP();
    biscuit::FPR st = rec.getST(top, &operands[0]);
    rec.pushST(top, st);
}

// FAST_HANDLE(FST) {
//     biscuit::GPR top = rec.getTOP();
//     biscuit::FPR st = rec.getST(top, &operands[0]);
//     rec.writeST(top, &operands[1], st);
// }