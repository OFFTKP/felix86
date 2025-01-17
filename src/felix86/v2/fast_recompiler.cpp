#include <sys/mman.h>
#include "felix86/emulator.hpp"
#include "felix86/v2/fast_recompiler.hpp"

#define X(name) void fast_##name(FastRecompiler& rec, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands);
#include "felix86/v2/handlers.inc"
#undef X

constexpr static u64 code_cache_size = 64 * 1024 * 1024;

// If you don't flush the cache the code will randomly SIGILL
static inline void flushICache() {
#if defined(__riscv)
    asm volatile("fence.i" ::: "memory");
#elif defined(__aarch64__)
#pragma message("Don't forget to implement me")
#elif defined(__x86_64__)
    // No need to flush the cache on x86
#endif
}

static u8* allocateCodeCache() {
    u8 prot = PROT_READ | PROT_WRITE | PROT_EXEC;
    u8 flags = MAP_PRIVATE | MAP_ANONYMOUS;

    return (u8*)mmap(nullptr, code_cache_size, prot, flags, -1, 0);
}

static void deallocateCodeCache(u8* memory) {
    munmap(memory, code_cache_size);
}

FastRecompiler::FastRecompiler(Emulator& emulator) : emulator(emulator), code_cache(allocateCodeCache()), as(code_cache, code_cache_size) {
    for (int i = 0; i < 16; i++) {
        metadata[i].reg = (x86_ref_e)(X86_REF_RAX + i);
        metadata[i + 16 + 6].reg = (x86_ref_e)(X86_REF_XMM0 + i);
    }

    metadata[16].reg = X86_REF_CF;
    metadata[17].reg = X86_REF_PF;
    metadata[18].reg = X86_REF_AF;
    metadata[19].reg = X86_REF_ZF;
    metadata[20].reg = X86_REF_SF;
    metadata[21].reg = X86_REF_OF;

    emitDispatcher();

    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
}

FastRecompiler::~FastRecompiler() {
    deallocateCodeCache(code_cache);
}

void FastRecompiler::emitDispatcher() {
    enter_dispatcher = (decltype(enter_dispatcher))as.GetCursorPointer();

    // Give it an initial valid state
    as.VSETIVLI(x0, SUPPORTED_VLEN / 8, SEW::E8);

    // Save the current register state of callee-saved registers and return address
    const auto& saved_gprs = Registers::GetSavedGPRs();
    as.ADDI(sp, sp, -((int)saved_gprs.size() * 8));
    for (size_t i = 0; i < saved_gprs.size(); i++) {
        as.SD(saved_gprs[i], i * sizeof(u64), sp);
    }

    as.MV(threadStatePointer(), a0);

    compile_next_handler = as.GetCursorPointer();

    Label exit_dispatcher_label;

    // If it's not zero it has some exit reason, exit the dispatcher
    as.LB(a0, offsetof(ThreadState, exit_reason), threadStatePointer());
    as.BNEZ(a0, &exit_dispatcher_label);
    as.LI(a0, (u64)&emulator);
    as.MV(a1, threadStatePointer());
    as.LI(a2, (u64)Emulator::CompileNext);
    as.JALR(a2); // returns the function pointer to the compiled function
    as.JR(a0);   // jump to the compiled function

    as.Bind(&exit_dispatcher_label);

    for (size_t i = 0; i < saved_gprs.size(); i++) {
        as.LD(saved_gprs[i], i * sizeof(u64), sp);
    }

    as.ADDI(sp, sp, (int)saved_gprs.size() * 8);

    as.JR(ra);

    flushICache();
}

void* FastRecompiler::compile(u64 rip) {
    if (map.find(rip) != map.end()) {
        return map[rip].first;
    }

    void* start = as.GetCursorPointer();

    // A sequence of code. This is so that we can also call it recursively.
    compileSequence(rip);

    map[rip] = {start, (u64)as.GetCursorPointer() - (u64)start};

    // Make code visible to instruction fetches.
    flushICache();

    return start;
}

