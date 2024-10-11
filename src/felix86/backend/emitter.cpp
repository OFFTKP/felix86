#include "felix86/backend/backend.hpp"
#include "felix86/backend/emitter.hpp"
#include "felix86/hle/cpuid.hpp"
#include "felix86/hle/syscall.hpp"

#define AS (backend.GetAssembler())

#define MAYBE_C(operation)                                                                                                                           \
    if (Rd == Rs1) {                                                                                                                                 \
        AS.C_##operation(Rd, Rs2);                                                                                                                   \
    } else if (Rd == Rs2) {                                                                                                                          \
        AS.C_##operation(Rd, Rs1);                                                                                                                   \
    } else {                                                                                                                                         \
        AS.operation(Rd, Rs1, Rs2);                                                                                                                  \
    }

namespace {

// Nothing ships these extensions yet and AFAIK there's no way to check for them
constexpr bool HasZabha() {
    return false;
}

constexpr bool HasZacas() {
    return false;
}

constexpr bool HasB() {
    return false;
}

void EmitCrash(Backend& backend, ExitReason reason) {
    Emitter::EmitSetExitReason(backend, static_cast<u64>(reason));
    Emitter::EmitJump(backend, backend.GetCrashTarget());
}

void EmitZext8(Backend& backend, biscuit::GPR Rd) {
    AS.C_ANDI(Rd, 0xFF);
}

void EmitZext16(Backend& backend, biscuit::GPR Rd) {
    AS.C_SLLI(Rd, 48);
    AS.C_SRLI(Rd, 48);
}

void EmitZext32(Backend& backend, biscuit::GPR Rd) {
    AS.C_SLLI(Rd, 32);
    AS.C_SRLI(Rd, 32);
}

void SoftwareCtz(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs, u32 size) {
    WARN("Untested CTZ implementation");
    biscuit::GPR mask = backend.AcquireScratchGPR();
    biscuit::GPR counter = backend.AcquireScratchGPR();
    AS.LI(mask, 1);

    Label loop, end;
    AS.Bind(&loop);
    AS.AND(Rd, Rs, mask);
    AS.BNEZ(Rd, &end);
    AS.C_ADDI(counter, 1);
    AS.C_SLLI(mask, 1);
    AS.LI(Rd, size);
    AS.SLTU(Rd, counter, Rd);
    AS.BNEZ(Rd, &loop);

    AS.Bind(&end);
    AS.MV(Rd, counter);
}

using Operation = void (biscuit::Assembler::*)(biscuit::GPR, biscuit::GPR, biscuit::GPR);

void SoftwareAtomicFetchRMW8(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs, biscuit::GPR Address, biscuit::Ordering ordering,
                             Operation operation) {
    if (ordering != biscuit::Ordering::AQRL) {
        UNIMPLEMENTED();
    }

    // Since there's no 8-bit atomics yet, we need to emulate the RMW operations using 32-bit SC/LL.
    // We need a mask for the byte we want to modify and a mask for the bytes we want to stay the same
    // Load the word, do the operation, mask the resulting byte and the initial bytes, or them together and SC.
    // Also save the initial read value in Rd

    biscuit::GPR scratch = backend.AcquireScratchGPR();
    biscuit::GPR scratch2 = backend.AcquireScratchGPR();
    biscuit::GPR mask = backend.AcquireScratchGPR();
    biscuit::GPR mask_not = backend.AcquireScratchGPR();
    biscuit::GPR Rs_shifted = backend.AcquireScratchGPR();
    biscuit::GPR address_aligned = backend.AcquireScratchGPR();

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
    (AS.*operation)(scratch2, Rd, Rs_shifted);
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
}

void SoftwareAtomicFetchRMW16(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs, biscuit::GPR Address, biscuit::Ordering ordering,
                              Operation operation) {
    if (ordering != biscuit::Ordering::AQRL) {
        UNIMPLEMENTED();
    }

    // See SoftwareAtomicFetchRMW8

    biscuit::GPR scratch = backend.AcquireScratchGPR();
    biscuit::GPR scratch2 = backend.AcquireScratchGPR();
    biscuit::GPR mask = backend.AcquireScratchGPR();
    biscuit::GPR mask_not = backend.AcquireScratchGPR();
    biscuit::GPR Rs_shifted = backend.AcquireScratchGPR();
    biscuit::GPR address_aligned = backend.AcquireScratchGPR();

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
    (AS.*operation)(scratch2, Rd, Rs_shifted);
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
}

[[nodiscard]] constexpr bool IsValidSigned12BitImm(ptrdiff_t value) {
    return value >= -2048 && value <= 2047;
}

// Inefficient but push/pop everything for now around calls
void PushAllCallerSaved(Backend& backend) {
    auto& caller_saved_gprs = Registers::GetCallerSavedGPRs();
    auto& caller_saved_fprs = Registers::GetCallerSavedFPRs();

    constexpr i64 size = 8 * (caller_saved_gprs.size() + caller_saved_fprs.size());
    constexpr i64 gprs_size = 8 * caller_saved_gprs.size();

    AS.ADDI(Registers::StackPointer(), Registers::StackPointer(), -size);

    for (size_t i = 0; i < caller_saved_gprs.size(); i++) {
        AS.SD(caller_saved_gprs[i], i * 8, Registers::StackPointer());
    }

    for (size_t i = 0; i < caller_saved_fprs.size(); i++) {
        AS.FSD(caller_saved_fprs[i], gprs_size + i * 8, Registers::StackPointer());
    }
}

void PopAllCallerSaved(Backend& backend) {
    auto& caller_saved_gprs = Registers::GetCallerSavedGPRs();
    auto& caller_saved_fprs = Registers::GetCallerSavedFPRs();

    constexpr i64 size = 8 * (caller_saved_gprs.size() + caller_saved_fprs.size());
    constexpr i64 gprs_size = 8 * caller_saved_gprs.size();

    for (size_t i = 0; i < caller_saved_gprs.size(); i++) {
        AS.LD(caller_saved_gprs[i], i * 8, Registers::StackPointer());
    }

    for (size_t i = 0; i < caller_saved_fprs.size(); i++) {
        AS.FLD(caller_saved_fprs[i], gprs_size + i * 8, Registers::StackPointer());
    }

    AS.ADDI(Registers::StackPointer(), Registers::StackPointer(), size);
}

// Sanity check for alignment until we want to properly implement unaligned atomics
void EmitAlignmentCheck(Backend& backend, biscuit::GPR address, u8 alignment) {
    biscuit::Label ok;
    AS.ANDI(address, address, alignment - 1);
    AS.BEQZ(address, &ok);
    EmitCrash(backend, ExitReason::EXIT_REASON_BAD_ALIGNMENT);
    AS.Bind(&ok);
}

} // namespace

