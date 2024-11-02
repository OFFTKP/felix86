#include "felix86/backend/backend.hpp"
#include "felix86/backend/emitter.hpp"
#include "felix86/hle/cpuid.hpp"
#include "felix86/hle/syscall.hpp"

#define AS (backend.GetAssembler())

namespace {

biscuit::GPR Push(Backend& backend, biscuit::GPR Rs) {
    AS.ADDI(Registers::StackPointer(), Registers::StackPointer(), -8);
    AS.SD(Rs, 0, Registers::StackPointer());
    return Rs;
}

void Pop(Backend& backend, biscuit::GPR Rs) {
    AS.LD(Rs, 0, Registers::StackPointer());
    AS.ADDI(Registers::StackPointer(), Registers::StackPointer(), 8);
}

void EmitCrash(Backend& backend, ExitReason reason) {
    Emitter::EmitSetExitReason(backend, static_cast<u64>(reason));
    Emitter::EmitJumpFar(backend, backend.GetCrashTarget());
}

void SoftwareCtz(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs, u32 size) {
    const auto& gprs = Registers::GetAllocatableGPRs();
    biscuit::GPR mask = Push(backend, gprs[0]);
    biscuit::GPR counter = Push(backend, gprs[1]);
    AS.LI(mask, 1);

    Label loop, end;
    AS.Bind(&loop);
    AS.AND(Rd, Rs, mask);
    AS.BNEZ(Rd, &end);
    AS.ADDI(counter, counter, 1);
    AS.SLLI(mask, mask, 1);
    AS.LI(Rd, size);
    AS.SLTU(Rd, counter, Rd);
    AS.BNEZ(Rd, &loop);

    AS.Bind(&end);
    AS.MV(Rd, counter);

    Pop(backend, counter);
    Pop(backend, mask);
}

using Operation = std::function<void(biscuit::Assembler&, biscuit::GPR, biscuit::GPR, biscuit::GPR)>;

void SoftwareAtomicFetchRMW8(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs, biscuit::GPR Address, biscuit::Ordering ordering,
                             Operation operation) {
    if (ordering != biscuit::Ordering::AQRL) {
        UNIMPLEMENTED();
    }

    // Since there's no 8-bit atomics yet, we need to emulate the RMW operations using 32-bit SC/LL.
    // We need a mask for the byte we want to modify and a mask for the bytes we want to stay the same
    // Load the word, do the operation, mask the resulting byte and the initial bytes, or them together and SC.
    // Also save the initial read value in Rd

    const auto& gprs = Registers::GetAllocatableGPRs();

    biscuit::GPR scratch = Push(backend, gprs[0]);
    biscuit::GPR scratch2 = Push(backend, gprs[1]);
    biscuit::GPR mask = Push(backend, gprs[2]);
    biscuit::GPR mask_not = Push(backend, gprs[3]);
    biscuit::GPR Rs_shifted = Push(backend, gprs[4]);
    biscuit::GPR address_aligned = Push(backend, gprs[5]);

    AS.ANDI(mask, Address, 3);
    AS.SLLIW(mask, mask, 3);
    AS.SLLW(Rs_shifted, Rs, mask);
    AS.LI(scratch, 0xFF);
    AS.SLLW(mask, scratch, mask); // mask now contains a mask for the relevant byte
    AS.NOT(mask_not, mask);
    AS.ANDI(address_aligned, Address, -4);

    biscuit::Label loop;

    AS.Bind(&loop);
    AS.LR_W(biscuit::Ordering::AQRL, Rd, address_aligned);
    operation(AS, scratch2, Rd, Rs_shifted);
    AS.AND(scratch2, scratch2, mask);
    AS.AND(scratch, Rd, mask_not);
    AS.OR(scratch, scratch, scratch2);
    AS.SC_W(biscuit::Ordering::RL, scratch2, scratch, address_aligned);
    AS.BNEZ(scratch2, &loop);

    // Shift the loaded value to the correct place
    AS.ANDI(scratch, Address, 3);
    AS.SLLIW(scratch, scratch, 3);
    AS.SRAW(Rd, Rd, scratch);
    AS.ANDI(Rd, Rd, 0xFF);

    Pop(backend, address_aligned);
    Pop(backend, Rs_shifted);
    Pop(backend, mask_not);
    Pop(backend, mask);
    Pop(backend, scratch2);
    Pop(backend, scratch);
}

void SoftwareAtomicFetchRMW16(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs, biscuit::GPR Address, biscuit::Ordering ordering,
                              Operation operation) {
    if (ordering != biscuit::Ordering::AQRL) {
        UNIMPLEMENTED();
    }

    // See SoftwareAtomicFetchRMW8

    const auto& gprs = Registers::GetAllocatableGPRs();

    // TODO: obviously this is horrible but also one day we'll have 8/16 bit atomics <- clueless
    biscuit::GPR scratch = Push(backend, gprs[0]);
    biscuit::GPR scratch2 = Push(backend, gprs[1]);
    biscuit::GPR mask = Push(backend, gprs[2]);
    biscuit::GPR mask_not = Push(backend, gprs[3]);
    biscuit::GPR Rs_shifted = Push(backend, gprs[4]);
    biscuit::GPR address_aligned = Push(backend, gprs[5]);

    AS.ANDI(mask, Address, 3);
    AS.LI(scratch, 0xFFFF);
    AS.SLLIW(mask, mask, 3);
    AS.SLLW(Rs_shifted, Rs, mask);
    AS.SLLW(mask, scratch, mask);
    AS.ANDI(address_aligned, Address, -4);
    AS.NOT(mask_not, mask);

    biscuit::Label loop;

    AS.Bind(&loop);
    AS.LR_W(biscuit::Ordering::AQRL, Rd, address_aligned);
    operation(AS, scratch2, Rd, Rs_shifted);
    AS.AND(scratch2, scratch2, mask);
    AS.AND(scratch, Rd, mask_not);
    AS.OR(scratch, scratch, scratch2);
    AS.SC_W(biscuit::Ordering::RL, scratch2, scratch, address_aligned);
    AS.BNEZ(scratch2, &loop);

    // Shift the nibble accordingly
    AS.ANDI(scratch, Address, 3);
    AS.SLLIW(scratch, scratch, 3);
    AS.SRAW(Rd, Rd, scratch);
    AS.SLLI(Rd, Rd, 48);
    AS.SRLI(Rd, Rd, 48);

    Pop(backend, address_aligned);
    Pop(backend, Rs_shifted);
    Pop(backend, mask_not);
    Pop(backend, mask);
    Pop(backend, scratch2);
    Pop(backend, scratch);
}

// Sanity check for alignment until we have unaligned atomic extensions
void EmitAlignmentCheck(Backend& backend, biscuit::GPR address, u8 alignment) {
    if (!Extensions::Zam) {
        biscuit::Label ok;
        AS.ANDI(address, address, alignment - 1);
        AS.BEQZ(address, &ok);
        EmitCrash(backend, ExitReason::EXIT_REASON_BAD_ALIGNMENT);
        AS.Bind(&ok);
    }
}

} // namespace