void FastRecompiler::compileSequence(u64 rip) {
    compiling = true;
    while (compiling) {
        resetScratch();
        ZydisMnemonic mnemonic = decode(rip, instruction, operands);
        switch (mnemonic) {
#define X(name)                                                                                                                                      \
    case ZYDIS_MNEMONIC_##name:                                                                                                                      \
        fast_##name(*this, instruction, operands);                                                                                                   \
        break;
#include "felix86/v2/handlers.inc"
#undef X
        default: {
            ERROR("Unhandled instruction %s (%02x)", ZydisMnemonicGetString(mnemonic), (int)mnemonic);
            break;
        }
        }
        rip += instruction.length;
    }
}

biscuit::GPR FastRecompiler::allocatedGPR(x86_ref_e reg) {
    switch (reg) {
    case X86_REF_RAX: {
        return biscuit::x5;
    }
    case X86_REF_RCX: {
        return biscuit::x6;
    }
    case X86_REF_RDX: {
        return biscuit::x7;
    }
    case X86_REF_RBX: {
        return biscuit::x8;
    }
    case X86_REF_RSP: {
        return biscuit::x9;
    }
    case X86_REF_RBP: {
        return biscuit::x10;
    }
    case X86_REF_RSI: {
        return biscuit::x11;
    }
    case X86_REF_RDI: {
        return biscuit::x12;
    }
    case X86_REF_R8: {
        return biscuit::x13;
    }
    case X86_REF_R9: {
        return biscuit::x14;
    }
    case X86_REF_R10: {
        return biscuit::x15;
    }
    case X86_REF_R11: {
        return biscuit::x16;
    }
    case X86_REF_R12: {
        return biscuit::x17;
    }
    case X86_REF_R13: {
        return biscuit::x18;
    }
    case X86_REF_R14: {
        return biscuit::x19;
    }
    case X86_REF_R15: {
        return biscuit::x20;
    }
    case X86_REF_CF: {
        return biscuit::x21;
    }
    case X86_REF_PF: {
        return biscuit::x22;
    }
    case X86_REF_AF: {
        return biscuit::x23;
    }
    case X86_REF_ZF: {
        return biscuit::x24;
    }
    case X86_REF_SF: {
        return biscuit::x25;
    }
    case X86_REF_OF: {
        return biscuit::x26;
    }
    default: {
        UNREACHABLE();
        return x0;
    }
    }
}

biscuit::Vec FastRecompiler::allocatedVec(x86_ref_e reg) {
    switch (reg) {
    case X86_REF_XMM0: {
        return biscuit::v1;
    }
    case X86_REF_XMM1: {
        return biscuit::v2;
    }
    case X86_REF_XMM2: {
        return biscuit::v3;
    }
    case X86_REF_XMM3: {
        return biscuit::v4;
    }
    case X86_REF_XMM4: {
        return biscuit::v5;
    }
    case X86_REF_XMM5: {
        return biscuit::v6;
    }
    case X86_REF_XMM6: {
        return biscuit::v7;
    }
    case X86_REF_XMM7: {
        return biscuit::v8;
    }
    case X86_REF_XMM8: {
        return biscuit::v9;
    }
    case X86_REF_XMM9: {
        return biscuit::v10;
    }
    case X86_REF_XMM10: {
        return biscuit::v11;
    }
    case X86_REF_XMM11: {
        return biscuit::v12;
    }
    case X86_REF_XMM12: {
        return biscuit::v13;
    }
    case X86_REF_XMM13: {
        return biscuit::v14;
    }
    case X86_REF_XMM14: {
        return biscuit::v15;
    }
    case X86_REF_XMM15: {
        return biscuit::v16;
    }
    default: {
        UNREACHABLE();
        return v0;
    }
    }
}

