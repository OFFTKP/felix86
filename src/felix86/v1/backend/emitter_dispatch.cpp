#include "felix86/backend/backend.hpp"
#include "felix86/backend/emitter.hpp"

struct DeferredSpill {
    DeferredSpill(Backend& backend) : backend(backend) {}
    ~DeferredSpill() {
        if (active) {
            if (alloc.IsGPR()) {
                Emitter::EmitStoreSpill(backend, alloc.AsGPR(), spill_location);
            } else if (alloc.IsVec()) {
                Emitter::EmitStoreSpill(backend, alloc.AsVec(), spill_location);
            } else {
                UNREACHABLE();
            }
        }
    }

    void Enable(Allocation alloc, u32 spill_location) {
        active = true;
        this->alloc = alloc;
        this->spill_location = spill_location;
    }

    Backend& backend;
    bool active = false;
    Allocation alloc;
    u32 spill_location;
};

auto RegR = [](Backend& bd, Assembler& as, AllocationMap& allocation_map, u32 name) -> Allocation {
    AllocationType type = allocation_map.GetAllocationType(name);
    if (type == AllocationType::GPR) {
        return allocation_map.GetAllocation(name).AsGPR();
    } else if (type == AllocationType::StaticSpillGPR) {
        biscuit::GPR reg = allocation_map.GetAllocation(name);
        Emitter::EmitLoadSpill(bd, reg, allocation_map.GetSpillLocation(name));
        return reg;
    } else if (type == AllocationType::Vec) {
        return allocation_map.GetAllocation(name).AsVec();
    } else if (type == AllocationType::StaticSpillVec) {
        biscuit::Vec reg = allocation_map.GetAllocation(name);
        Emitter::EmitLoadSpill(bd, reg, allocation_map.GetSpillLocation(name));
        return reg;
    } else {
        UNREACHABLE();
        return x0;
    }
};

auto RegW = [](DeferredSpill& def, Backend& bd, Assembler& as, AllocationMap& allocation_map, u32 name) -> Allocation {
    AllocationType type = allocation_map.GetAllocationType(name);
    if (type == AllocationType::GPR) {
        return allocation_map.GetAllocation(name).AsGPR();
    } else if (type == AllocationType::StaticSpillGPR) {
        biscuit::GPR reg = allocation_map.GetAllocation(name);
        def.Enable(reg, allocation_map.GetSpillLocation(name));
        return reg;
    } else if (type == AllocationType::Vec) {
        return allocation_map.GetAllocation(name).AsVec();
    } else if (type == AllocationType::StaticSpillVec) {
        biscuit::Vec reg = allocation_map.GetAllocation(name);
        def.Enable(reg, allocation_map.GetSpillLocation(name));
        return reg;
    } else {
        UNREACHABLE();
        return x0;
    }
};

#define _RegR_(name) RegR(backend, backend.GetAssembler(), allocation_map, name)
#define _RegW_(name) RegW(deferred_spill, backend, backend.GetAssembler(), allocation_map, name)