void Emitter::EmitPushAllCallerSaved(Backend& backend) {
    auto& caller_saved_gprs = Registers::GetCallerSavedGPRs();

    constexpr i64 size = 8 * caller_saved_gprs.size();
    AS.ADDI(Registers::StackPointer(), Registers::StackPointer(), -size);

    for (size_t i = 0; i < caller_saved_gprs.size(); i++) {
        AS.SD(caller_saved_gprs[i], i * 8, Registers::StackPointer());
    }

    AS.VSETIVLI(x0, SUPPORTED_VLEN / 8, biscuit::SEW::E8);

    for (int i = 0; i < 32; i++) {
        AS.ADDI(Registers::StackPointer(), Registers::StackPointer(), -16);
        AS.VSE8(Vec(i), Registers::StackPointer());
    }
}

void Emitter::EmitPopAllCallerSaved(Backend& backend) {
    auto& caller_saved_gprs = Registers::GetCallerSavedGPRs();

    AS.VSETIVLI(x0, SUPPORTED_VLEN / 8, biscuit::SEW::E8);
    
    for (int i = 31; i >= 0; i--) {
        AS.VLE8(Vec(i), Registers::StackPointer());
        AS.ADDI(Registers::StackPointer(), Registers::StackPointer(), 16);
    }

    for (size_t i = 0; i < caller_saved_gprs.size(); i++) {
        AS.LD(caller_saved_gprs[i], i * 8, Registers::StackPointer());
    }

    constexpr i64 size = 8 * caller_saved_gprs.size();
    AS.ADDI(Registers::StackPointer(), Registers::StackPointer(), size);
}

void Emitter::EmitJumpFar(Backend& backend, void* target) {
    auto my_abs = [](u64 x) -> u64 { return x < 0 ? -x : x; };

    // Check if target is in one MB range
    void* cursor = AS.GetCursorPointer();
    if (my_abs((u64)cursor - (u64)target) > 0x100000) {
        AS.LI(t0, (u64)target);
        AS.JR(t0);
    } else {
        AS.J((u64)target - (u64)cursor);
    }
}

void Emitter::EmitJump(Backend& backend, Label* target) {
    AS.J(target);
}

void Emitter::EmitJumpConditional(Backend& backend, biscuit::GPR condition, Label* target_true, Label* target_false) {
    if (IsValidBTypeImm(target_false->GetLocation().value() - AS.GetCodeBuffer().GetCursorOffset())) {
        AS.BEQZ(condition, target_false);
        AS.J(target_true);
    } else if (IsValidBTypeImm(target_true->GetLocation().value() - AS.GetCodeBuffer().GetCursorOffset())) {
        AS.BNEZ(condition, target_true);
        AS.J(target_false);
    } else {
        Label false_label;
        AS.BEQZ(condition, &false_label);
        AS.J(target_true);
        AS.Bind(&false_label);
        AS.J(target_false);
    }
}

void Emitter::EmitCallHostFunction(Backend& backend, u64 function) {
    // Really naive implementation for now
    EmitPushAllCallerSaved(backend);

    AS.LI(t0, function);
    AS.MV(a0, Registers::ThreadStatePointer());
    AS.JALR(t0);

    EmitPopAllCallerSaved(backend);
}

void Emitter::EmitSetExitReason(Backend& backend, u64 reason) {
    AS.LI(t0, (u8)reason);
    AS.SB(t0, offsetof(ThreadState, exit_reason), Registers::ThreadStatePointer());
}

void Emitter::EmitMov(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    if (Rd == Rs) {
        return;
    }

    ASSERT(Rd != Registers::Zero());

    AS.MV(Rd, Rs);
}

void Emitter::EmitMov(Backend& backend, biscuit::Vec Rd, biscuit::Vec Rs) {
    if (Rd == Rs) {
        return;
    }

    AS.VMV(Rd, Rs);
}