void Emitter::EmitJump(Backend& backend, void* target) {
    auto my_abs = [](u64 x) -> u64 { return x < 0 ? -x : x; };

    // Check if target is in one MB range
    void* cursor = AS.GetCursorPointer();
    if (my_abs((u64)cursor - (u64)target) > 0x100000) {
        biscuit::GPR scratch = backend.AcquireScratchGPR();
        AS.LI(scratch, (u64)target);
        AS.JR(scratch);
        backend.ReleaseScratchRegs();
    } else {
        AS.J((u64)target - (u64)cursor);
    }
}

void Emitter::EmitJumpConditional(Backend& backend, biscuit::GPR condition, void* target_true, void* target_false) {
    biscuit::GPR address_true = backend.AcquireScratchGPR();
    biscuit::GPR address_false = backend.AcquireScratchGPR();
    Label false_label;

    // TODO: emit relative jumps if possible
    AS.BEQZ(condition, &false_label);
    AS.LI(address_true, (u64)target_true);
    AS.JR(address_true);
    AS.Bind(&false_label);
    AS.LI(address_false, (u64)target_false);
    AS.JR(address_false);

    backend.ReleaseScratchRegs();
}

void Emitter::EmitSetExitReason(Backend& backend, u64 reason) {
    biscuit::GPR reason_reg = backend.AcquireScratchGPR();
    AS.LI(reason_reg, (u8)reason);
    AS.SB(reason_reg, offsetof(ThreadState, exit_dispatcher_flag), Registers::ThreadStatePointer());
}

void Emitter::EmitMov(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    if (Rd == Rs) {
        return;
    }

    AS.C_MV(Rd, Rs);
}

