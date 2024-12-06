#include "felix86/backend/backend.hpp"
#include "felix86/backend/emitter.hpp"

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

biscuit::GPR PickNot(const auto& gprs, std::initializer_list<biscuit::GPR> exclude) {
    for (const auto& gpr : gprs) {
        if (std::find(exclude.begin(), exclude.end(), gpr) == exclude.end()) {
            return gpr;
        }
    }
    UNREACHABLE();
    return x0;
}

void EmitCrash(Backend& backend, ExitReason reason) {
    Emitter::EmitSetExitReason(backend, static_cast<u64>(reason));
    AS.LD(t0, offsetof(ThreadState, crash_handler), Registers::ThreadStatePointer());
    AS.JR(t0);
}

// TODO: pull out to ir emitter
void SoftwareCtz(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs, u32 size) {
    if (Rd == Rs) {
        AS.MV(t0, Rs);
        Rs = t0;
    }
    const auto& gprs = Registers::GetAllocatableGPRs();
    biscuit::GPR mask = Push(backend, PickNot(gprs, {Rd, Rs}));
    biscuit::GPR counter = Push(backend, PickNot(gprs, {Rd, Rs, mask}));
    AS.LI(mask, 1);
    AS.LI(counter, 0);

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

// TODO: pull out to ir emitter
void SoftwareClz(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs, u32 size) {
    if (Rd == Rs) {
        AS.MV(t0, Rs);
        Rs = t0;
    }
    const auto& gprs = Registers::GetAllocatableGPRs();
    biscuit::GPR mask = Push(backend, PickNot(gprs, {Rd, Rs}));
    biscuit::GPR counter = Push(backend, PickNot(gprs, {Rd, Rs, mask}));
    AS.LI(mask, (1ull << (size - 1)));
    AS.LI(counter, 0);

    Label loop, end;
    AS.Bind(&loop);
    AS.AND(Rd, Rs, mask);
    AS.BNEZ(Rd, &end);
    AS.ADDI(counter, counter, 1);
    AS.SRLI(mask, mask, 1);
    AS.LI(Rd, size);
    AS.SLTU(Rd, counter, Rd);
    AS.BNEZ(Rd, &loop);

    AS.Bind(&end);
    AS.MV(Rd, counter);

    Pop(backend, counter);
    Pop(backend, mask);
}

// Sanity check for alignment until we have unaligned atomic extensions
void EmitAlignmentCheck(Backend& backend, biscuit::GPR address, u8 alignment) {
    if (!Extensions::Zam) {
        biscuit::Label ok;
        AS.ANDI(t0, address, alignment - 1);
        AS.BEQZ(t0, &ok);
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
    // Check if target is in one MB range
    u8* cursor = AS.GetCursorPointer();
    if (!IsValidJTypeImm((u64)target - (u64)cursor)) {
        u64 offset = (u64)target - (u64)cursor;
        const auto hi20 = static_cast<int32_t>((static_cast<uint32_t>(offset) + 0x800) >> 12 & 0xFFFFF);
        const auto lo12 = static_cast<int32_t>(offset << 20) >> 20;
        AS.AUIPC(t0, hi20);
        AS.ADDI(t0, t0, lo12);
        AS.JR(t0);
    } else {
        AS.J((u64)target - (u64)cursor);
    }
}

void Emitter::EmitJump(Backend& backend, Label* target) {
    AS.J(target);
}

void Emitter::EmitJumpConditional(Backend& backend, biscuit::GPR condition, Label* target_true, Label* target_false) {
    // TODO: optimizations for nearby labels
    Label false_label;
    AS.BEQZ(condition, &false_label);
    AS.J(target_true);
    AS.Bind(&false_label);
    AS.J(target_false);
}

void Emitter::EmitJumpConditionalFar(Backend& backend, biscuit::GPR condition, void* target_true, void* target_false) {
    Label false_label;
    AS.BEQZ(condition, &false_label);

    {
        u64 offset = (u64)target_true - (u64)AS.GetCursorPointer();
        const auto hi20 = static_cast<int32_t>((static_cast<uint32_t>(offset) + 0x800) >> 12 & 0xFFFFF);
        const auto lo12 = static_cast<int32_t>(offset << 20) >> 20;
        AS.AUIPC(t0, hi20);
        AS.ADDI(t0, t0, lo12);
        AS.JR(t0);
    }

    {
        AS.Bind(&false_label);
        u64 offset = (u64)target_false - (u64)AS.GetCursorPointer();
        const auto hi20 = static_cast<int32_t>((static_cast<uint32_t>(offset) + 0x800) >> 12 & 0xFFFFF);
        const auto lo12 = static_cast<int32_t>(offset << 20) >> 20;
        AS.AUIPC(t0, hi20);
        AS.ADDI(t0, t0, lo12);
        AS.JR(t0);
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
    EmitPushAllCallerSaved(backend);

    AS.LD(t0, offsetof(ThreadState, rdtsc_handler), Registers::ThreadStatePointer());
    AS.MV(a0, Registers::ThreadStatePointer());
    AS.JALR(t0);

    EmitPopAllCallerSaved(backend);
}

void Emitter::EmitSyscall(Backend& backend) {
    EmitPushAllCallerSaved(backend);

    AS.LD(t0, offsetof(ThreadState, syscall_handler), Registers::ThreadStatePointer());
    AS.MV(a0, Registers::ThreadStatePointer());
    AS.JALR(t0);

    EmitPopAllCallerSaved(backend);
}

void Emitter::EmitCpuid(Backend& backend) {
    EmitPushAllCallerSaved(backend);

    AS.LD(t0, offsetof(ThreadState, cpuid_handler), Registers::ThreadStatePointer());
    AS.MV(a0, Registers::ThreadStatePointer());
    AS.JALR(t0);

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
    if (Extensions::B) {
        AS.ZEXTH(Rd, Rs);
    } else {
        AS.SLLI(Rd, Rs, 48);
        AS.SRLI(Rd, Rd, 48);
    }
}

void Emitter::EmitZext32(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    if (Extensions::B) {
        AS.ZEXTW(Rd, Rs);
    } else {
        AS.SLLI(Rd, Rs, 32);
        AS.SRLI(Rd, Rd, 32);
    }
}

void Emitter::EmitClz(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs) {
    if (Extensions::B) {
        AS.CLZ(Rd, Rs);
    } else {
        SoftwareClz(backend, Rd, Rs, 64);
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
    // TODO: pull both of these out to ir emitter
    if (Extensions::B) {
        AS.ANDI(Rd, Rs, 0xFF);
        AS.CPOPW(Rd, Rd);
        AS.ANDI(Rd, Rd, 1);
        AS.XORI(Rd, Rd, 1);
    } else {
        ASSERT_MSG(!g_cache_functions, "TODO: function caching doesn't work here");
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
        static_assert(sizeof(bitcount) == 256, "Invalid bitcount table size");
        // clang-format on

        AS.LI(t0, (u64)&bitcount);
        AS.ANDI(Rd, Rs, 0xFF);
        AS.ADD(Rd, Rd, t0);
        AS.LB(Rd, 0, Rd);
    }
}

void Emitter::EmitDiv128(Backend& backend, biscuit::GPR Rs) {
    EmitPushAllCallerSaved(backend);

    AS.LD(t0, offsetof(ThreadState, div128_handler), Registers::ThreadStatePointer());
    AS.MV(a1, Rs); // a1 must be set first because Rs may be a0
    AS.MV(a0, Registers::ThreadStatePointer());
    AS.JALR(t0);

    EmitPopAllCallerSaved(backend);
}

void Emitter::EmitDivu128(Backend& backend, biscuit::GPR Rs) {
    EmitPushAllCallerSaved(backend);

    AS.LD(t0, offsetof(ThreadState, divu128_handler), Registers::ThreadStatePointer());
    AS.MV(a1, Rs); // a1 must be set first because Rs may be a0
    AS.MV(a0, Registers::ThreadStatePointer());
    AS.JALR(t0);

    EmitPopAllCallerSaved(backend);
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
    case VectorState::FloatBytes:
    case VectorState::DoubleBytes:
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
    case VectorState::Null:
        // This is null when we want to do a full register load because the target and host vlen match
        ASSERT(Extensions::VLEN == SUPPORTED_VLEN);
        AS.VL1RE8(Vd, Address);
        break;
    case VectorState::AnyPacked:
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
    case VectorState::FloatBytes:
    case VectorState::DoubleBytes:
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
    case VectorState::Null:
        // This is null when we want to do a full register load because the target and host vlen match
        ASSERT(Extensions::VLEN == SUPPORTED_VLEN);
        AS.VS1R(Vs, Address); // use this one because we expect the address to be aligned
        break;
    case VectorState::AnyPacked:
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

void Emitter::EmitAddShifted(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs, biscuit::GPR Shifted, u8 shift) {
    if (Extensions::B) {
        switch (shift) {
        case 0: {
            AS.ADD(Rd, Rs, Shifted);
            break;
        }
        case 1: {
            AS.SH1ADD(Rd, Shifted, Rs);
            break;
        }
        case 2: {
            AS.SH2ADD(Rd, Shifted, Rs);
            break;
        }
        case 3: {
            AS.SH3ADD(Rd, Shifted, Rs);
            break;
        }
        default: {
            UNREACHABLE();
        }
        }
    } else if (Extensions::Xtheadba) {
        AS.TH_ADDSL(Rd, Rs, Shifted, shift);
    } else {
        UNREACHABLE();
    }
}

void Emitter::EmitAddi(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs, u64 immediate) {
    ASSERT(Rs != x0);
    if (IsValidSigned12BitImm((i64)immediate)) {
        AS.ADDI(Rd, Rs, (i64)immediate);
    } else {
        UNREACHABLE();
    }
}

void Emitter::EmitLoadReserved32(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::Ordering ordering) {
    AS.LR_W(ordering, Rd, Address);
}

void Emitter::EmitLoadReserved64(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::Ordering ordering) {
    AS.LR_D(ordering, Rd, Address);
}

void Emitter::EmitStoreConditional32(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    AS.SC_W(ordering, Rd, Rs, Address);
}

void Emitter::EmitStoreConditional64(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    AS.SC_D(ordering, Rd, Rs, Address);
}

void Emitter::EmitAmoAdd8(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    if (Extensions::Zabha) {
        AS.AMOADD_B(ordering, Rd, Rs, Address);
        EmitZext8(backend, Rd, Rd);
    } else {
        UNREACHABLE();
    }
}

void Emitter::EmitAmoAdd16(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    if (Extensions::Zabha) {
        EmitAlignmentCheck(backend, Address, 2);
        AS.AMOADD_H(ordering, Rd, Rs, Address);
        EmitZext16(backend, Rd, Rd);
    } else {
        UNREACHABLE();
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
        UNREACHABLE();
    }
}

void Emitter::EmitAmoAnd16(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    if (Extensions::Zabha) {
        EmitAlignmentCheck(backend, Address, 2);
        AS.AMOAND_H(ordering, Rd, Rs, Address);
        EmitZext16(backend, Rd, Rd);
    } else {
        UNREACHABLE();
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
        UNREACHABLE();
    }
}

void Emitter::EmitAmoOr16(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    if (Extensions::Zabha) {
        EmitAlignmentCheck(backend, Address, 2);
        AS.AMOOR_H(ordering, Rd, Rs, Address);
        EmitZext16(backend, Rd, Rd);
    } else {
        UNREACHABLE();
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
        UNREACHABLE();
    }
}

void Emitter::EmitAmoXor16(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    if (Extensions::Zabha) {
        EmitAlignmentCheck(backend, Address, 2);
        AS.AMOXOR_H(ordering, Rd, Rs, Address);
        EmitZext16(backend, Rd, Rd);
    } else {
        UNREACHABLE();
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
        UNREACHABLE();
    }
}

void Emitter::EmitAmoSwap16(Backend& backend, biscuit::GPR Rd, biscuit::GPR Address, biscuit::GPR Rs, biscuit::Ordering ordering) {
    if (Extensions::Zabha) {
        EmitAlignmentCheck(backend, Address, 2);
        AS.AMOSWAP_H(ordering, Rd, Rs, Address);
        EmitZext16(backend, Rd, Rd);
    } else {
        UNREACHABLE();
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
        UNREACHABLE();
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

void Emitter::EmitCZeroEqz(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs, biscuit::GPR Cond) {
    if (Extensions::Zicond) {
        AS.CZERO_EQZ(Rd, Rs, Cond);
    } else {
        ASSERT(Rd != Cond);
        Label eqz;
        if (Rd != Rs)
            AS.MV(Rd, Rs);
        AS.BNEZ(Cond, &eqz);
        AS.LI(Rd, 0);
        AS.Bind(&eqz);
    }
}

void Emitter::EmitCZeroNez(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs, biscuit::GPR Cond) {
    if (Extensions::Zicond) {
        AS.CZERO_NEZ(Rd, Rs, Cond);
    } else {
        ASSERT(Rd != Cond);
        Label nez;
        if (Rd != Rs)
            AS.MV(Rd, Rs);
        AS.BEQZ(Cond, &nez);
        AS.LI(Rd, 0);
        AS.Bind(&nez);
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

void Emitter::EmitRol32(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.ROLW(Rd, Rs1, Rs2);
}

void Emitter::EmitRol64(Backend& backend, biscuit::GPR Rd, biscuit::GPR Rs1, biscuit::GPR Rs2) {
    AS.ROL(Rd, Rs1, Rs2);
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
        if (Rd != RsFalse && Rd != Condition) {
            Label true_label;
            AS.MV(Rd, RsTrue);
            AS.BNEZ(Condition, &true_label);
            AS.MV(Rd, RsFalse);
            AS.Bind(&true_label);
        } else {
            // If Rd == RsFalse || Rd == Condition we can't do this shorthand mode above.
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

void Emitter::EmitSetVectorStateFloatBytes(Backend& backend) {
    // Operate on 4 8-bit elements, 32-bits
    // So that loads can be misaligned
    AS.VSETIVLI(x0, 4, SEW::E8);
}

void Emitter::EmitSetVectorStateDoubleBytes(Backend& backend) {
    // Operate on 8 8-bit elements, 64-bits
    // So that loads can be misaligned
    AS.VSETIVLI(x0, 8, SEW::E8);
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

void Emitter::EmitVMin(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs2, biscuit::Vec Vs1) {
    AS.VMIN(Vd, Vs2, Vs1);
}

void Emitter::EmitVMinu(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs2, biscuit::Vec Vs1) {
    AS.VMINU(Vd, Vs2, Vs1);
}

void Emitter::EmitVMax(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs2, biscuit::Vec Vs1) {
    AS.VMAX(Vd, Vs2, Vs1);
}

void Emitter::EmitVMaxu(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs2, biscuit::Vec Vs1) {
    AS.VMAXU(Vd, Vs2, Vs1);
}

void Emitter::EmitVSub(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs2, biscuit::Vec Vs1) {
    AS.VSUB(Vd, Vs2, Vs1);
}

void Emitter::EmitVAdd(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs2, biscuit::Vec Vs1) {
    AS.VADD(Vd, Vs2, Vs1);
}

void Emitter::EmitVAddi(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs, u64 immediate) {
    AS.VADD(Vd, Vs, immediate);
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

void Emitter::EmitVId(Backend& backend, biscuit::Vec Vd) {
    AS.VID(Vd);
}

void Emitter::EmitVGather(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs1, biscuit::Vec Vs2, biscuit::Vec Viota, VecMask masked) {
    ASSERT(Vd != Vs2 && Vd != Viota);
    if (masked == VecMask::Yes) {
        if (Vd != Vs1)
            AS.VMV(Vd, Vs1);
        AS.VRGATHER(Vd, Vs2, Viota, VecMask::Yes);
    } else {
        AS.VRGATHER(Vd, Vs2, Viota);
    }
}

void Emitter::EmitVSplat(Backend& backend, biscuit::Vec Vd, biscuit::GPR Rs) {
    AS.VMV(Vd, Rs);
}

void Emitter::EmitVSplati(Backend& backend, biscuit::Vec Vd, u64 immediate) {
    AS.VMV(Vd, immediate);
}

void Emitter::EmitVSll(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs, biscuit::GPR Rs, VecMask masked) {
    AS.VSLL(Vd, Vs, Rs, masked);
}

void Emitter::EmitVSlli(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs, u64 immediate, VecMask masked) {
    AS.VSLL(Vd, Vs, immediate, masked);
}

void Emitter::EmitVSrl(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs, biscuit::GPR Rs, VecMask masked) {
    AS.VSRL(Vd, Vs, Rs, masked);
}

void Emitter::EmitVSrli(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs, u64 immediate, VecMask masked) {
    AS.VSRL(Vd, Vs, immediate, masked);
}

void Emitter::EmitVSrai(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs, u64 immediate, VecMask masked) {
    AS.VSRA(Vd, Vs, immediate, masked);
}

void Emitter::EmitVMSeqi(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs, u64 immediate, VecMask masked) {
    AS.VMSEQ(Vd, Vs, immediate, masked);
}

void Emitter::EmitVMSlt(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs, biscuit::GPR Rs, VecMask masked) {
    AS.VMSLT(Vd, Vs, Rs, masked);
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
    ASSERT(Vd != Vs);
    AS.VSLIDEUP(Vd, Vs, immediate, masked);
}

void Emitter::EmitVSlideUpZeroesi(Backend& backend, biscuit::Vec Vd, biscuit::Vec Vs, u64 immediate, VecMask masked) {
    ASSERT(Vd != Vs);
    AS.VMV(Vd, 0);
    AS.VSLIDEUP(Vd, Vs, immediate, masked);
}

void Emitter::EmitVSlide1Up(Backend& backend, biscuit::Vec Vd, biscuit::GPR Rs, biscuit::Vec Vs, VecMask masked) {
    ASSERT(Vd != Vs);
    AS.VSLIDE1UP(Vd, Vs, Rs, masked);
}

void Emitter::EmitVSlide1Down(Backend& backend, biscuit::Vec Vd, biscuit::GPR Rs, biscuit::Vec Vs, VecMask masked) {
    AS.VSLIDE1DOWN(Vd, Vs, Rs, masked);
}

void Emitter::EmitFence(Backend& backend, biscuit::FenceOrder pred, biscuit::FenceOrder succ) {
    AS.FENCE(pred, succ);
}