void Emitter::EmitStoreSpill(Backend& backend, biscuit::GPR Rd, u32 spill_offset) {
    if (spill_offset < 2048) {
        AS.SD(Rd, spill_offset, Registers::StackPointer());
    } else {
        AS.LI(t0, spill_offset);
        AS.ADD(t0, t0, Registers::StackPointer());
        AS.SD(Rd, 0, t0);
    }
}

void Emitter::EmitStoreSpill(Backend& backend, biscuit::Vec Rd, u32 spill_offset) {
    // TODO: need to get what the vectorstate is when emitting spills
    UNREACHABLE();
}

void Emitter::EmitLoadSpill(Backend& backend, biscuit::GPR Rd, u32 spill_offset) {
    if (spill_offset < 2048) {
        AS.LD(Rd, spill_offset, Registers::StackPointer());
    } else {
        AS.LI(t0, spill_offset);
        AS.ADD(t0, t0, Registers::StackPointer());
        AS.LD(Rd, 0, t0);
    }
}

void Emitter::EmitLoadSpill(Backend& backend, biscuit::Vec Rd, u32 spill_offset) {
    // TODO: need to get what the vectorstate is when emitting spills
    UNREACHABLE();
}

void Emitter::EmitImmediate(Backend& backend, biscuit::GPR Rd, u64 immediate) {
    ASSERT(immediate != 0);
    AS.LI(Rd, immediate);
}

void Emitter::EmitRdtsc(Backend& backend) {
    UNREACHABLE();
}

void Emitter::EmitSyscall(Backend& backend) {
    EmitPushAllCallerSaved(backend);

    AS.LI(a0, (u64)&backend.GetEmulator()); // TODO: maybe make Emulator class global...
    AS.MV(a1, Registers::ThreadStatePointer());
    AS.LI(a2, (u64)felix86_syscall); // TODO: remove when moving code buffer close to text?
    AS.JALR(a2);

    EmitPopAllCallerSaved(backend);
}

void Emitter::EmitCpuid(Backend& backend) {
    EmitPushAllCallerSaved(backend);

    AS.MV(a0, Registers::ThreadStatePointer());
    AS.LI(a1, (u64)felix86_cpuid);
    AS.JALR(a1);

    EmitPopAllCallerSaved(backend);
}

void Emitter::EmitSext8(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    if (Extensions::B) {
        AS.SEXTB(Rd, Rs);
    } else {
        AS.SLLI(Rd, Rs, 56);
        AS.SRAI(Rd, Rd, 56);
    }
}

void Emitter::EmitSext16(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    if (Extensions::B) {
        AS.SEXTH(Rd, Rs);
    } else {
        AS.SLLI(Rd, Rs, 48);
        AS.SRAI(Rd, Rd, 48);
    }
}

void Emitter::EmitSext32(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    AS.ADDIW(Rd, Rs, 0);
}

void Emitter::EmitZext8(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    AS.ANDI(Rd, Rs, 0xFF);
}

void Emitter::EmitZext16(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    AS.SLLI(Rd, Rs, 48);
    AS.SRLI(Rd, Rd, 48);
}

void Emitter::EmitZext32(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    AS.SLLI(Rd, Rs, 32);
    AS.SRLI(Rd, Rd, 32);
}

void Emitter::EmitClz(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    AS.CLZ(Rd, Rs);
}

void Emitter::EmitCtzh(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    SoftwareCtz(backend, Rd, Rs, 16);
}

void Emitter::EmitCtzw(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    if (Extensions::B) {
        AS.CTZW(Rd, Rs);
    } else {
        SoftwareCtz(backend, Rd, Rs, 32);
    }
}

void Emitter::EmitCtz(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    if (Extensions::B) {
        AS.CTZ(Rd, Rs);
    } else {
        SoftwareCtz(backend, Rd, Rs, 64);
    }
}

void Emitter::EmitNot(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    AS.NOT(Rd, Rs);
}

void Emitter::EmitNeg(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    AS.NEG(Rd, Rs);
}

void Emitter::EmitParity(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    if (Extensions::B) {
        AS.ANDI(Rd, Rs, 0xFF);
        AS.CPOPW(Rd, Rd);
        AS.ANDI(Rd, Rd, 1);
        AS.XORI(Rd, Rd, 1);
    } else {
        // clang-format off
        static bool bitcount[] = {
            1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
            0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
            0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
            1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
            0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
            1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
            1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
            0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
            0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
            1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
            1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
            0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
            1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
            0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
            0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
            1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
        };
        // clang-format on

        AS.LI(t0, (u64)&bitcount);
        AS.ANDI(Rd, Rs, 0xFF);
        AS.ADD(Rd, Rd, t0);
        AS.LB(Rd, 0, Rd);
    }
}

void Emitter::EmitDiv128(Backend& backend, biscuit::GPR Rs) {
    Push(backend, a1);
    AS.MV(a1, Rs);
    EmitCallHostFunction(backend, (u64)felix86_div128);
    Pop(backend, a1);
}

void Emitter::EmitDivu128(Backend& backend, biscuit::GPR Rs) {
    Push(backend, a1);
    AS.MV(a1, Rs);
    EmitCallHostFunction(backend, (u64)felix86_divu128);
    Pop(backend, a1);
}

void Emitter::EmitReadByte(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    AS.LBU(Rd, 0, Rs);
}

void Emitter::EmitReadWord(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    AS.LHU(Rd, 0, Rs);
}

