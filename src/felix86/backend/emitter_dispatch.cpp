#include "felix86/backend/allocated_register.hpp"
#include "felix86/backend/backend.hpp"
#include "felix86/backend/emitter.hpp"

// Dispatch to correct function
void Emitter::Emit(Backend& backend, const BackendInstruction& inst) {
    switch (inst.GetOpcode()) {
    // Should not exist in the reduced IR representation
    case IROpcode::Null:
    case IROpcode::Phi:
    case IROpcode::SetGuest:
    case IROpcode::GetGuest:
    case IROpcode::Count:
    case IROpcode::LoadGuestFromMemory:
    case IROpcode::StoreGuestToMemory:
    case IROpcode::Comment: {
        UNREACHABLE();
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

    case IROpcode::Immediate: {
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

    case IROpcode::IGreaterThan: {
        EmitIGreaterThan(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::ILessThan: {
        EmitILessThan(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::UGreaterThan: {
        EmitUGreaterThan(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::ULessThan: {
        EmitULessThan(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)), _RegRO_(inst.GetOperand(1)));
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

    case IROpcode::CastIntegerToVector: {
        EmitCastIntegerToVector(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)));
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

    case IROpcode::CastVectorToInteger: {
        EmitCastVectorToInteger(backend, _RegWO_(inst.GetAllocation()), _RegRO_(inst.GetOperand(0)));
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
