#include "felix86/backend/backend.hpp"
#include "felix86/backend/emitter.hpp"

struct AllocatedGPR {
    AllocatedGPR(Backend& backend, Assembler& as, const IRInstruction* inst, bool load) : as(as) {
        this->load = load;
        if (inst->IsSpilled()) {
            spilled = true;
            spill_location = inst->GetSpillLocation() * (inst->IsVec() ? 16 : 8);
            if (inst->IsGPR()) {
                biscuit::GPR gpr = backend.AcquireScratchGPR();
                if (load) {
                    if (spill_location > 2047) {
                        ERROR("Spill location too large");
                    }
                    as.LD(gpr, spill_location, Registers::SpillPointer());
                }
                reg = gpr;
            } else {
                ERROR("Implme");
            }
        } else {
            reg = inst->GetGPR();
        }
    }

    ~AllocatedGPR() {
        if (spilled && !load) {
            switch (reg.index()) {
            case 0: {
                biscuit::GPR gpr = std::get<biscuit::GPR>(reg);
                // Store to spilled location
                as.SD(gpr, spill_location, Registers::SpillPointer());
                break;
            }
            case 1: {
                biscuit::FPR fpr = std::get<biscuit::FPR>(reg);
                as.FSD(fpr, spill_location, Registers::SpillPointer());
                break;
            }
            case 2: {
                ERROR("Implme, needs vector spill location instead because they are 128-bit");
                break;
            }
            default: {
                UNREACHABLE();
            }
            }
        }
    }

    AllocatedGPR(const AllocatedGPR&) = delete;
    AllocatedGPR& operator=(const AllocatedGPR&) = delete;

    AllocatedGPR(AllocatedGPR&& other) = delete;
    AllocatedGPR& operator=(AllocatedGPR&& other) = delete;

    operator biscuit::GPR() const {
        return std::get<biscuit::GPR>(reg);
    }

    operator biscuit::FPR() const {
        return std::get<biscuit::FPR>(reg);
    }

    operator biscuit::Vec() const {
        return std::get<biscuit::Vec>(reg);
    }

    bool spilled = false;
    bool load = false;
    u64 spill_location = 0;
    std::variant<biscuit::GPR, biscuit::FPR, biscuit::Vec> reg;
    Assembler& as;
};

#define _RegRO_(instruction) AllocatedGPR(backend, as, instruction, true)
#define _RegWO_(instruction) AllocatedGPR(backend, as, instruction, false)

// Dispatch to correct function
void Emitter::Emit(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
#define X(stuff)                                                                                                                                     \
    case IROpcode::stuff: {                                                                                                                          \
        Emit##stuff(backend, as, inst);                                                                                                              \
        break;                                                                                                                                       \
    }

    switch (inst.GetOpcode()) {
        IR_OPCODES
    default: {
        UNREACHABLE();
    }
    }

#undef X

    backend.ReleaseScratchRegs();
}

template <typename T>
T my_abs(T val) {
    return val < 0 ? -val : val;
}

void Emitter::EmitJump(Backend& backend, biscuit::Assembler& as, void* target) {
    // Check if target is in one MB range
    void* cursor = as.GetCursorPointer();
    if (my_abs((u64)cursor - (u64)target) > 0x100000) {
        biscuit::GPR scratch = backend.AcquireScratchGPR();
        as.LI(scratch, (u64)target);
        as.JR(scratch);
        backend.ReleaseScratchRegs();
    } else {
        as.J((u64)target - (u64)cursor);
    }
}

void Emitter::EmitJumpConditional(Backend& backend, biscuit::Assembler& as, const IRInstruction& condition, void* target_true, void* target_false) {
    biscuit::GPR address_true = backend.AcquireScratchGPR();
    biscuit::GPR address_false = backend.AcquireScratchGPR();
    biscuit::GPR condition_reg = _RegRO_(&condition);
    Label false_label;

    // TODO: emit relative jumps if possible
    as.BEQZ(condition_reg, &false_label);
    as.LI(address_true, (u64)target_true);
    as.JR(address_true);
    as.Bind(&false_label);
    as.LI(address_false, (u64)target_false);
    as.JR(address_false);
}