void Emitter::EmitReadDWord(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    AS.LWU(Rd, 0, Rs);
}

void Emitter::EmitReadQWord(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    AS.LD(Rd, 0, Rs);
}

void Emitter::EmitReadXmmWord(Backend& backend, biscuit::Vec Vd, biscuit::GPR Address, VectorState state) {
    switch (state) {
    case VectorState::PackedByte:
        AS.VLE8(Vd, Address);
        break;
    case VectorState::PackedWord:
        AS.VLE16(Vd, Address);
        break;
    case VectorState::Float:
    case VectorState::PackedDWord:
        AS.VLE32(Vd, Address);
        break;
    case VectorState::Double:
    case VectorState::PackedQWord:
        AS.VLE64(Vd, Address);
        break;
    case VectorState::AnyPacked:
    case VectorState::Null:
        UNREACHABLE();
    }
}

void Emitter::EmitReadByteRelative(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs, u64 offset) {
    ASSERT(IsValidSigned12BitImm(offset));
    AS.LBU(Rd, offset, Rs);
}

void Emitter::EmitReadWordRelative(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs, u64 offset) {
    ASSERT(IsValidSigned12BitImm(offset));
    AS.LHU(Rd, offset, Rs);
}

void Emitter::EmitReadDWordRelative(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs, u64 offset) {
    ASSERT(IsValidSigned12BitImm(offset));
    AS.LWU(Rd, offset, Rs);
}

void Emitter::EmitReadQWordRelative(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs, u64 offset) {
    ASSERT(IsValidSigned12BitImm(offset));
    AS.LD(Rd, offset, Rs);
}

void Emitter::EmitReadXmmWordRelative(Backend& backend, biscuit::Vec Vd, biscuit::GPR Address, u64 offset, VectorState state) {
    ASSERT(IsValidSigned12BitImm(offset));
    if (offset == 0) {
        EmitReadXmmWord(backend, Vd, Address, state);
    } else {
        AS.ADDI(t0, Address, (i64)offset);
        EmitReadXmmWord(backend, Vd, t0, state);
    }
}

void Emitter::EmitWriteByte(Backend& backend, biscuit::GPR Address, biscuit::GPR Rs) {
    AS.SB(Rs, 0, Address);
}

void Emitter::EmitWriteWord(Backend& backend, biscuit::GPR Address, biscuit::GPR Rs) {
    AS.SH(Rs, 0, Address);
}

void Emitter::EmitWriteDWord(Backend& backend, biscuit::GPR Address, biscuit::GPR Rs) {
    AS.SW(Rs, 0, Address);
}

void Emitter::EmitWriteQWord(Backend& backend, biscuit::GPR Address, biscuit::GPR Rs) {
    AS.SD(Rs, 0, Address);
}

void Emitter::EmitWriteXmmWord(Backend& backend, biscuit::GPR Address, biscuit::Vec Vs, VectorState state) {
    switch (state) {
    case VectorState::PackedByte:
        AS.VSE8(Vs, Address);
        break;
    case VectorState::PackedWord:
        AS.VSE16(Vs, Address);
        break;
    case VectorState::Float:
    case VectorState::PackedDWord:
        AS.VSE32(Vs, Address);
        break;
    case VectorState::Double:
    case VectorState::PackedQWord:
        AS.VSE64(Vs, Address);
        break;
    case VectorState::AnyPacked:
    case VectorState::Null:
        UNREACHABLE();
    }
}

void Emitter::EmitWriteByteRelative(Backend& backend, biscuit::GPR Address, biscuit::GPR Rs, u64 offset) {
    ASSERT(IsValidSigned12BitImm(offset));
    AS.SB(Rs, offset, Address);
}

void Emitter::EmitWriteWordRelative(Backend& backend, biscuit::GPR Address, biscuit::GPR Rs, u64 offset) {
    ASSERT(IsValidSigned12BitImm(offset));
    AS.SH(Rs, offset, Address);
}

void Emitter::EmitWriteDWordRelative(Backend& backend, biscuit::GPR Address, biscuit::GPR Rs, u64 offset) {
    ASSERT(IsValidSigned12BitImm(offset));
    AS.SW(Rs, offset, Address);
}

void Emitter::EmitWriteQWordRelative(Backend& backend, biscuit::GPR Address, biscuit::GPR Rs, u64 offset) {
    ASSERT(IsValidSigned12BitImm(offset));
    AS.SD(Rs, offset, Address);
}

void Emitter::EmitWriteXmmWordRelative(Backend& backend, biscuit::GPR Address, biscuit::Vec Vs, u64 offset, VectorState state) {
    ASSERT(IsValidSigned12BitImm(offset));
    if (offset == 0) {
        EmitWriteXmmWord(backend, Address, Vs, state);
    } else {
        AS.ADDI(t0, Address, (i64)offset);
        EmitWriteXmmWord(backend, t0, Vs, state);
    }
}

void Emitter::EmitAdd(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.ADD(Rd, Rs1, Rs2);
}

void Emitter::EmitAddi(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs, u64 immediate) {
    ASSERT(Rs != x0);
    if (IsValidSigned12BitImm((i64)immediate)) {
        AS.ADDI(Rd, Rs, (i64)immediate);
    } else {
        UNREACHABLE();
    }
}

void Emitter::EmitAmoAdd8(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    if (Extensions::Zabha) {
        AS.AMOADD_B(ordering, Rd, Rs, Address);
        EmitZext8(backend, Rd, Rd);
    } else {
        SoftwareAtomicFetchRMW8(backend, Rd, Rs, Address, ordering, &biscuit::Assembler::ADD);
    }
}