void Emitter::EmitMov(Backend& backend, biscuit::FPR Rd, biscuit::FPR Rs) {
    if (Rd == Rs) {
        return;
    }

    AS.FMV_D(Rd, Rs);
}

void Emitter::EmitMov(Backend& backend, biscuit::Vec Rd, biscuit::Vec Rs) {
    if (Rd == Rs) {
        return;
    }

    AS.VMV(Rd, Rs);
}

void Emitter::EmitImmediate(Backend& backend, biscuit::GPR Rd, u64 immediate) {
    AS.LI(Rd, immediate);
}

void Emitter::EmitRdtsc(Backend& backend) {
    UNREACHABLE();
}

void Emitter::EmitSyscall(Backend& backend) {
    PushAllCallerSaved(backend);

    AS.LI(a0, (u64)&backend.GetEmulator()); // TODO: maybe make Emulator class global...
    AS.MV(a1, Registers::ThreadStatePointer());
    AS.LI(a2, (u64)felix86_syscall); // TODO: remove when moving code buffer close to text?
    AS.JALR(a2);

    PopAllCallerSaved(backend);
}

void Emitter::EmitCpuid(Backend& backend) {
    PushAllCallerSaved(backend);

    AS.MV(a0, Registers::ThreadStatePointer());
    AS.LI(a1, (u64)felix86_cpuid);
    AS.JALR(a1);

    PopAllCallerSaved(backend);
}

void Emitter::EmitSext8(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    if (Rd == Rs) {
        AS.C_SEXT_B(Rd);
    } else {
        AS.SEXTB(Rd, Rs);
    }
}

void Emitter::EmitSext16(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    if (Rd == Rs) {
        AS.C_SEXT_H(Rd);
    } else {
        AS.SEXTH(Rd, Rs);
    }
}

void Emitter::EmitSext32(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    if (Rd == Rs) {
        AS.C_ADDIW(Rd, 0);
    } else {
        AS.ADDIW(Rd, Rs, 0);
    }
}

void Emitter::EmitClz(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    AS.CLZ(Rd, Rs);
}

void Emitter::EmitCtzh(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    SoftwareCtz(backend, Rd, Rs, 16);
}

void Emitter::EmitCtzw(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    if (HasB()) {
        AS.CTZW(Rd, Rs);
    } else {
        SoftwareCtz(backend, Rd, Rs, 32);
    }
}

void Emitter::EmitCtz(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    if (HasB()) {
        AS.CTZ(Rd, Rs);
    } else {
        SoftwareCtz(backend, Rd, Rs, 64);
    }
}

void Emitter::EmitNot(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    if (Rd == Rs) {
        AS.C_NOT(Rd);
    } else {
        AS.NOT(Rd, Rs);
    }
}

void Emitter::EmitNeg(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    AS.NEG(Rd, Rs);
}

void Emitter::EmitParity(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    if (HasB()) {
        AS.ANDI(Rd, Rs, 0xFF);
        AS.CPOPW(Rd, Rd);
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

        biscuit::GPR scratch = backend.AcquireScratchGPR();
        AS.LI(scratch, (u64)&bitcount);
        AS.ANDI(Rd, Rs, 0xFF);
        AS.C_ADD(Rd, scratch);
        AS.LB(Rd, 0, Rd);
    }
}

void Emitter::EmitDiv128(Backend& backend, biscuit::GPR Rs) {
    UNREACHABLE();
}