constexpr biscuit::GPR FastRecompiler::threadStatePointer() {
    return x27; // saved register so that when we exit VM we don't have to save it
}

biscuit::GPR FastRecompiler::scratch() {
    switch (scratch_index++) {
    case 0:
        return x28;
    case 1:
        return x29;
    case 2:
        return x30;
    case 3:
        return x31;
    default:
        ERROR("Tried to use more than 4 scratch registers");
        return x0;
    }
}

void FastRecompiler::resetScratch() {
    scratch_index = 0;
}

void FastRecompiler::popScratch() {
    scratch_index--;
    ASSERT(scratch_index >= 0);
}

x86_ref_e FastRecompiler::zydisToRef(ZydisRegister reg) {
    x86_ref_e ref;
    switch (reg) {
    case ZYDIS_REGISTER_AL:
    case ZYDIS_REGISTER_AH:
    case ZYDIS_REGISTER_AX:
    case ZYDIS_REGISTER_EAX:
    case ZYDIS_REGISTER_RAX: {
        ref = X86_REF_RAX;
        break;
    }
    case ZYDIS_REGISTER_CL:
    case ZYDIS_REGISTER_CH:
    case ZYDIS_REGISTER_CX:
    case ZYDIS_REGISTER_ECX:
    case ZYDIS_REGISTER_RCX: {
        ref = X86_REF_RCX;
        break;
    }
    case ZYDIS_REGISTER_DL:
    case ZYDIS_REGISTER_DH:
    case ZYDIS_REGISTER_DX:
    case ZYDIS_REGISTER_EDX:
    case ZYDIS_REGISTER_RDX: {
        ref = X86_REF_RDX;
        break;
    }
    case ZYDIS_REGISTER_BL:
    case ZYDIS_REGISTER_BH:
    case ZYDIS_REGISTER_BX:
    case ZYDIS_REGISTER_EBX:
    case ZYDIS_REGISTER_RBX: {
        ref = X86_REF_RBX;
        break;
    }
    case ZYDIS_REGISTER_SPL:
    case ZYDIS_REGISTER_SP:
    case ZYDIS_REGISTER_ESP:
    case ZYDIS_REGISTER_RSP: {
        ref = X86_REF_RSP;
        break;
    }
    case ZYDIS_REGISTER_BPL:
    case ZYDIS_REGISTER_BP:
    case ZYDIS_REGISTER_EBP:
    case ZYDIS_REGISTER_RBP: {
        ref = X86_REF_RBP;
        break;
    }
    case ZYDIS_REGISTER_SIL:
    case ZYDIS_REGISTER_SI:
    case ZYDIS_REGISTER_ESI:
    case ZYDIS_REGISTER_RSI: {
        ref = X86_REF_RSI;
        break;
    }
    case ZYDIS_REGISTER_DIL:
    case ZYDIS_REGISTER_DI:
    case ZYDIS_REGISTER_EDI:
    case ZYDIS_REGISTER_RDI: {
        ref = X86_REF_RDI;
        break;
    }
    case ZYDIS_REGISTER_R8B:
    case ZYDIS_REGISTER_R8W:
    case ZYDIS_REGISTER_R8D:
    case ZYDIS_REGISTER_R8: {
        ref = X86_REF_R8;
        break;
    }
    case ZYDIS_REGISTER_R9B:
    case ZYDIS_REGISTER_R9W:
    case ZYDIS_REGISTER_R9D:
    case ZYDIS_REGISTER_R9: {
        ref = X86_REF_R9;
        break;
    }
    case ZYDIS_REGISTER_R10B:
    case ZYDIS_REGISTER_R10W:
    case ZYDIS_REGISTER_R10D:
    case ZYDIS_REGISTER_R10: {
        ref = X86_REF_R10;
        break;
    }
    case ZYDIS_REGISTER_R11B:
    case ZYDIS_REGISTER_R11W:
    case ZYDIS_REGISTER_R11D:
    case ZYDIS_REGISTER_R11: {
        ref = X86_REF_R11;
        break;
    }
    case ZYDIS_REGISTER_R12B:
    case ZYDIS_REGISTER_R12W:
    case ZYDIS_REGISTER_R12D:
    case ZYDIS_REGISTER_R12: {
        ref = X86_REF_R12;
        break;
    }
    case ZYDIS_REGISTER_R13B:
    case ZYDIS_REGISTER_R13W:
    case ZYDIS_REGISTER_R13D:
    case ZYDIS_REGISTER_R13: {
        ref = X86_REF_R13;
        break;
    }
    case ZYDIS_REGISTER_R14B:
    case ZYDIS_REGISTER_R14W:
    case ZYDIS_REGISTER_R14D:
    case ZYDIS_REGISTER_R14: {
        ref = X86_REF_R14;
        break;
    }
    case ZYDIS_REGISTER_R15B:
    case ZYDIS_REGISTER_R15W:
    case ZYDIS_REGISTER_R15D:
    case ZYDIS_REGISTER_R15: {
        ref = X86_REF_R15;
        break;
    }
    case ZYDIS_REGISTER_XMM0 ... ZYDIS_REGISTER_XMM15: {
        ref = (x86_ref_e)(X86_REF_XMM0 + (reg - ZYDIS_REGISTER_XMM0));
        break;
    }
    default: {
        UNREACHABLE();
        return X86_REF_RAX;
    }
    }

    return ref;
}