void Emitter::EmitNull(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitPhi(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitComment(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {}

void Emitter::EmitMov(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitImmediate(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    const Immediate& imm = inst.AsImmediate();
    auto Rd = _RegWO_(&inst);
    as.LI(Rd, imm.immediate);
}

void Emitter::EmitPopcount(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitSext8(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitSext16(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitSext32(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitSyscall(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitCpuid(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitRdtsc(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitGetGuest(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitSetGuest(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitLoadGuestFromMemory(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    biscuit::GPR address = backend.GetRegisters().ThreadStatePointer();

    auto Rd = _RegWO_(&inst);

    const GetGuest& get_guest = inst.AsGetGuest();
    x86_ref_e ref = get_guest.ref;
    switch (ref) {
    case X86_REF_RAX ... X86_REF_R15: {
        u64 offset = offsetof(ThreadState, gprs) + (ref - X86_REF_RAX) * sizeof(u64);
        as.LD(Rd, offset, address);
        break;
    }
    case X86_REF_CF ... X86_REF_OF: {
        u64 offset = offsetof(ThreadState, cf) + (ref - X86_REF_CF) * sizeof(bool);
        as.LB(Rd, offset, address);
        break;
    }
    case X86_REF_RIP: {
        u64 offset = offsetof(ThreadState, rip);
        as.LD(Rd, offset, address);
        break;
    }
    case X86_REF_FS: {
        u64 offset = offsetof(ThreadState, fsbase);
        as.LD(Rd, offset, address);
        break;
    }
    case X86_REF_GS: {
        u64 offset = offsetof(ThreadState, gsbase);
        as.LD(Rd, offset, address);
        break;
    }
    default: {
        UNREACHABLE();
    }
    }
}

void Emitter::EmitStoreGuestToMemory(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    biscuit::GPR address = backend.GetRegisters().ThreadStatePointer();

    const SetGuest& set_guest = inst.AsSetGuest();
    x86_ref_e ref = set_guest.ref;
    auto Rs = _RegRO_(set_guest.source);

    switch (ref) {
    case X86_REF_RAX ... X86_REF_R15: {
        u64 offset = offsetof(ThreadState, gprs) + (ref - X86_REF_RAX) * sizeof(u64);
        as.SD(Rs, offset, address);
        break;
    }
    case X86_REF_CF ... X86_REF_OF: {
        u64 offset = offsetof(ThreadState, cf) + (ref - X86_REF_CF) * sizeof(bool);
        as.SB(Rs, offset, address);
        break;
    }
    case X86_REF_RIP: {
        u64 offset = offsetof(ThreadState, rip);
        as.SD(Rs, offset, address);
        break;
    }
    case X86_REF_FS: {
        u64 offset = offsetof(ThreadState, fsbase);
        as.SD(Rs, offset, address);
        break;
    }
    case X86_REF_GS: {
        u64 offset = offsetof(ThreadState, gsbase);
        as.SD(Rs, offset, address);
        break;
    }
    default: {
        UNREACHABLE();
    }
    }
}

void Emitter::EmitPushHost(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    const PushHost& push_host = inst.AsPushHost();
    biscuit::GPR vm_state = backend.GetRegisters().AcquireScratchGPR();
    as.LI(vm_state, backend.GetVMStatePointer());
    switch (push_host.ref) {
    case RISCV_REF_X0 ... RISCV_REF_X31: {
        u32 index = push_host.ref - RISCV_REF_X0;
        u32 offset = index * sizeof(u64);
        biscuit::GPR to_push = biscuit::GPR(index);
        as.SD(to_push, offset, vm_state);
        break;
    }
    case RISCV_REF_F0 ... RISCV_REF_F31: {
        u32 index = push_host.ref - RISCV_REF_F0;
        u32 offset = index * sizeof(double) + (32 * sizeof(u64));
        biscuit::FPR to_push = biscuit::FPR(index);
        as.FSD(to_push, offset, vm_state);
        break;
    }
    case RISCV_REF_VEC0 ... RISCV_REF_VEC31: {
        ERROR("Implme");
        break;
    }
    default: {
        UNREACHABLE();
    }
    }
}

void Emitter::EmitPopHost(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    const PopHost& pop_host = inst.AsPopHost();
    biscuit::GPR vm_state = backend.GetRegisters().AcquireScratchGPR();
    as.LI(vm_state, backend.GetVMStatePointer());
    switch (pop_host.ref) {
    case RISCV_REF_X0 ... RISCV_REF_X31: {
        u32 index = pop_host.ref - RISCV_REF_X0;
        u32 offset = index * sizeof(u64);
        biscuit::GPR to_pop = biscuit::GPR(index);
        biscuit::GPR base = vm_state;
        as.LD(to_pop, offset, base);
        break;
    }
    case RISCV_REF_F0 ... RISCV_REF_F31: {
        u32 index = pop_host.ref - RISCV_REF_F0;
        u32 offset = index * sizeof(double) + (32 * sizeof(u64));
        biscuit::FPR to_pop = biscuit::FPR(index);
        biscuit::GPR base = vm_state;
        as.FLD(to_pop, offset, base);
        break;
    }
    case RISCV_REF_VEC0 ... RISCV_REF_VEC31: {
        ERROR("Implme");
        break;
    }
    default: {
        UNREACHABLE();
    }
    }
}

void Emitter::EmitAdd(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    auto Rs1 = _RegRO_(inst.GetOperand(0));
    auto Rs2 = _RegRO_(inst.GetOperand(1));
    auto Rd = _RegWO_(&inst);
    as.ADD(Rd, Rs1, Rs2);
}

void Emitter::EmitSub(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    auto Rs1 = _RegRO_(inst.GetOperand(0));
    auto Rs2 = _RegRO_(inst.GetOperand(1));
    auto Rd = _RegWO_(&inst);
    as.SUB(Rd, Rs1, Rs2);
}

void Emitter::EmitDivu(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitDiv(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitRemu(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitRem(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitDivuw(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitDivw(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitRemuw(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitRemw(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitDiv128(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitDivu128(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitMul(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitMulh(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitMulhu(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitClz(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitCtz(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitShiftLeft(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitShiftRight(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitShiftRightArithmetic(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitLeftRotate8(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitLeftRotate16(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitLeftRotate32(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitLeftRotate64(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitSelect(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitAnd(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    auto Rs1 = _RegRO_(inst.GetOperand(0));
    auto Rs2 = _RegRO_(inst.GetOperand(1));
    auto Rd = _RegWO_(&inst);
    as.AND(Rd, Rs1, Rs2);
}

void Emitter::EmitOr(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitXor(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitNot(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitEqual(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitNotEqual(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitIGreaterThan(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitILessThan(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitUGreaterThan(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitULessThan(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitReadByte(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitReadWord(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitReadDWord(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitReadQWord(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    auto address = _RegRO_(inst.GetOperand(0));
    auto Rd = _RegWO_(&inst);
    as.LD(Rd, 0, address);
}

void Emitter::EmitReadXmmWord(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitWriteByte(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitWriteWord(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitWriteDWord(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitWriteQWord(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    auto address = _RegRO_(inst.GetOperand(0));
    auto Rs = _RegRO_(inst.GetOperand(1));
    as.SD(Rs, 0, address);
}

void Emitter::EmitWriteXmmWord(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitCastIntegerToVector(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitCastVectorToInteger(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVInsertInteger(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVExtractInteger(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVUnpackByteLow(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVUnpackWordLow(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVUnpackDWordLow(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVUnpackQWordLow(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVAnd(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVOr(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVXor(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVShr(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVShl(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVPackedSubByte(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVPackedAddQWord(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVPackedEqualByte(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVPackedEqualWord(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVPackedEqualDWord(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVPackedShuffleDWord(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVMoveByteMask(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVPackedMinByte(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitVZext64(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}

void Emitter::EmitCount(Backend& backend, biscuit::Assembler& as, const IRInstruction& inst) {
    UNREACHABLE();
}