void Emitter::EmitAmoAdd16(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    if (Extensions::Zabha) {
        EmitAlignmentCheck(backend, Address, 2);
        AS.AMOADD_H(ordering, Rd, Rs, Address);
        EmitZext16(backend, Rd, Rd);
    } else {
        SoftwareAtomicFetchRMW16(backend, Rd, Rs, Address, ordering, &biscuit::Assembler::ADD);
    }
}

void Emitter::EmitAmoAdd32(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    EmitAlignmentCheck(backend, Address, 4);
    AS.AMOADD_W(ordering, Rd, Rs, Address);
}

void Emitter::EmitAmoAdd64(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    EmitAlignmentCheck(backend, Address, 8);
    AS.AMOADD_D(ordering, Rd, Rs, Address);
}

void Emitter::EmitAmoAnd8(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    if (Extensions::Zabha) {
        AS.AMOAND_B(ordering, Rd, Rs, Address);
        EmitZext8(backend, Rd, Rd);
    } else {
        SoftwareAtomicFetchRMW8(backend, Rd, Rs, Address, ordering, &biscuit::Assembler::AND);
    }
}

void Emitter::EmitAmoAnd16(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    if (Extensions::Zabha) {
        EmitAlignmentCheck(backend, Address, 2);
        AS.AMOAND_H(ordering, Rd, Rs, Address);
        EmitZext16(backend, Rd, Rd);
    } else {
        SoftwareAtomicFetchRMW16(backend, Rd, Rs, Address, ordering, &biscuit::Assembler::AND);
    }
}

void Emitter::EmitAmoAnd32(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    EmitAlignmentCheck(backend, Address, 4);
    AS.AMOAND_W(ordering, Rd, Rs, Address);
    EmitZext32(backend, Rd, Rd);
}

void Emitter::EmitAmoAnd64(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    EmitAlignmentCheck(backend, Address, 8);
    AS.AMOAND_D(ordering, Rd, Rs, Address);
}

void Emitter::EmitAmoOr8(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    if (Extensions::Zabha) {
        AS.AMOOR_B(ordering, Rd, Rs, Address);
        EmitZext8(backend, Rd, Rd);
    } else {
        SoftwareAtomicFetchRMW8(backend, Rd, Rs, Address, ordering, &biscuit::Assembler::OR);
    }
}

void Emitter::EmitAmoOr16(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    if (Extensions::Zabha) {
        EmitAlignmentCheck(backend, Address, 2);
        AS.AMOOR_H(ordering, Rd, Rs, Address);
        EmitZext16(backend, Rd, Rd);
    } else {
        SoftwareAtomicFetchRMW16(backend, Rd, Rs, Address, ordering, &biscuit::Assembler::OR);
    }
}

void Emitter::EmitAmoOr32(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    EmitAlignmentCheck(backend, Address, 4);
    AS.AMOOR_W(ordering, Rd, Rs, Address);
    EmitZext32(backend, Rd, Rd);
}

void Emitter::EmitAmoOr64(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    EmitAlignmentCheck(backend, Address, 8);
    AS.AMOOR_D(ordering, Rd, Rs, Address);
}

void Emitter::EmitAmoXor8(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    if (Extensions::Zabha) {
        AS.AMOXOR_B(ordering, Rd, Rs, Address);
        EmitZext8(backend, Rd, Rd);
    } else {
        SoftwareAtomicFetchRMW8(backend, Rd, Rs, Address, ordering, &biscuit::Assembler::XOR);
    }
}

void Emitter::EmitAmoXor16(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    if (Extensions::Zabha) {
        EmitAlignmentCheck(backend, Address, 2);
        AS.AMOXOR_H(ordering, Rd, Rs, Address);
        EmitZext16(backend, Rd, Rd);
    } else {
        SoftwareAtomicFetchRMW16(backend, Rd, Rs, Address, ordering, &biscuit::Assembler::XOR);
    }
}

void Emitter::EmitAmoXor32(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    EmitAlignmentCheck(backend, Address, 4);
    AS.AMOXOR_W(ordering, Rd, Rs, Address);
    EmitZext32(backend, Rd, Rd);
}

void Emitter::EmitAmoXor64(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    EmitAlignmentCheck(backend, Address, 8);
    AS.AMOXOR_D(ordering, Rd, Rs, Address);
}

void Emitter::EmitAmoSwap8(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    if (Extensions::Zabha) {
        AS.AMOSWAP_B(ordering, Rd, Rs, Address);
        EmitZext8(backend, Rd, Rd);
    } else {
        auto mv = [](biscuit::Assembler& as, biscuit::GPR Rd, biscuit::GPR Rs, biscuit::GPR) { as.MV(Rd, Rs); };
        SoftwareAtomicFetchRMW8(backend, Rd, Rs, Address, ordering, mv);
    }
}

void Emitter::EmitAmoSwap16(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    if (Extensions::Zabha) {
        EmitAlignmentCheck(backend, Address, 2);
        AS.AMOSWAP_H(ordering, Rd, Rs, Address);
        EmitZext16(backend, Rd, Rd);
    } else {
        auto mv = [](biscuit::Assembler& as, biscuit::GPR Rd, biscuit::GPR Rs, biscuit::GPR) { as.MV(Rd, Rs); };
        SoftwareAtomicFetchRMW16(backend, Rd, Rs, Address, ordering, mv);
    }
}