biscuit::GPR FastRecompiler::gpr(ZydisRegister reg) {
    x86_ref_e ref = zydisToRef(reg);
    biscuit::GPR gpr = allocatedGPR(ref);

    loadGPR(ref, gpr);

    switch (reg) {
    case ZYDIS_REGISTER_AL:
    case ZYDIS_REGISTER_CL:
    case ZYDIS_REGISTER_DL:
    case ZYDIS_REGISTER_BL:
    case ZYDIS_REGISTER_SPL:
    case ZYDIS_REGISTER_BPL:
    case ZYDIS_REGISTER_SIL:
    case ZYDIS_REGISTER_DIL:
    case ZYDIS_REGISTER_R8B:
    case ZYDIS_REGISTER_R9B:
    case ZYDIS_REGISTER_R10B:
    case ZYDIS_REGISTER_R11B:
    case ZYDIS_REGISTER_R12B:
    case ZYDIS_REGISTER_R13B:
    case ZYDIS_REGISTER_R14B:
    case ZYDIS_REGISTER_R15B: {
        biscuit::GPR gpr8 = scratch();
        as.ANDI(gpr8, gpr, 0xff);
        return gpr8;
    }
    case ZYDIS_REGISTER_AH:
    case ZYDIS_REGISTER_CH:
    case ZYDIS_REGISTER_DH:
    case ZYDIS_REGISTER_BH: {
        biscuit::GPR gpr8 = scratch();
        as.SRLI(gpr8, gpr, 8);
        as.ANDI(gpr8, gpr8, 0xff);
        return gpr8;
    }
    case ZYDIS_REGISTER_AX:
    case ZYDIS_REGISTER_CX:
    case ZYDIS_REGISTER_DX:
    case ZYDIS_REGISTER_BX:
    case ZYDIS_REGISTER_SP:
    case ZYDIS_REGISTER_BP:
    case ZYDIS_REGISTER_SI:
    case ZYDIS_REGISTER_DI:
    case ZYDIS_REGISTER_R8W:
    case ZYDIS_REGISTER_R9W:
    case ZYDIS_REGISTER_R10W:
    case ZYDIS_REGISTER_R11W:
    case ZYDIS_REGISTER_R12W:
    case ZYDIS_REGISTER_R13W:
    case ZYDIS_REGISTER_R14W:
    case ZYDIS_REGISTER_R15W: {
        biscuit::GPR gpr16 = scratch();
        if (Extensions::B) {
            as.ZEXTH(gpr16, gpr);
        } else {
            as.SLLI(gpr16, gpr, 48);
            as.SRLI(gpr16, gpr16, 48);
        }
        return gpr16;
    }
    case ZYDIS_REGISTER_EAX:
    case ZYDIS_REGISTER_ECX:
    case ZYDIS_REGISTER_EDX:
    case ZYDIS_REGISTER_EBX:
    case ZYDIS_REGISTER_ESP:
    case ZYDIS_REGISTER_EBP:
    case ZYDIS_REGISTER_ESI:
    case ZYDIS_REGISTER_EDI:
    case ZYDIS_REGISTER_R8D:
    case ZYDIS_REGISTER_R9D:
    case ZYDIS_REGISTER_R10D:
    case ZYDIS_REGISTER_R11D:
    case ZYDIS_REGISTER_R12D:
    case ZYDIS_REGISTER_R13D:
    case ZYDIS_REGISTER_R14D:
    case ZYDIS_REGISTER_R15D: {
        biscuit::GPR gpr32 = scratch();
        as.ZEXTW(gpr32, gpr);
        return gpr32;
    }
    default: {
        return gpr;
    }
    }
}

