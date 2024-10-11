#include "felix86/backend/allocated_register.hpp"
#include "felix86/backend/backend.hpp"
#include "felix86/backend/emitter.hpp"

// Dispatch to correct function
void Emitter::Emit(Backend& backend, const BackendInstruction& inst) {
    switch (inst.GetOpcode()) {
    // Should not exist in the backend IR representation, replaced by simpler stuff
    case IROpcode::Null: {
        UNREACHABLE();
    }
    case IROpcode::Phi: {
        UNREACHABLE();
    }
    case IROpcode::SetGuest: {
        UNREACHABLE();
    }
    case IROpcode::GetGuest: {
        UNREACHABLE();
    }
    case IROpcode::Count: {
        UNREACHABLE();
    }
    case IROpcode::LoadGuestFromMemory: {
        UNREACHABLE();
    }
    case IROpcode::StoreGuestToMemory: {
        UNREACHABLE();
    }
    case IROpcode::Comment: {
        UNREACHABLE();
    }
    case IROpcode::GetThreadStatePointer: {
        UNREACHABLE();
    }

    case IROpcode::SetExitReason: {
        EmitSetExitReason(backend, inst.GetImmediateData());
        break;
    }

    case IROpcode::Mov: {
        auto Rd = _RegWO_(inst.GetAllocation());
        auto Rs = _RegRO_(inst.GetOperand(0));
        if (inst.GetAllocation().IsGPR() || inst.GetAllocation().IsSpilled()) {
            EmitMov(backend, Rd.AsGPR(), Rs.AsGPR());
        } else if (inst.GetAllocation().IsFPR()) {
            EmitMov(backend, Rd.AsFPR(), Rs.AsFPR());
        } else if (inst.GetAllocation().IsVec()) {
            EmitMov(backend, Rd.AsVec(), Rs.AsVec());
        } else {
            UNREACHABLE();
        }
        break;
    }

    case IROpcode::ReadByteRelative: {
        EmitReadByteRelative(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), inst.GetImmediateData());
        break;
    }

    case IROpcode::ReadQWordRelative: {
        EmitReadQWordRelative(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), inst.GetImmediateData());
        break;
    }

    case IROpcode::WriteByteRelative: {
        EmitWriteByteRelative(backend, _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)), inst.GetImmediateData());
        break;
    }

    case IROpcode::WriteQWordRelative: {
        EmitWriteQWordRelative(backend, _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)), inst.GetImmediateData());
        break;
    }

    case IROpcode::WriteXmmWordRelative: {
        UNIMPLEMENTED();
        break;
    }

    case IROpcode::Immediate: {
        if (inst.GetImmediateData() == 0) {
            ERROR("Immediate data is 0, should've been allocated to $zero");
        }

        EmitImmediate(backend, _RegWO_(inst.GetAllocation()), inst.GetImmediateData());
        break;
    }
    case IROpcode::Rdtsc: {
        EmitRdtsc(backend);
        break;
    }

    case IROpcode::Syscall: {
        EmitSyscall(backend);
        break;
    }

    case IROpcode::Cpuid: {
        EmitCpuid(backend);
        break;
    }

    case IROpcode::Div128: {
        EmitDiv128(backend, _RegRO_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::Divu128: {
        EmitDivu128(backend, _RegRO_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::ReadByte: {
        EmitReadByte(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::ReadWord: {
        EmitReadWord(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::ReadDWord: {
        EmitReadDWord(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::ReadQWord: {
        EmitReadQWord(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::ReadXmmWord: {
        EmitReadXmmWord(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)));
        break;
    }
    case IROpcode::Sext8: {
        EmitSext8(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::Sext16: {
        EmitSext16(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::Sext32: {
        EmitSext32(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::Not: {
        EmitNot(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::Neg: {
        EmitNeg(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::Clz: {
        EmitClz(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::Ctzh: {
        EmitCtzh(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::Ctzw: {
        EmitCtzw(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::Ctz: {
        EmitCtz(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::Parity: {
        EmitParity(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::Add: {
        EmitAdd(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Addi: {
        EmitAddi(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), inst.GetImmediateData());
        break;
    }

    case IROpcode::AmoAdd8: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoAdd8(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoAdd16: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoAdd16(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoAdd32: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoAdd32(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoAdd64: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoAdd64(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoAnd8: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoAnd8(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoAnd16: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoAnd16(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoAnd32: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoAnd32(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoAnd64: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoAnd64(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoOr8: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoOr8(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoOr16: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoOr16(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoOr32: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoOr32(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoOr64: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoOr64(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoXor8: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoXor8(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoXor16: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoXor16(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoXor32: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoXor32(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoXor64: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoXor64(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoSwap8: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoSwap8(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoSwap16: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoSwap16(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoSwap32: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoSwap32(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoSwap64: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoSwap64(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoCAS8: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoCAS8(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)), _RegRO_(inst.GetOperand(2)),
                    ordering);
        break;
    }

    case IROpcode::AmoCAS16: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoCAS16(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)), _RegRO_(inst.GetOperand(2)),
                     ordering);
        break;
    }

    case IROpcode::AmoCAS32: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoCAS32(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)), _RegRO_(inst.GetOperand(2)),
                     ordering);
        break;
    }

    case IROpcode::AmoCAS64: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoCAS64(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)), _RegRO_(inst.GetOperand(2)),
                     ordering);
        break;
    }

    case IROpcode::AmoCAS128: {
        UNIMPLEMENTED();
        break;
    }

    case IROpcode::Sub: {
        EmitSub(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::And: {
        EmitAnd(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Or: {
        EmitOr(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Xor: {
        EmitXor(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::ShiftLeft: {
        EmitShiftLeft(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::ShiftRight: {
        EmitShiftRight(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::ShiftRightArithmetic: {
        EmitShiftRightArithmetic(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Mul: {
        EmitMul(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Mulh: {
        EmitMulh(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Mulhu: {
        EmitMulhu(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Div: {
        EmitDiv(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Divu: {
        EmitDivu(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Rem: {
        EmitRem(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Remu: {
        EmitRemu(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Divw: {
        EmitDivw(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Divuw: {
        EmitDivuw(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Remw: {
        EmitRemw(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Remuw: {
        EmitRemuw(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Equal: {
        EmitEqual(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::NotEqual: {
        EmitNotEqual(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::SetLessThanSigned: {
        EmitSetLessThanSigned(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::SetLessThanUnsigned: {
        EmitSetLessThanUnsigned(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::LeftRotate8: {
        EmitLeftRotate8(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::LeftRotate16: {
        EmitLeftRotate16(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::LeftRotate32: {
        EmitLeftRotate32(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::LeftRotate64: {
        EmitLeftRotate64(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Select: {
        EmitSelect(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)), _RegRO_(inst.GetOperand(2)));
        break;
    }

    case IROpcode::CastVectorFromInteger: {
        EmitCastVectorFromInteger(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::CastIntegerFromVector: {
        EmitCastIntegerFromVector(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::VInsertInteger: {
        EmitVInsertInteger(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)), inst.GetImmediateData());
        break;
    }

    case IROpcode::VExtractInteger: {
        EmitVExtractInteger(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), inst.GetImmediateData());
        break;
    }

    case IROpcode::WriteByte: {
        EmitWriteByte(backend, _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::WriteWord: {
        EmitWriteWord(backend, _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::WriteDWord: {
        EmitWriteDWord(backend, _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::WriteQWord: {
        EmitWriteQWord(backend, _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::WriteXmmWord: {
        EmitWriteXmmWord(backend, _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VPackedShuffleDWord: {
        EmitVPackedShuffleDWord(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), inst.GetImmediateData());
        break;
    }

    case IROpcode::VAnd: {
        EmitVAnd(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VOr: {
        EmitVOr(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VXor: {
        EmitVXor(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VShiftRight: {
        EmitVShiftRight(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VShiftLeft: {
        EmitVShiftLeft(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VUnpackByteLow: {
        EmitVUnpackByteLow(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VUnpackWordLow: {
        EmitVUnpackWordLow(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VUnpackDWordLow: {
        EmitVUnpackDWordLow(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VUnpackQWordLow: {
        EmitVUnpackQWordLow(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VPackedSubByte: {
        EmitVPackedSubByte(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VPackedAddQWord: {
        EmitVPackedAddQWord(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VPackedEqualByte: {
        EmitVPackedEqualByte(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VPackedEqualWord: {
        EmitVPackedEqualWord(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VPackedEqualDWord: {
        EmitVPackedEqualDWord(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VPackedMinByte: {
        EmitVPackedMinByte(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VMoveByteMask: {
        EmitVMoveByteMask(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::VZext64: {
        EmitVZext64(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)));
        break;
    }
    }

    backend.ReleaseScratchRegs();
}