void Emitter::EmitAmoSwap32(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    EmitAlignmentCheck(backend, Address, 4);
    AS.AMOSWAP_W(ordering, Rd, Rs, Address);
    EmitZext32(backend, Rd, Rd);
}

void Emitter::EmitAmoSwap64(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    EmitAlignmentCheck(backend, Address, 8);
    AS.AMOSWAP_D(ordering, Rd, Rs, Address);
}

void Emitter::EmitAmoCAS8(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Expected, biscuit::GPR Rs,
                          biscuit::Ordering ordering) {
    if (Extensions::Zabha && Extensions::Zacas) {
        AS.MV(Rd, Expected);
        AS.AMOCAS_B(ordering, Rd, Rs, Address);
        EmitZext8(backend, Rd, Rd);
    } else {
        WARN("Non-atomic CAS8 fallback");
        Label not_same;
        AS.LB(t0, 0, Address);
        AS.BNE(t0, Expected, &not_same);
        AS.SB(Rs, 0, Address);
        AS.Bind(&not_same);
        AS.MV(Rd, t0);
    }
}

void Emitter::EmitAmoCAS16(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Expected, biscuit::GPR Rs,
                           biscuit::Ordering ordering) {
    if (Extensions::Zabha && Extensions::Zacas) {
        EmitAlignmentCheck(backend, Address, 2);
        AS.MV(Rd, Expected);
        AS.AMOCAS_H(ordering, Rd, Rs, Address);
        EmitZext16(backend, Rd, Rd);
    } else {
        WARN("Non-atomic CAS16 fallback");
        Label not_same;
        AS.LH(t0, 0, Address);
        AS.BNE(t0, Expected, &not_same);
        AS.SH(Rs, 0, Address);
        AS.Bind(&not_same);
        AS.MV(Rd, t0);
    }
}

void Emitter::EmitAmoCAS32(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Expected, biscuit::GPR Rs,
                           biscuit::Ordering ordering) {
    if (Extensions::Zacas) {
        EmitAlignmentCheck(backend, Address, 4);
        AS.MV(Rd, Expected);
        AS.AMOCAS_W(ordering, Rd, Rs, Address);
        EmitZext32(backend, Rd, Rd);
    } else {
        WARN("Non-atomic CAS32 fallback");
        Label not_same;
        AS.LW(t0, 0, Address);
        AS.BNE(t0, Expected, &not_same);
        AS.SW(Rs, 0, Address);
        AS.Bind(&not_same);
        AS.MV(Rd, t0);
    }
}

void Emitter::EmitAmoCAS64(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Expected, biscuit::GPR Rs,
                           biscuit::Ordering ordering) {
    if (Extensions::Zacas) {
        EmitAlignmentCheck(backend, Address, 8);
        AS.MV(Rd, Expected);
        AS.AMOCAS_D(ordering, Rd, Rs, Address);
    } else {
        WARN("Non-atomic CAS64 fallback");
        Label not_same;
        AS.LD(t0, 0, Address);
        AS.BNE(t0, Expected, &not_same);
        AS.SD(Rs, 0, Address);
        AS.Bind(&not_same);
        AS.MV(Rd, t0);
    }
}

void Emitter::EmitSub(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.SUB(Rd, Rs1, Rs2);
}

void Emitter::EmitAnd(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.AND(Rd, Rs1, Rs2);
}

void Emitter::EmitAndi(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs, u64 immediate) {
    if (IsValidSigned12BitImm((i64)immediate)) {
        AS.ANDI(Rd, Rs, (i64)immediate);
    } else {
        UNREACHABLE();
    }
}

void Emitter::EmitOr(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.OR(Rd, Rs1, Rs2);
}

void Emitter::EmitOri(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs, u64 immediate) {
    if (IsValidSigned12BitImm((i64)immediate)) {
        AS.ORI(Rd, Rs, (i64)immediate);
    } else {
        UNREACHABLE();
    }
}

void Emitter::EmitXor(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.XOR(Rd, Rs1, Rs2);
}

void Emitter::EmitXori(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs, u64 immediate) {
    if (IsValidSigned12BitImm((i64)immediate)) {
        AS.XORI(Rd, Rs, (i64)immediate);
    } else {
        UNREACHABLE();
    }
}

void Emitter::EmitEqual(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.XOR(Rd, Rs1, Rs2);
    AS.SEQZ(Rd, Rd);
}

void Emitter::EmitNotEqual(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.XOR(Rd, Rs1, Rs2);
    AS.SNEZ(Rd, Rd);
}

void Emitter::EmitSeqz(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    AS.SEQZ(Rd, Rs);
}

void Emitter::EmitSnez(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    AS.SNEZ(Rd, Rs);
}

void Emitter::EmitSetLessThanSigned(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.SLT(Rd, Rs1, Rs2);
}

void Emitter::EmitSetLessThanUnsigned(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.SLTU(Rd, Rs1, Rs2);
}

void Emitter::EmitShl(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.SLL(Rd, Rs1, Rs2); // TODO: add more robust shift IR instructions to abuse C_SLLI & co
}

void Emitter::EmitShli(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs, i64 immediate) {
    if (IsValidSigned12BitImm(immediate)) {
        AS.SLLI(Rd, Rs, immediate);
    } else {
        UNREACHABLE();
    }
}

void Emitter::EmitShr(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.SRL(Rd, Rs1, Rs2);
}