// Dispatch to correct function
void Emitter::Emit(Backend& backend, AllocationMap& allocation_map, const BackendBlock& block, const BackendInstruction& inst) {
    allocation_map.ResetSpillRegisters();

    DeferredSpill deferred_spill(backend);
    switch (inst.GetOpcode()) {
    // Should not exist in the backend IR representation, replaced by simpler stuff
    case IROpcode::Null: {
        UNREACHABLE();
        break;
    }
    case IROpcode::Phi: {
        UNREACHABLE();
        break;
    }
    case IROpcode::SetGuest: {
        UNREACHABLE();
        break;
    }
    case IROpcode::GetGuest: {
        UNREACHABLE();
        break;
    }
    case IROpcode::Count: {
        UNREACHABLE();
        break;
    }
    case IROpcode::LoadGuestFromMemory: {
        UNREACHABLE();
        break;
    }
    case IROpcode::StoreGuestToMemory: {
        UNREACHABLE();
        break;
    }
    case IROpcode::Comment: {
        UNREACHABLE();
        break;
    }

    case IROpcode::CallHostFunction: {
        EmitCallHostFunction(backend, inst.GetImmediateData());
        break;
    }

    case IROpcode::SetVectorStateFloat: {
        EmitSetVectorStateFloat(backend);
        break;
    }

    case IROpcode::SetVectorStateDouble: {
        EmitSetVectorStateDouble(backend);
        break;
    }

    case IROpcode::SetVectorStateFloatBytes: {
        EmitSetVectorStateFloatBytes(backend);
        break;
    }

    case IROpcode::SetVectorStateDoubleBytes: {
        EmitSetVectorStateDoubleBytes(backend);
        break;
    }

    case IROpcode::SetVectorStatePackedByte: {
        EmitSetVectorStatePackedByte(backend);
        break;
    }

    case IROpcode::SetVectorStatePackedWord: {
        EmitSetVectorStatePackedWord(backend);
        break;
    }

    case IROpcode::SetVectorStatePackedDWord: {
        EmitSetVectorStatePackedDWord(backend);
        break;
    }

    case IROpcode::SetVectorStatePackedQWord: {
        EmitSetVectorStatePackedQWord(backend);
        break;
    }

    case IROpcode::GetThreadStatePointer: {
        // Do nothing, ThreadStatePointer is already in a register
        // This static assert serves as a reminder in case something is changed and this is no longer true
        static_assert(Registers::ThreadStatePointer() == x9);
        break;
    }

    case IROpcode::SetExitReason: {
        EmitSetExitReason(backend, inst.GetImmediateData());
        break;
    }

    case IROpcode::LoadSpill: {
        auto Rd = allocation_map.GetAllocation(inst.GetName());
        u32 spill_offset = inst.GetImmediateData();
        if (Rd.IsGPR()) {
            EmitLoadSpill(backend, Rd.AsGPR(), spill_offset);
        } else if (Rd.IsVec()) {
            EmitLoadSpill(backend, Rd.AsVec(), spill_offset);
        } else {
            UNREACHABLE();
        }
        break;
    }

    case IROpcode::StoreSpill: {
        auto Rs = _RegR_(inst.GetOperand(0));
        u32 spill_offset = inst.GetImmediateData();
        if (Rs.IsGPR()) {
            EmitStoreSpill(backend, Rs.AsGPR(), spill_offset);
        } else if (Rs.IsVec()) {
            EmitStoreSpill(backend, Rs.AsVec(), spill_offset);
        } else {
            UNREACHABLE();
        }
        break;
    }

    case IROpcode::Mov: {
        auto Rd = _RegW_(inst.GetName());
        auto Rs = _RegR_(inst.GetOperand(0));
        if (Rd.IsGPR() && Rs.IsGPR()) {
            if (Rd.AsGPR() == Registers::Zero()) {
                ERROR("Name %s is zero\n", GetNameString(inst.GetName()).c_str());
            }
            EmitMov(backend, Rd.AsGPR(), Rs.AsGPR());
        } else if (Rd.IsVec() && Rs.IsVec()) {
            EmitMov(backend, Rd.AsVec(), Rs.AsVec());
        } else {
            ERROR("Rd type: %d, Rs type: %d", (int)Rd.GetAllocationType(), (int)Rs.GetAllocationType());
        }
        break;
    }

    case IROpcode::Jump: {
        Label* target = block.GetSuccessor(0)->GetLabel();
        EmitJump(backend, target);
        break;
    }

    case IROpcode::JumpConditional: {
        auto condition = _RegR_(inst.GetOperand(0));
        Label* target_true = block.GetSuccessor(0)->GetLabel();
        Label* target_false = block.GetSuccessor(1)->GetLabel();
        EmitJumpConditional(backend, condition, target_true, target_false);
        break;
    }

    case IROpcode::BackToDispatcher: {
        Assembler& as = backend.GetAssembler();
        as.LD(t0, offsetof(ThreadState, compile_next_handler), Registers::ThreadStatePointer());
        as.JR(t0);
        break;
    }

    case IROpcode::ReadByteRelative: {
        EmitReadByteRelative(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetImmediateData());
        break;
    }

    case IROpcode::ReadWordRelative: {
        EmitReadWordRelative(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetImmediateData());
        break;
    }

    case IROpcode::ReadDWordRelative: {
        EmitReadDWordRelative(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetImmediateData());
        break;
    }

    case IROpcode::ReadQWordRelative: {
        EmitReadQWordRelative(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetImmediateData());
        break;
    }

    case IROpcode::ReadXmmWordRelative: {
        EmitReadXmmWordRelative(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetImmediateData(), inst.GetVectorState());
        break;
    }

    case IROpcode::WriteByteRelative: {
        EmitWriteByteRelative(backend, _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), inst.GetImmediateData());
        break;
    }

    case IROpcode::WriteWordRelative: {
        EmitWriteWordRelative(backend, _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), inst.GetImmediateData());
        break;
    }

    case IROpcode::WriteDWordRelative: {
        EmitWriteDWordRelative(backend, _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), inst.GetImmediateData());
        break;
    }

    case IROpcode::WriteQWordRelative: {
        EmitWriteQWordRelative(backend, _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), inst.GetImmediateData());
        break;
    }

    case IROpcode::WriteXmmWordRelative: {
        EmitWriteXmmWordRelative(backend, _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), inst.GetImmediateData(), inst.GetVectorState());
        break;
    }

    case IROpcode::Immediate: {
        if (inst.GetImmediateData() == 0) {
            ASSERT(allocation_map.GetAllocation(inst.GetName()).AsGPR() == Registers::Zero());
            break;
        }

        EmitImmediate(backend, _RegW_(inst.GetName()), inst.GetImmediateData());
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
        EmitDiv128(backend, _RegR_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::Divu128: {
        EmitDivu128(backend, _RegR_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::ReadByte: {
        EmitReadByte(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::ReadWord: {
        EmitReadWord(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::ReadDWord: {
        EmitReadDWord(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::ReadQWord: {
        EmitReadQWord(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::ReadXmmWord: {
        EmitReadXmmWord(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetVectorState());
        break;
    }

    case IROpcode::Sext8: {
        EmitSext8(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::Sext16: {
        EmitSext16(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::Sext32: {
        EmitSext32(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::Not: {
        EmitNot(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::Neg: {
        EmitNeg(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::Clz: {
        EmitClz(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::Ctz: {
        EmitCtz(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::Parity: {
        EmitParity(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::Zext8: {
        EmitZext8(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::Zext16: {
        EmitZext16(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::Zext32: {
        EmitZext32(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::BSwap32: {
        EmitBSwap32(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::BSwap64: {
        EmitBSwap64(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::Add: {
        EmitAdd(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::AddShifted: {
        EmitAddShifted(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), inst.GetImmediateData());
        break;
    }

    case IROpcode::Addi: {
        EmitAddi(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetImmediateData());
        break;
    }

    case IROpcode::LoadReserved32: {
        EmitLoadReserved32(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), (biscuit::Ordering)inst.GetImmediateData());
        break;
    }

    case IROpcode::LoadReserved64: {
        EmitLoadReserved64(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), (biscuit::Ordering)inst.GetImmediateData());
        break;
    }

    case IROpcode::StoreConditional32: {
        EmitStoreConditional32(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)),
                               (biscuit::Ordering)inst.GetImmediateData());
        break;
    }

    case IROpcode::StoreConditional64: {
        EmitStoreConditional64(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)),
                               (biscuit::Ordering)inst.GetImmediateData());
        break;
    }

    case IROpcode::AmoAdd8: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoAdd8(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoAdd16: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoAdd16(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoAdd32: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoAdd32(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoAdd64: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoAdd64(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoAnd8: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoAnd8(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoAnd16: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoAnd16(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoAnd32: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoAnd32(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoAnd64: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoAnd64(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoOr8: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoOr8(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoOr16: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoOr16(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoOr32: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoOr32(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoOr64: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoOr64(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoXor8: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoXor8(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoXor16: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoXor16(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoXor32: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoXor32(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoXor64: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoXor64(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoSwap8: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoSwap8(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoSwap16: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoSwap16(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoSwap32: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoSwap32(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoSwap64: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoSwap64(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), ordering);
        break;
    }

    case IROpcode::AmoCAS8: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoCAS8(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), _RegR_(inst.GetOperand(2)), ordering);
        break;
    }

    case IROpcode::AmoCAS16: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoCAS16(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), _RegR_(inst.GetOperand(2)), ordering);
        break;
    }

    case IROpcode::AmoCAS32: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoCAS32(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), _RegR_(inst.GetOperand(2)), ordering);
        break;
    }

    case IROpcode::AmoCAS64: {
        Ordering ordering = (Ordering)(inst.GetImmediateData() & 0b11);
        EmitAmoCAS64(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), _RegR_(inst.GetOperand(2)), ordering);
        break;
    }

    case IROpcode::AmoCAS128: {
        UNIMPLEMENTED();
        break;
    }

    case IROpcode::Sub: {
        EmitSub(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::And: {
        EmitAnd(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Andi: {
        EmitAndi(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetImmediateData());
        break;
    }

    case IROpcode::Or: {
        EmitOr(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Ori: {
        EmitOri(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetImmediateData());
        break;
    }

    case IROpcode::Xor: {
        EmitXor(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Xori: {
        EmitXori(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetImmediateData());
        break;
    }

    case IROpcode::Shl: {
        EmitShl(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Shli: {
        EmitShli(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetImmediateData());
        break;
    }

    case IROpcode::Shr: {
        EmitShr(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Shri: {
        EmitShri(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetImmediateData());
        break;
    }

    case IROpcode::Sar: {
        EmitSar(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Sari: {
        EmitSari(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetImmediateData());
        break;
    }

    case IROpcode::Mul: {
        EmitMul(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Mulh: {
        EmitMulh(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Mulhu: {
        EmitMulhu(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Div: {
        EmitDiv(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Divu: {
        EmitDivu(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Rem: {
        EmitRem(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Remu: {
        EmitRemu(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Divw: {
        EmitDivw(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Divuw: {
        EmitDivuw(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Remw: {
        EmitRemw(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Remuw: {
        EmitRemuw(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Seqz: {
        EmitSeqz(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::Snez: {
        EmitSnez(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::Equal: {
        EmitEqual(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::NotEqual: {
        EmitNotEqual(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::SetLessThanSigned: {
        EmitSetLessThanSigned(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::SetLessThanUnsigned: {
        EmitSetLessThanUnsigned(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Rol32: {
        EmitRol32(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Rol64: {
        EmitRol64(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Ror32: {
        EmitRor32(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Ror64: {
        EmitRor64(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Select: {
        EmitSelect(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), _RegR_(inst.GetOperand(2)));
        break;
    }

    case IROpcode::CZeroEqz: {
        EmitCZeroEqz(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::CZeroNez: {
        EmitCZeroNez(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::IToV: {
        EmitIToV(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::VToI: {
        EmitVToI(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::VInsertInteger: {
        EmitVInsertInteger(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), inst.GetImmediateData());
        break;
    }

    case IROpcode::VExtractInteger: {
        EmitVExtractInteger(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetImmediateData());
        break;
    }

    case IROpcode::WriteByte: {
        EmitWriteByte(backend, _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::WriteWord: {
        EmitWriteWord(backend, _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::WriteDWord: {
        EmitWriteDWord(backend, _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::WriteQWord: {
        EmitWriteQWord(backend, _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::WriteXmmWord: {
        EmitWriteXmmWord(backend, _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), inst.GetVectorState());
        break;
    }

    case IROpcode::SetVMask: {
        EmitSetVMask(backend, _RegR_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::VIota: {
        EmitVIota(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetMask());
        break;
    }

    case IROpcode::VId: {
        EmitVId(backend, _RegW_(inst.GetName()));
        break;
    }

    case IROpcode::VGather: {
        EmitVGather(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), _RegR_(inst.GetOperand(2)),
                    inst.GetMask());
        break;
    }

    case IROpcode::VSplat: {
        EmitVSplat(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::VSplati: {
        EmitVSplati(backend, _RegW_(inst.GetName()), inst.GetImmediateData());
        break;
    }

    case IROpcode::VMerge: {
        EmitVMerge(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VMergei: {
        EmitVMergei(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetImmediateData());
        break;
    }

    case IROpcode::VAnd: {
        EmitVAnd(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VOr: {
        EmitVOr(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VXor: {
        EmitVXor(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VMin: {
        EmitVMin(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VMinu: {
        EmitVMinu(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VMax: {
        EmitVMax(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VMaxu: {
        EmitVMaxu(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VXori: {
        EmitVXori(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetImmediateData());
        break;
    }

    case IROpcode::VSub: {
        EmitVSub(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VAdd: {
        EmitVAdd(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VAddi: {
        EmitVAddi(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetImmediateData());
        break;
    }

    case IROpcode::VEqual: {
        EmitVEqual(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), inst.GetMask());
        break;
    }

    case IROpcode::VLessThanSigned: {
        EmitVLessThanSigned(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), inst.GetMask());
        break;
    }

    case IROpcode::VLessThanUnsigned: {
        EmitVLessThanUnsigned(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), inst.GetMask());
        break;
    }

    case IROpcode::VGreaterThanSigned: {
        EmitVGreaterThanSigned(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), inst.GetMask());
        break;
    }

    case IROpcode::VGreaterThanUnsigned: {
        EmitVGreaterThanUnsigned(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), inst.GetMask());
        break;
    }

    case IROpcode::VSll: {
        EmitVSll(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), inst.GetMask());
        break;
    }

    case IROpcode::VSlli: {
        EmitVSlli(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetImmediateData(), inst.GetMask());
        break;
    }

    case IROpcode::VSrl: {
        EmitVSrl(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), inst.GetMask());
        break;
    }

    case IROpcode::VSrli: {
        EmitVSrli(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetImmediateData(), inst.GetMask());
        break;
    }

    case IROpcode::VSrai: {
        EmitVSrai(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetImmediateData(), inst.GetMask());
        break;
    }

    case IROpcode::VMSeqi: {
        EmitVMSeqi(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetImmediateData(), inst.GetMask());
        break;
    }

    case IROpcode::VMSlt: {
        EmitVMSlt(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), inst.GetMask());
        break;
    }

    case IROpcode::VSlideDowni: {
        EmitVSlideDowni(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetImmediateData(), inst.GetMask());
        break;
    }

    case IROpcode::VSlideUpi: {
        EmitVSlideUpi(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetImmediateData(), inst.GetMask());
        break;
    }

    case IROpcode::VSlideUpZeroesi: {
        EmitVSlideUpZeroesi(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetImmediateData(), inst.GetMask());
        break;
    }

    case IROpcode::VSlideDownZeroesi: {
        EmitVSlideDownZeroesi(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetImmediateData(), inst.GetMask());
        break;
    }

    case IROpcode::VSlide1Up: {
        EmitVSlide1Up(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), inst.GetMask());
        break;
    }

    case IROpcode::VSlide1Down: {
        EmitVSlide1Down(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), inst.GetMask());
        break;
    }

    case IROpcode::VFAdd: {
        EmitVFAdd(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VFSub: {
        EmitVFSub(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VFMul: {
        EmitVFMul(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VFDiv: {
        EmitVFDiv(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VFSqrt: {
        EmitVFSqrt(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::VFRcp: {
        EmitVFRcp(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::VFRcpSqrt: {
        EmitVFRcpSqrt(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)));
        break;
    }

    case IROpcode::VFNotEqual: {
        EmitVFNotEqual(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), inst.GetMask());
        break;
    }

    case IROpcode::VFLessThan: {
        EmitVFLessThan(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)), inst.GetMask());
        break;
    }

    case IROpcode::VCvtSToF: {
        EmitVCvtSToF(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetMask());
        break;
    }

    case IROpcode::VWCvtSToF: {
        EmitVWCvtSToF(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetMask());
        break;
    }

    case IROpcode::VNCvtSToF: {
        EmitVNCvtSToF(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetMask());
        break;
    }

    case IROpcode::VCvtFToS: {
        EmitVCvtFToS(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetMask());
        break;
    }

    case IROpcode::VCvtFToSRtz: {
        EmitVCvtFToSRtz(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetMask());
        break;
    }

    case IROpcode::VNCvtFToS: {
        EmitVNCvtFToS(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetMask());
        break;
    }

    case IROpcode::VNCvtFToSRtz: {
        EmitVNCvtFToSRtz(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetMask());
        break;
    }

    case IROpcode::VWCvtFToS: {
        EmitVWCvtFToS(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetMask());
        break;
    }

    case IROpcode::VWCvtFToSRtz: {
        EmitVWCvtFToSRtz(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), inst.GetMask());
        break;
    }

    case IROpcode::VFMin: {
        EmitVFMin(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::VFMax: {
        EmitVFMax(backend, _RegW_(inst.GetName()), _RegR_(inst.GetOperand(0)), _RegR_(inst.GetOperand(1)));
        break;
    }

    case IROpcode::Fence: {
        EmitFence(backend, (FenceOrder)(inst.GetImmediateData() >> 4), (FenceOrder)(inst.GetImmediateData() & 0b1111));
        break;
    }
    }
}