biscuit::Vec FastRecompiler::vec(ZydisRegister reg) {
    ASSERT(reg >= ZYDIS_REGISTER_XMM0 && reg <= ZYDIS_REGISTER_XMM15);
    biscuit::Vec vec = allocatedVec(zydisToRef(reg));
    ERROR("todo: loadvec, metadata vector state");
    return vec;
}

ZydisMnemonic FastRecompiler::decode(u64 rip, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands) {
    ZyanStatus status = ZydisDecoderDecodeFull(&decoder, (void*)rip, 15, &instruction, operands);
    if (!ZYAN_SUCCESS(status)) {
        ERROR("Failed to decode instruction at 0x%016lx", rip);
    }
    return instruction.mnemonic;
}

FastRecompiler::RegisterMetadata& FastRecompiler::getMetadata(x86_ref_e reg) {
    switch (reg) {
    case X86_REF_RAX ... X86_REF_R15: {
        return metadata[reg - X86_REF_RAX];
    }
    case X86_REF_CF: {
        return metadata[16];
    }
    case X86_REF_PF: {
        return metadata[17];
    }
    case X86_REF_AF: {
        return metadata[18];
    }
    case X86_REF_ZF: {
        return metadata[19];
    }
    case X86_REF_SF: {
        return metadata[20];
    }
    case X86_REF_OF: {
        return metadata[21];
    }
    case X86_REF_XMM0 ... X86_REF_XMM15: {
        return metadata[reg - X86_REF_XMM0 + 16 + 6];
    }
    default: {
        UNREACHABLE();
        return metadata[0];
    }
    }
}

biscuit::GPR FastRecompiler::getOperandGPR(ZydisDecodedOperand* operand) {
    switch (operand->type) {
    case ZYDIS_OPERAND_TYPE_REGISTER: {
        biscuit::GPR reg = gpr(operand->reg.value);
        return reg;
    }
    case ZYDIS_OPERAND_TYPE_MEMORY: {
        biscuit::GPR address = lea(operand);

        switch (operand->size) {
        case 8: {
            as.LB(address, 0, address);
            break;
        }
        case 16: {
            as.LH(address, 0, address);
            break;
        }
        case 32: {
            as.LW(address, 0, address);
            break;
        }
        case 64: {
            as.LD(address, 0, address);
            break;
        }
        default: {
            UNREACHABLE();
            break;
        }
        }

        return address;
    }
    case ZYDIS_OPERAND_TYPE_IMMEDIATE: {
        biscuit::GPR imm = scratch();
        as.LI(imm, operand->imm.value.u);
        return imm;
    }
    default: {
        UNREACHABLE();
        return x0;
    }
    }
}