void Emitter::EmitShri(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs, i64 immediate) {
    if (IsValidSigned12BitImm(immediate)) {
        AS.SRLI(Rd, Rs, immediate);
    } else {
        UNREACHABLE();
    }
}

void Emitter::EmitSar(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.SRA(Rd, Rs1, Rs2);
}

void Emitter::EmitSari(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs, i64 immediate) {
    if (IsValidSigned12BitImm(immediate)) {
        AS.SRAI(Rd, Rs, immediate);
    } else {
        UNREACHABLE();
    }
}

void Emitter::EmitRol8(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.ANDI(t0, Rs2, 0x7);
    AS.SLLW(Rd, Rs1, t0);
    AS.NEG(t0, t0);
    AS.ANDI(t0, t0, 0x7);
    AS.SRLW(t0, Rs1, t0);
    AS.OR(Rd, Rd, t0);
    AS.ANDI(Rd, Rd, 0xFF);
}

void Emitter::EmitRol16(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.ANDI(t0, Rs2, 0x1F);
    AS.SLLW(Rd, Rs1, t0);
    AS.NEG(t0, t0);
    AS.ANDI(t0, t0, 0x1F);
    AS.SRLW(t0, Rs1, t0);
    AS.OR(Rd, Rd, t0);
    AS.ZEXTH(Rd, Rd);
}

void Emitter::EmitRol32(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.ROLW(Rd, Rs1, Rs2);
}

void Emitter::EmitRol64(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.ROL(Rd, Rs1, Rs2);
}

void Emitter::EmitRor8(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    UNIMPLEMENTED();
}

void Emitter::EmitRor16(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    UNIMPLEMENTED();
}

void Emitter::EmitRor32(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.RORW(Rd, Rs1, Rs2);
}

void Emitter::EmitRor64(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.ROR(Rd, Rs1, Rs2);
}

void Emitter::EmitDiv(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.DIV(Rd, Rs1, Rs2);
}

void Emitter::EmitDivu(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.DIVU(Rd, Rs1, Rs2);
}

void Emitter::EmitDivw(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.DIVW(Rd, Rs1, Rs2);
}

void Emitter::EmitDivuw(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.DIVUW(Rd, Rs1, Rs2);
}

void Emitter::EmitRem(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.REM(Rd, Rs1, Rs2);
}

void Emitter::EmitRemu(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.REMU(Rd, Rs1, Rs2);
}

void Emitter::EmitRemw(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.REMW(Rd, Rs1, Rs2);
}

void Emitter::EmitRemuw(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.REMUW(Rd, Rs1, Rs2);
}

void Emitter::EmitMul(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.MUL(Rd, Rs1, Rs2);
}

void Emitter::EmitMulh(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.MULH(Rd, Rs1, Rs2);
}

void Emitter::EmitMulhu(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.MULHU(Rd, Rs1, Rs2);
}

void Emitter::EmitSelect(Backend& backend, biscuit::GPR Rd, biscuit::GPR Condition, biscuit::GPR RsTrue, biscuit::GPR RsFalse) {
    if (RsTrue == RsFalse) {
        WARN("Selecting the same register");
        AS.MV(Rd, RsTrue);
        return;
    }

    if (Extensions::Xtheadcondmov) {
        if (Rd != RsTrue) {
            if (Rd != RsFalse)
                AS.MV(Rd, RsFalse);
            AS.TH_MVNEZ(Rd, RsTrue, Condition);
        } else {
            AS.TH_MVEQZ(Rd, RsFalse, Condition);
        }
    } else if (Extensions::Zicond) {
        // Not my favorite of conditional move patterns.
        // This was done like that because no other RISC-V instructions
        // need a third read port.
        AS.CZERO_NEZ(t0, RsFalse, Condition);
        AS.CZERO_EQZ(Rd, RsTrue, Condition);
        AS.OR(Rd, Rd, t0);
    } else {
        if (Rd != RsFalse) {
            Label true_label;
            AS.MV(Rd, RsTrue);
            AS.BNEZ(Condition, &true_label);
            AS.MV(Rd, RsFalse);
            AS.Bind(&true_label);
        } else {
            // If Rd == RsFalse we can't do this shorthand mode above.
            Label true_label, end_label;
            AS.BNEZ(Condition, &true_label);
            AS.MV(Rd, RsFalse);
            AS.J(&end_label);
            AS.Bind(&true_label);
            AS.MV(Rd, RsTrue);
            AS.Bind(&end_label);
        }
    }
}

void Emitter::EmitIToV(Backend& backend, biscuit::Vec Vd, biscuit::GPR Rs) {
    AS.VMV_SX(Vd, Rs);
}

void Emitter::EmitVToI(Backend& backend, biscuit::GPR Rd, biscuit::Vec Vs) {
    AS.VMV_XS(Rd, Vs);
}

void Emitter::EmitVFAdd(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs2, biscuit::Vec Vs1) {
    AS.VFADD(Vd, Vs2, Vs1);
}

void Emitter::EmitVFSub(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs2, biscuit::Vec Vs1) {
    AS.VFSUB(Vd, Vs2, Vs1);
}

void Emitter::EmitVFMul(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs2, biscuit::Vec Vs1) {
    AS.VFMUL(Vd, Vs2, Vs1);
}

void Emitter::EmitVFDiv(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs2, biscuit::Vec Vs1) {
    AS.VFDIV(Vd, Vs2, Vs1);
}

void Emitter::EmitVFSqrt(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs) {
    AS.VFSQRT(Vd, Vs);
}

