#include <sys/mman.h>
#include "felix86/emulator.hpp"
#include "felix86/v2/fast_recompiler.hpp"

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
    ZydisMnemonic mnemonic = decode(rip, instruction, operands);
    switch (mnemonic) {
    default: {
        ERROR("Unhandled instruction %s (%d)", ZydisMnemonicGetString(mnemonic), (int)mnemonic);
        break;
    }
    }
}

biscuit::GPR FastRecompiler::gpr(x86_ref_e reg) {
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

constexpr biscuit::GPR FastRecompiler::threadStatePointer() {
    return x27; // saved register so that when we exit VM we don't have to save it
}

constexpr biscuit::GPR FastRecompiler::scratch(int index) {
    switch (index) {
    case 0:
        return x28;
    case 1:
        return x29;
    case 2:
        return x30;
    case 3:
        return x31;
    default:
        UNREACHABLE();
        return x0;
    }
}

biscuit::Vec FastRecompiler::vec(x86_ref_e reg) {
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

ZydisMnemonic FastRecompiler::decode(u64 rip, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands) {
    ZyanStatus status = ZydisDecoderDecodeFull(&decoder, (void*)rip, 15, &instruction, operands);
    if (!ZYAN_SUCCESS(status)) {
        ERROR("Failed to decode instruction at 0x%016lx", rip);
    }
    return instruction.mnemonic;
}