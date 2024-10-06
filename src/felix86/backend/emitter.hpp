#pragma once

#include "biscuit/assembler.hpp"
#include "felix86/ir/instruction.hpp"

struct Backend;

struct Emitter {
    static void Emit(Backend& backend, const IRInstruction& instruction);
    static void EmitJump(Backend& backend, void* target);
    static void EmitJumpConditional(Backend& backend, const IRInstruction& condition, void* target_true, void* target_false);

private:
    static void EmitLoadGuestFromMemory(Backend&, const IRInstruction&);
    static void EmitStoreGuestToMemory(Backend&, const IRInstruction&);
    static void EmitPushHost(Backend&, const IRInstruction&);
    static void EmitPopHost(Backend&, const IRInstruction&);
    static void EmitImmediate(Backend&, biscuit::GPR, u64);
    static void EmitRdtsc(Backend&);
    static void EmitSyscall(Backend&);
    static void EmitCpuid(Backend&);
    static void EmitSext8(Backend&, biscuit::GPR, biscuit::GPR);
    static void EmitSext16(Backend&, biscuit::GPR, biscuit::GPR);
    static void EmitSext32(Backend&, biscuit::GPR, biscuit::GPR);
    static void EmitClz(Backend&, biscuit::GPR, biscuit::GPR);
    static void EmitCtz(Backend&, biscuit::GPR, biscuit::GPR);
    static void EmitNot(Backend&, biscuit::GPR, biscuit::GPR);
    static void EmitPopcount(Backend&, biscuit::GPR, biscuit::GPR);
    static void EmitReadByte(Backend&, biscuit::GPR, biscuit::GPR);
    static void EmitReadWord(Backend&, biscuit::GPR, biscuit::GPR);
    static void EmitReadDWord(Backend&, biscuit::GPR, biscuit::GPR);
    static void EmitReadQWord(Backend&, biscuit::GPR, biscuit::GPR);
    static void EmitReadXmmWord(Backend&, biscuit::Vec, biscuit::GPR);
    static void EmitDiv128(Backend&, biscuit::GPR);
    static void EmitDivu128(Backend&, biscuit::GPR);
    static void EmitWriteByte(Backend&, biscuit::GPR, biscuit::GPR);
    static void EmitWriteWord(Backend&, biscuit::GPR, biscuit::GPR);
    static void EmitWriteDWord(Backend&, biscuit::GPR, biscuit::GPR);
    static void EmitWriteQWord(Backend&, biscuit::GPR, biscuit::GPR);
    static void EmitWriteXmmWord(Backend&, biscuit::GPR, biscuit::Vec);
    static void EmitAdd(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitSub(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitAnd(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitOr(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitXor(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitEqual(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitNotEqual(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitIGreaterThan(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitILessThan(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitUGreaterThan(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitULessThan(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitShiftLeft(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitShiftRight(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitShiftRightArithmetic(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitLeftRotate8(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitLeftRotate16(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitLeftRotate32(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitLeftRotate64(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitDiv(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitDivu(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitDivw(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitDivuw(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitRem(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitRemu(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitRemw(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitRemuw(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitMul(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitMulh(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitMulhu(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitSelect(Backend&, biscuit::GPR, biscuit::GPR, biscuit::GPR, biscuit::GPR);
    static void EmitCastIntegerToVector(Backend&, biscuit::Vec, biscuit::GPR);
    static void EmitCastVectorToInteger(Backend&, biscuit::GPR, biscuit::Vec);
    static void EmitVInsertInteger(Backend&, biscuit::Vec, biscuit::GPR, biscuit::Vec, u64);
    static void EmitVExtractInteger(Backend&, biscuit::GPR, biscuit::Vec, u64);
    static void EmitVPackedShuffleDWord(Backend&, biscuit::Vec, biscuit::Vec, u64);
    static void EmitVMoveByteMask(Backend&, biscuit::Vec, biscuit::Vec);
    static void EmitVUnpackByteLow(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec);
    static void EmitVUnpackWordLow(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec);
    static void EmitVUnpackDWordLow(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec);
    static void EmitVUnpackQWordLow(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec);
    static void EmitVAnd(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec);
    static void EmitVOr(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec);
    static void EmitVXor(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec);
    static void EmitVShiftRight(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec);
    static void EmitVShiftLeft(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec);
    static void EmitVPackedSubByte(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec);
    static void EmitVPackedAddQWord(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec);
    static void EmitVPackedEqualByte(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec);
    static void EmitVPackedEqualWord(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec);
    static void EmitVPackedEqualDWord(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec);
    static void EmitVPackedMinByte(Backend&, biscuit::Vec, biscuit::Vec, biscuit::Vec);
    static void EmitVZext64(Backend&, biscuit::Vec, biscuit::Vec);
};