void Emitter::EmitDivu128(Backend& backend, biscuit::GPR Rs) {
    UNREACHABLE();
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

void Emitter::EmitReadXmmWord(Backend& backend, biscuit::Vec Vd, biscuit::GPR Address) {
    AS.VLM(Vd, Address);
}

void Emitter::EmitReadByteRelative(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs, u64 offset) {
    AS.LBU(Rd, offset, Rs);
}

void Emitter::EmitReadQWordRelative(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs, u64 offset) {
    AS.LD(Rd, offset, Rs);
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

void Emitter::EmitWriteXmmWord(Backend& backend, biscuit::GPR Address, biscuit::Vec Vs) {
    AS.VSM(Vs, Address);
}

void Emitter::EmitWriteByteRelative(Backend& backend, biscuit::GPR Address, biscuit::GPR Rs, u64 offset) {
    AS.SB(Rs, offset, Address);
}

void Emitter::EmitWriteQWordRelative(Backend& backend, biscuit::GPR Address, biscuit::GPR Rs, u64 offset) {
    AS.SD(Rs, offset, Address);
}

void Emitter::EmitAdd(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    MAYBE_C(ADD);
}

void Emitter::EmitAddi(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs, u64 immediate) {
    if (IsValidSigned12BitImm((i64)immediate)) {
        AS.ADDI(Rd, Rs, (i64)immediate);
    } else {
        biscuit::GPR scratch = backend.AcquireScratchGPR();
        AS.LI(scratch, immediate);
        AS.ADD(Rd, Rs, scratch);
    }
}

void Emitter::EmitAmoAdd8(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    if (HasZabha()) {
        AS.AMOADD_B(ordering, Rd, Rs, Address);
        EmitZext8(backend, Rd);
    } else {
        SoftwareAtomicFetchRMW8(backend, Rd, Rs, Address, ordering, &biscuit::Assembler::ADD);
    }
}

void Emitter::EmitAmoAdd16(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    if (HasZabha()) {
        EmitAlignmentCheck(backend, Address, 2);
        AS.AMOADD_H(ordering, Rd, Rs, Address);
        EmitZext16(backend, Rd);
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
    if (HasZabha()) {
        AS.AMOAND_B(ordering, Rd, Rs, Address);
        EmitZext8(backend, Rd);
    } else {
        SoftwareAtomicFetchRMW8(backend, Rd, Rs, Address, ordering, &biscuit::Assembler::AND);
    }
}

void Emitter::EmitAmoAnd16(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    if (HasZabha()) {
        EmitAlignmentCheck(backend, Address, 2);
        AS.AMOAND_H(ordering, Rd, Rs, Address);
        EmitZext16(backend, Rd);
    } else {
        SoftwareAtomicFetchRMW16(backend, Rd, Rs, Address, ordering, &biscuit::Assembler::AND);
    }
}

void Emitter::EmitAmoAnd32(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    EmitAlignmentCheck(backend, Address, 4);
    AS.AMOAND_W(ordering, Rd, Rs, Address);
    EmitZext32(backend, Rd);
}

void Emitter::EmitAmoAnd64(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    EmitAlignmentCheck(backend, Address, 8);
    AS.AMOAND_D(ordering, Rd, Rs, Address);
}

void Emitter::EmitAmoOr8(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    if (HasZabha()) {
        AS.AMOOR_B(ordering, Rd, Rs, Address);
        EmitZext8(backend, Rd);
    } else {
        SoftwareAtomicFetchRMW8(backend, Rd, Rs, Address, ordering, &biscuit::Assembler::OR);
    }
}

void Emitter::EmitAmoOr16(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    if (HasZabha()) {
        EmitAlignmentCheck(backend, Address, 2);
        AS.AMOOR_H(ordering, Rd, Rs, Address);
        EmitZext16(backend, Rd);
    } else {
        SoftwareAtomicFetchRMW16(backend, Rd, Rs, Address, ordering, &biscuit::Assembler::OR);
    }
}

void Emitter::EmitAmoOr32(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    EmitAlignmentCheck(backend, Address, 4);
    AS.AMOOR_W(ordering, Rd, Rs, Address);
    EmitZext32(backend, Rd);
}

void Emitter::EmitAmoOr64(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    EmitAlignmentCheck(backend, Address, 8);
    AS.AMOOR_D(ordering, Rd, Rs, Address);
}

void Emitter::EmitAmoXor8(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    if (HasZabha()) {
        AS.AMOXOR_B(ordering, Rd, Rs, Address);
        EmitZext8(backend, Rd);
    } else {
        SoftwareAtomicFetchRMW8(backend, Rd, Rs, Address, ordering, &biscuit::Assembler::XOR);
    }
}

void Emitter::EmitAmoXor16(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    if (HasZabha()) {
        EmitAlignmentCheck(backend, Address, 2);
        AS.AMOXOR_H(ordering, Rd, Rs, Address);
        EmitZext16(backend, Rd);
    } else {
        SoftwareAtomicFetchRMW16(backend, Rd, Rs, Address, ordering, &biscuit::Assembler::XOR);
    }
}

void Emitter::EmitAmoXor32(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    EmitAlignmentCheck(backend, Address, 4);
    AS.AMOXOR_W(ordering, Rd, Rs, Address);
    EmitZext32(backend, Rd);
}

void Emitter::EmitAmoXor64(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    EmitAlignmentCheck(backend, Address, 8);
    AS.AMOXOR_D(ordering, Rd, Rs, Address);
}

void Emitter::EmitAmoSwap8(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    if (HasZabha()) {
        AS.AMOSWAP_B(ordering, Rd, Rs, Address);
        EmitZext8(backend, Rd);
    } else {
        UNIMPLEMENTED();
    }
}

void Emitter::EmitAmoSwap16(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    if (HasZabha()) {
        EmitAlignmentCheck(backend, Address, 2);
        AS.AMOSWAP_H(ordering, Rd, Rs, Address);
        EmitZext16(backend, Rd);
    } else {
        UNIMPLEMENTED();
    }
}

void Emitter::EmitAmoSwap32(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    EmitAlignmentCheck(backend, Address, 4);
    AS.AMOSWAP_W(ordering, Rd, Rs, Address);
    EmitZext32(backend, Rd);
}

void Emitter::EmitAmoSwap64(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    EmitAlignmentCheck(backend, Address, 8);
    AS.AMOSWAP_D(ordering, Rd, Rs, Address);
}

void Emitter::EmitAmoCAS8(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Expected, biscuit::GPR Rs,
                          biscuit::Ordering ordering) {
    AS.MV(Rd, Expected);
    if (HasZabha() && HasZacas()) {
        AS.AMOCAS_B(ordering, Rd, Rs, Address);
        EmitZext8(backend, Rd);
    } else {
        UNIMPLEMENTED();
    }
}

void Emitter::EmitAmoCAS16(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Expected, biscuit::GPR Rs,
                           biscuit::Ordering ordering) {
    AS.MV(Rd, Expected);
    if (HasZabha() && HasZacas()) {
        EmitAlignmentCheck(backend, Address, 2);
        AS.AMOCAS_H(ordering, Rd, Rs, Address);
        EmitZext16(backend, Rd);
    } else {
        UNIMPLEMENTED();
    }
}

void Emitter::EmitAmoCAS32(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Expected, biscuit::GPR Rs,
                           biscuit::Ordering ordering) {
    AS.MV(Rd, Expected);
    if (HasZacas()) {
        EmitAlignmentCheck(backend, Address, 4);
        AS.AMOCAS_W(ordering, Rd, Rs, Address);
        EmitZext32(backend, Rd);
    } else {
        UNIMPLEMENTED();
    }
}

void Emitter::EmitAmoCAS64(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Expected, biscuit::GPR Rs,
                           biscuit::Ordering ordering) {
    AS.MV(Rd, Expected);
    if (HasZacas()) {
        EmitAlignmentCheck(backend, Address, 8);
        AS.AMOCAS_D(ordering, Rd, Rs, Address);
    } else {
        UNIMPLEMENTED();
    }
}

void Emitter::EmitSub(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    if (Rd == Rs1) {
        AS.C_SUB(Rd, Rs2);
    } else {
        AS.SUB(Rd, Rs1, Rs2);
    }
}

void Emitter::EmitAnd(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    MAYBE_C(AND);
}

void Emitter::EmitOr(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    MAYBE_C(OR);
}

void Emitter::EmitXor(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    MAYBE_C(XOR);
}

void Emitter::EmitEqual(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    MAYBE_C(XOR);
    AS.SEQZ(Rd, Rd);
}

void Emitter::EmitNotEqual(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    MAYBE_C(XOR);
    AS.SNEZ(Rd, Rd);
}

void Emitter::EmitSetLessThanSigned(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.SLT(Rd, Rs1, Rs2);
}

void Emitter::EmitSetLessThanUnsigned(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.SLTU(Rd, Rs1, Rs2);
}

void Emitter::EmitShiftLeft(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.SLL(Rd, Rs1, Rs2); // TODO: add more robust shift IR instructions to abuse C_SLLI & co
}

void Emitter::EmitShiftRight(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.SRL(Rd, Rs1, Rs2);
}

void Emitter::EmitShiftRightArithmetic(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.SRA(Rd, Rs1, Rs2);
}

void Emitter::EmitLeftRotate8(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    biscuit::GPR scratch = backend.AcquireScratchGPR();
    AS.ANDI(scratch, Rs2, 0x7);
    AS.SLLW(Rd, Rs1, scratch);
    AS.NEG(scratch, scratch);
    AS.ANDI(scratch, scratch, 0x7);
    AS.SRLW(scratch, Rs1, scratch);
    AS.OR(Rd, Rd, scratch);
    AS.ANDI(Rd, Rd, 0xFF);
}

void Emitter::EmitLeftRotate16(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    biscuit::GPR scratch = backend.AcquireScratchGPR();
    AS.ANDI(scratch, Rs2, 0x1F);
    AS.SLLW(Rd, Rs1, scratch);
    AS.NEG(scratch, scratch);
    AS.ANDI(scratch, scratch, 0x1F);
    AS.SRLW(scratch, Rs1, scratch);
    AS.OR(Rd, Rd, scratch);
    AS.ZEXTH(Rd, Rd);
}

void Emitter::EmitLeftRotate32(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.ROLW(Rd, Rs1, Rs2);
}

void Emitter::EmitLeftRotate64(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.ROL(Rd, Rs1, Rs2);
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
    Label true_label;
    AS.C_MV(Rd, RsTrue);
    AS.C_BNEZ(Condition, &true_label);
    AS.C_MV(Rd, RsFalse);
    AS.Bind(&true_label);
}

void Emitter::EmitCastVectorFromInteger(Backend& backend, biscuit::Vec Vd, biscuit::GPR Rs) {
    AS.VMV_SX(Vd, Rs);
}

void Emitter::EmitCastIntegerFromVector(Backend& backend, biscuit::GPR Rd, biscuit::Vec Vs) {
    AS.VMV_XS(Rd, Vs);
}

void Emitter::EmitVInsertInteger(Backend& backend, biscuit::Vec, biscuit::GPR, biscuit::Vec, u64) {
    UNREACHABLE();
}

void Emitter::EmitVExtractInteger(Backend& backend, biscuit::GPR, biscuit::Vec, u64) {
    UNREACHABLE();
}

void Emitter::EmitVPackedShuffleDWord(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs, u64) {
    UNREACHABLE();
}

void Emitter::EmitVMoveByteMask(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs) {
    UNREACHABLE();
}

void Emitter::EmitVUnpackByteLow(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs1, biscuit::Vec Vs2) {
    UNREACHABLE();
}

void Emitter::EmitVUnpackWordLow(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs1, biscuit::Vec Vs2) {
    UNREACHABLE();
}

void Emitter::EmitVUnpackDWordLow(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs1, biscuit::Vec Vs2) {
    UNREACHABLE();
}

void Emitter::EmitVUnpackQWordLow(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs1, biscuit::Vec Vs2) {
    UNREACHABLE();
}

void Emitter::EmitVAnd(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs1, biscuit::Vec Vs2) {
    AS.VAND(Vd, Vs1, Vs2);
}

void Emitter::EmitVOr(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs1, biscuit::Vec Vs2) {
    AS.VOR(Vd, Vs1, Vs2);
}

void Emitter::EmitVXor(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs1, biscuit::Vec Vs2) {
    AS.VXOR(Vd, Vs1, Vs2);
}

void Emitter::EmitVShiftRight(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs1, biscuit::Vec Vs2) {
    UNREACHABLE();
}

void Emitter::EmitVShiftLeft(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs1, biscuit::Vec Vs2) {
    UNREACHABLE();
}

void Emitter::EmitVPackedSubByte(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs1, biscuit::Vec Vs2) {
    UNREACHABLE();
}

void Emitter::EmitVPackedAddQWord(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs1, biscuit::Vec Vs2) {
    UNREACHABLE();
}

void Emitter::EmitVPackedEqualByte(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs1, biscuit::Vec Vs2) {
    UNREACHABLE();
}

void Emitter::EmitVPackedEqualWord(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs1, biscuit::Vec Vs2) {
    UNREACHABLE();
}

void Emitter::EmitVPackedEqualDWord(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs1, biscuit::Vec Vs2) {
    UNREACHABLE();
}

void Emitter::EmitVPackedMinByte(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs1, biscuit::Vec Vs2) {
    UNREACHABLE();
}

void Emitter::EmitVZext64(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs) {
    UNREACHABLE();
}