void FastRecompiler::setOperandGPR(ZydisDecodedOperand* operand, biscuit::GPR reg) {
    switch (operand->type) {
    case ZYDIS_OPERAND_TYPE_REGISTER: {
        x86_ref_e ref = zydisToRef(operand->reg.value);
        biscuit::GPR dest = allocatedGPR(ref);

        switch (operand->reg.value) {
        case ZYDIS_REGISTER_AL:
        case ZYDIS_REGISTER_CL:
        case ZYDIS_REGISTER_DL:
        case ZYDIS_REGISTER_BL:
        case ZYDIS_REGISTER_SPL:
        case ZYDIS_REGISTER_BPL:
        case ZYDIS_REGISTER_SIL:
        case ZYDIS_REGISTER_DIL:
        case ZYDIS_REGISTER_R8B:
        case ZYDIS_REGISTER_R9B:
        case ZYDIS_REGISTER_R10B:
        case ZYDIS_REGISTER_R11B:
        case ZYDIS_REGISTER_R12B:
        case ZYDIS_REGISTER_R13B:
        case ZYDIS_REGISTER_R14B:
        case ZYDIS_REGISTER_R15B: {
            biscuit::GPR gpr8 = scratch();
            as.ANDI(gpr8, reg, 0xff);
            as.ANDI(dest, dest, ~0xff);
            as.OR(dest, dest, gpr8);
            popScratch();
            break;
        }
        case ZYDIS_REGISTER_AH:
        case ZYDIS_REGISTER_CH:
        case ZYDIS_REGISTER_DH:
        case ZYDIS_REGISTER_BH: {
            biscuit::GPR gpr8 = scratch();
            biscuit::GPR mask = scratch();
            as.LI(mask, 0xff00);
            as.AND(gpr8, reg, mask);
            as.NOT(mask, mask);
            as.AND(dest, dest, mask);
            as.OR(dest, dest, gpr8);
            popScratch();
            popScratch();
            break;
        }
        case ZYDIS_REGISTER_AX:
        case ZYDIS_REGISTER_CX:
        case ZYDIS_REGISTER_DX:
        case ZYDIS_REGISTER_BX:
        case ZYDIS_REGISTER_SP:
        case ZYDIS_REGISTER_BP:
        case ZYDIS_REGISTER_SI:
        case ZYDIS_REGISTER_DI:
        case ZYDIS_REGISTER_R8W:
        case ZYDIS_REGISTER_R9W:
        case ZYDIS_REGISTER_R10W:
        case ZYDIS_REGISTER_R11W:
        case ZYDIS_REGISTER_R12W:
        case ZYDIS_REGISTER_R13W:
        case ZYDIS_REGISTER_R14W:
        case ZYDIS_REGISTER_R15W: {
            biscuit::GPR gpr16 = scratch();
            if (Extensions::B) {
                as.ZEXTH(gpr16, reg);
            } else {
                as.SLLI(gpr16, reg, 48);
                as.SRLI(gpr16, gpr16, 48);
            }
            as.SRLI(dest, dest, 16);
            as.SLLI(dest, dest, 16);
            as.OR(dest, dest, gpr16);
            popScratch();
            break;
        }
        case ZYDIS_REGISTER_EAX:
        case ZYDIS_REGISTER_ECX:
        case ZYDIS_REGISTER_EDX:
        case ZYDIS_REGISTER_EBX:
        case ZYDIS_REGISTER_ESP:
        case ZYDIS_REGISTER_EBP:
        case ZYDIS_REGISTER_ESI:
        case ZYDIS_REGISTER_EDI:
        case ZYDIS_REGISTER_R8D:
        case ZYDIS_REGISTER_R9D:
        case ZYDIS_REGISTER_R10D:
        case ZYDIS_REGISTER_R11D:
        case ZYDIS_REGISTER_R12D:
        case ZYDIS_REGISTER_R13D:
        case ZYDIS_REGISTER_R14D:
        case ZYDIS_REGISTER_R15D: {
            as.ZEXTW(dest, reg);
            break;
        }
        default: {
            as.MV(dest, reg);
            break;
        }
        }

        RegisterMetadata& meta = getMetadata(ref);
        meta.dirty = true;
        meta.loaded = true; // since the value is fresh it's as if we read it from memory
        break;
    }
    case ZYDIS_OPERAND_TYPE_MEMORY: {
        biscuit::GPR address = lea(operand);

        switch (operand->size) {
        case 8: {
            as.SB(address, 0, reg);
            break;
        }
        case 16: {
            as.SH(address, 0, reg);
            break;
        }
        case 32: {
            as.SW(address, 0, reg);
            break;
        }
        case 64: {
            as.SD(address, 0, reg);
            break;
        }
        default: {
            UNREACHABLE();
            break;
        }
        }
        break;
    }
    default: {
        UNREACHABLE();
    }
    }
}

