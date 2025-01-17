#include <Zydis/Zydis.h>
#include "felix86/v2/fast_recompiler.hpp"

#define FAST_HANDLE(name) void fast_##name(FastRecompiler& rec, u64 rip, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands)

#define AS (rec.getAssembler())

u64 getBitSize(x86_size_e size_e) {
    switch (size_e) {
    case X86_SIZE_BYTE:
        return 8;
    case X86_SIZE_WORD:
        return 16;
    case X86_SIZE_DWORD:
        return 32;
    case X86_SIZE_QWORD:
        return 64;
    case X86_SIZE_XMM:
        return 128;
    default:
        UNREACHABLE();
        return 0;
    }
}

u64 getSignMask(x86_size_e size_e) {
    u16 size = getBitSize(size_e);
    return 1ull << (size - 1);
}

FAST_HANDLE(MOV) {
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    rec.setOperandGPR(&operands[0], src);
}

FAST_HANDLE(ADD) {
    biscuit::GPR result = rec.scratch();
    biscuit::GPR src = rec.getOperandGPR(&operands[1]);
    biscuit::GPR dst = rec.getOperandGPR(&operands[0]);

    AS.ADD(result, dst, src);

    rec.setOperandGPR(&operands[0], result);

    x86_size_e size = rec.getOperandSize(&operands[0]);
    u64 sign_mask = getSignMask(size);

    if (rec.shouldEmitFlag(rip, X86_REF_CF)) {
        biscuit::GPR cf = rec.flagW(X86_REF_CF);
        rec.zext(cf, result, size);
        AS.SLTU(cf, cf, dst);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_PF)) {
        if (Extensions::B) {
            biscuit::GPR pf = rec.flagW(X86_REF_PF);
            AS.ANDI(pf, result, 0xFF);
            AS.CPOPW(pf, pf);
            AS.ANDI(pf, pf, 1);
            AS.XORI(pf, pf, 1);
        } else {
            ERROR("This needs B extension");
        }
    }

    if (rec.shouldEmitFlag(rip, X86_REF_AF)) {
        biscuit::GPR af = rec.flagW(X86_REF_AF);
        biscuit::GPR scratch = rec.scratch();
        AS.ANDI(af, result, 0xF);
        AS.ANDI(scratch, dst, 0xF);
        AS.SLTU(af, af, scratch);
        rec.popScratch();
    }

    if (rec.shouldEmitFlag(rip, X86_REF_ZF)) {
        biscuit::GPR zf = rec.flagW(X86_REF_ZF);
        AS.SEQZ(zf, result);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_SF)) {
        biscuit::GPR sf = rec.flagW(X86_REF_SF);
        AS.SRLI(sf, result, getBitSize(size) - 1);
        AS.ANDI(sf, sf, 1);
    }

    if (rec.shouldEmitFlag(rip, X86_REF_OF)) {
        biscuit::GPR scratch = rec.scratch();
        biscuit::GPR of = rec.flagW(X86_REF_OF);
        AS.XOR(scratch, result, dst);
        AS.XOR(of, result, src);
        AS.AND(of, of, scratch);
        AS.LI(scratch, sign_mask);
        AS.AND(of, of, scratch);
        AS.SNEZ(of, of);
        rec.popScratch();
    }
}

FAST_HANDLE(HLT) {
    rec.setExitReason(ExitReason::EXIT_REASON_HLT);
    rec.backToDispatcher();
    rec.stopCompiling();
}