void Emitter::EmitVFRcp(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs) {
    AS.VFREC7(Vd, Vs);
}

void Emitter::EmitVFRcpSqrt(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs) {
    AS.VFRSQRT7(Vd, Vs);
}

void Emitter::EmitVFMin(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs2, biscuit::Vec Vs1) {
    AS.VFMIN(Vd, Vs2, Vs1);
}

void Emitter::EmitVFMax(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs2, biscuit::Vec Vs1) {
    AS.VFMAX(Vd, Vs2, Vs1);
}

void Emitter::EmitSetVectorStateFloat(Backend& backend) {
    // Operate on one element, 32-bits
    AS.VSETIVLI(x0, 1, SEW::E32);
}

void Emitter::EmitSetVectorStateDouble(Backend& backend) {
    // Operate on one element, 64-bits
    AS.VSETIVLI(x0, 1, SEW::E64);
}

void Emitter::EmitSetVectorStatePackedByte(Backend& backend) {
    // Operate on VLEN/8 elements, 8-bits
    static_assert(SUPPORTED_VLEN / 8 < 31); // for when we upgrade to 256-bit vectors
    AS.VSETIVLI(x0, SUPPORTED_VLEN / 8, SEW::E8);
}

void Emitter::EmitSetVectorStatePackedWord(Backend& backend) {
    // Operate on VLEN/16 elements, 16-bits
    AS.VSETIVLI(x0, SUPPORTED_VLEN / 16, SEW::E16);
}

void Emitter::EmitSetVectorStatePackedDWord(Backend& backend) {
    // Operate on VLEN/32 elements, 32-bits
    AS.VSETIVLI(x0, SUPPORTED_VLEN / 32, SEW::E32);
}

void Emitter::EmitSetVectorStatePackedQWord(Backend& backend) {
    // Operate on VLEN/64 elements, 64-bits
    AS.VSETIVLI(x0, SUPPORTED_VLEN / 64, SEW::E64);
}

void Emitter::EmitVInsertInteger(Backend& backend, biscuit::Vec, biscuit::GPR, biscuit::Vec, u64) {
    UNREACHABLE();
}

void Emitter::EmitVExtractInteger(Backend& backend, biscuit::GPR, biscuit::Vec, u64) {
    UNREACHABLE();
}

void Emitter::EmitVAnd(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs2, biscuit::Vec Vs1) {
    AS.VAND(Vd, Vs2, Vs1);
}

void Emitter::EmitVOr(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs2, biscuit::Vec Vs1) {
    AS.VOR(Vd, Vs2, Vs1);
}

void Emitter::EmitVXor(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs2, biscuit::Vec Vs1) {
    AS.VXOR(Vd, Vs2, Vs1);
}

void Emitter::EmitVXori(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs, u64 immediate) {
    AS.VXOR(Vd, Vs, immediate);
}

void Emitter::EmitVSub(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs2, biscuit::Vec Vs1) {
    UNREACHABLE();
}

void Emitter::EmitVAdd(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs2, biscuit::Vec Vs1) {
    AS.VADD(Vd, Vs2, Vs1);
}

void Emitter::EmitVEqual(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs2, biscuit::Vec Vs1, VecMask masked) {
    AS.VMSEQ(Vd, Vs2, Vs1, masked);
}

void Emitter::EmitSetVMask(Backend& backend, biscuit::Vec Vs) {
    AS.VMV(v0, Vs);
}

void Emitter::EmitVIota(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs, VecMask masked) {
    AS.VIOTA(Vd, Vs, masked);
}

void Emitter::EmitVGather(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs1, biscuit::Vec Vs2, biscuit::Vec Viota, VecMask masked) {
    if (Vd != Vs2 && Vd != Viota) {
        AS.VMV(Vd, Vs1);
        AS.VRGATHER(Vd, Vs2, Viota, masked);
    } else {
        // We don't wanna modify Vs1
        AS.VMV(v1, Vs1);
        AS.VRGATHER(v1, Vs2, Viota, masked);
        AS.VMV(Vd, v1);
    }
}

void Emitter::EmitVSplat(Backend& backend, biscuit::Vec Vd, biscuit::GPR Rs) {
    AS.VMV(Vd, Rs);
}

void Emitter::EmitVSplati(Backend& backend, biscuit::Vec Vd, u64 immediate) {
    AS.VMV(Vd, immediate);
}

void Emitter::EmitVSlli(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs, u64 immediate, VecMask masked) {
    AS.VSLL(Vd, Vs, immediate, masked);
}

void Emitter::EmitVSrai(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs, u64 immediate, VecMask masked) {
    AS.VSRA(Vd, Vs, immediate, masked);
}

void Emitter::EmitVMerge(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs2, biscuit::Vec Vs1) {
    AS.VMERGE(Vd, Vs1, Vs2);
}

void Emitter::EmitVMergei(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs, u64 immediate) {
    AS.VMERGE(Vd, Vs, immediate);
}

void Emitter::EmitVSlideDowni(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs, u64 immediate, VecMask masked) {
    AS.VSLIDEDOWN(Vd, Vs, immediate, masked);
}

void Emitter::EmitVSlideUpi(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs, u64 immediate, VecMask masked) {
    if (Vd == Vs) {
        AS.VMV(v1, Vs);
        AS.VSLIDEUP(Vd, v1, immediate, masked);
    } else {
        AS.VSLIDEUP(Vd, Vs, immediate, masked);
    }
}