void FastRecompiler::loadGPR(x86_ref_e reg, biscuit::GPR gpr) {
    RegisterMetadata& meta = getMetadata(reg);
    if (meta.loaded) {
        return;
    }

    as.LD(gpr, offsetof(ThreadState, gpr_storage) + (reg - X86_REF_RAX) * sizeof(u64), threadStatePointer());
    meta.loaded = true;
}

biscuit::GPR FastRecompiler::lea(ZydisDecodedOperand* operand) {
    biscuit::GPR address = scratch();

    u8 scale = 0;
    biscuit::GPR base, index;

    // Load displacement first
    as.LI(address, operand->mem.disp.value);

    if (operand->mem.index != ZYDIS_REGISTER_NONE) {
        index = gpr(operand->mem.index);
        scale = operand->mem.scale;
        if (scale != 0) {
            if (Extensions::B) {
                switch (scale) {
                case 1:
                    as.SH1ADD(address, address, index);
                    break;
                case 2:
                    as.SH2ADD(address, address, index);
                    break;
                case 3: {
                    as.SH3ADD(address, address, index);
                    break;
                }
                default: {
                    UNREACHABLE();
                    break;
                }
                }
            } else {
                if (operand->mem.disp.value == 0) {
                    // Can use the address register directly as there's only zero there
                    as.SLLI(address, index, scale);
                } else {
                    biscuit::GPR scale_reg = scratch();
                    as.SLLI(scale_reg, index, scale);
                    as.ADD(address, address, scale_reg);
                    popScratch();
                }
            }
        } else {
            as.ADD(address, address, index);
        }
    }

    if (operand->mem.base != ZYDIS_REGISTER_NONE) {
        base = gpr(operand->mem.base);
        as.ADD(address, address, base);
    }

    if (operand->mem.segment == ZYDIS_REGISTER_FS) {
        biscuit::GPR fs = scratch();
        as.LD(fs, offsetof(ThreadState, fsbase), threadStatePointer());
        as.ADD(address, address, fs);
        popScratch();
    } else if (operand->mem.segment == ZYDIS_REGISTER_GS) {
        biscuit::GPR gs = scratch();
        as.LD(gs, offsetof(ThreadState, gsbase), threadStatePointer());
        as.ADD(address, address, gs);
        popScratch();
    }

    return address;
}

void FastRecompiler::stopCompiling() {
    ASSERT(compiling);
    compiling = false;
}

void FastRecompiler::setExitReason(ExitReason reason) {
    biscuit::GPR reg = scratch();
    as.LI(reg, (int)reason);
    as.SB(reg, offsetof(ThreadState, exit_reason), threadStatePointer());
    popScratch();
}

void FastRecompiler::backToDispatcher() {
    // TODO: stuff for block linking
    biscuit::GPR address = scratch();
    as.LD(address, offsetof(ThreadState, compile_next_handler), threadStatePointer());
    as.JR(address);
    popScratch();
}