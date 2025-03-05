#include <algorithm>
#include <sys/file.h>
#include <sys/mman.h>
#include <unistd.h>
#include "Zydis/Disassembler.h"
#include "biscuit/decoder.hpp"
#include "felix86/emulator.hpp"
#include "felix86/hle/syscall.hpp"
#include "felix86/v2/recompiler.hpp"

#define X(name)                                                                                                                                      \
    void fast_##name(Recompiler& rec, const HandlerMetadata& meta, Assembler& as, ZydisDecodedInstruction& instruction,                              \
                     ZydisDecodedOperand* operands);
#include "felix86/v2/handlers.inc"
#undef X

constexpr static u64 code_cache_size = 64 * 1024 * 1024;

constexpr static std::array saved_gprs = {ra, sp, gp, tp, s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11};

static u8* allocateCodeCache() {
    u8 prot = PROT_READ | PROT_WRITE | PROT_EXEC;
    u8 flags = MAP_PRIVATE | MAP_ANONYMOUS;

    return (u8*)mmap(nullptr, code_cache_size, prot, flags, -1, 0);
}

static void deallocateCodeCache(u8* memory) {
    munmap(memory, code_cache_size);
}

// Some instructions modify the flags conditionally or sometimes they don't modify them at all.
// This needs to be marked as a usage of the flag as it can be passed through if they don't modify,
// and previous instructions need to know that.
static bool flag_passthrough(ZydisMnemonic mnemonic, x86_ref_e flag) {
    switch (mnemonic) {
    case ZYDIS_MNEMONIC_SHL:
    case ZYDIS_MNEMONIC_SHR:
    case ZYDIS_MNEMONIC_SAR:
    case ZYDIS_MNEMONIC_ROL:
    case ZYDIS_MNEMONIC_ROR: {
        return flag == X86_REF_CF || flag == X86_REF_OF;
    }
    default: {
        return false;
    }
    }
}

Recompiler::Recompiler() : code_cache(allocateCodeCache()), as(code_cache, code_cache_size) {
    for (int i = 0; i < 16; i++) {
        metadata[i].reg = (x86_ref_e)(X86_REF_RAX + i);
        metadata[i + 16 + 5].reg = (x86_ref_e)(X86_REF_XMM0 + i);
    }

    metadata[16].reg = X86_REF_CF;
    metadata[17].reg = X86_REF_AF;
    metadata[18].reg = X86_REF_ZF;
    metadata[19].reg = X86_REF_SF;
    metadata[20].reg = X86_REF_OF;

    // Deduplicate code with clearcodecache -> emitNecessaryStuff
    emitDispatcher();
    emitSigreturnThunk();
    emitUnlinkIndirectThunk();
    start_of_code_cache = as.GetCursorPointer();

    ZydisMachineMode mode = g_mode32 ? ZYDIS_MACHINE_MODE_LONG_COMPAT_32 : ZYDIS_MACHINE_MODE_LONG_64;
    ZydisStackWidth stack_width = g_mode32 ? ZYDIS_STACK_WIDTH_32 : ZYDIS_STACK_WIDTH_64;

    ZydisDecoderInit(&decoder, mode, stack_width);
    ZydisDecoderEnableMode(&decoder, ZYDIS_DECODER_MODE_AMD_BRANCHES, ZYAN_TRUE);

    if (g_block_trace > 0) {
        block_trace.resize(g_block_trace);
    }
}

Recompiler::~Recompiler() {
    deallocateCodeCache(code_cache);
}

void Recompiler::emitDispatcher() {
    enter_dispatcher = (decltype(enter_dispatcher))as.GetCursorPointer();

    // Save the current register state of callee-saved registers and return address
    static_assert(sizeof(saved_host_gprs) == saved_gprs.size() * 8);
    as.LI(t0, (u64)this);
    as.ADDI(t0, t0, offsetof(Recompiler, saved_host_gprs));
    for (size_t i = 0; i < saved_gprs.size(); i++) {
        as.SD(saved_gprs[i], i * sizeof(u64), t0);
    }

    as.MV(threadStatePointer(), a0);

    if (g_rsb) {
        // Try to lower the chances that our return stack buffer optimizations end up
        // ruining the data in the host stack, if somehow there's multiple returns before any calls
        as.ADDI(sp, sp, -1024);
        // Also make sure it's aligned
        as.ANDI(sp, sp, -16);
        // In exit_dispatcher the original stack pointer is restored so it's fine that we don't
        // add 1024 to the stack pointer at any point
    }

    compile_next_handler = as.GetCursorPointer();

    Label exit_dispatcher_label;

    as.MV(a0, threadStatePointer());
    // If it's not zero it has some exit reason, exit the dispatcher
    as.LBU(t2, offsetof(ThreadState, exit_reason), threadStatePointer());
    as.BNEZ(t2, &exit_dispatcher_label);
    if (g_rsb) {
        as.SD(sp, offsetof(ThreadState, current_sp), threadStatePointer());
    }
    as.LI(t0, (u64)Emulator::CompileNext);
    as.JALR(t0); // returns the function pointer to the compiled function
    restoreRoundingMode();
    if (g_rsb) {
        as.MV(ra, a0);
        // "return" to the compiled function. This encoding hints to the
        // return stack buffer to pop, which should have been pushed by a jalr
        // when doing backToDispatcher or jumpAndLink
        as.JR(ra);
    } else {
        as.JR(a0);
    }

    as.Bind(&exit_dispatcher_label);

    exit_dispatcher = (decltype(exit_dispatcher))as.GetCursorPointer();

    as.LI(t0, (u64)this);
    as.ADDI(t0, t0, offsetof(Recompiler, saved_host_gprs));
    for (size_t i = 0; i < saved_gprs.size(); i++) {
        as.LD(saved_gprs[i], i * sizeof(u64), t0);
    }

    as.JR(ra);

    flush_icache();
}

HostAddress Recompiler::emitSigreturnThunk() {
    // This piece of code is responsible for moving the thread state pointer to the right place (so we don't have to find it using tid)
    // calling sigreturn, returning and going back to the dispatcher.
    HostAddress here{(u64)as.GetCursorPointer()};
    getBlockMetadata(Signals::magicSigreturnAddress()).address = here;

    as.MV(a0, threadStatePointer());
    as.LI(t0, (u64)Signals::sigreturn);
    as.JALR(t0);
    backToDispatcher();

    return here;
}

HostAddress Recompiler::emitUnlinkIndirectThunk() {
    HostAddress here{(u64)as.GetCursorPointer()};

    unlink_indirect_thunk = (u8*)here.raw();

    as.MV(a0, threadStatePointer());
    as.MV(a1, ra);
    as.ADDI(a1, a1, -11 * 4); // see justification in Recompiler::linkIndirect
    as.LI(t0, (u64)Emulator::UnlinkIndirect);
    as.JR(t0); // Tail jump, UnlinkIndirect is gonna return to ra

    return here;
}

void Recompiler::clearCodeCache(ThreadState* state) {
    std::lock_guard lock(block_map_mutex);
    WARN("Clearing cache on thread %u", gettid());
    as.RewindBuffer();
    block_metadata.clear();
    std::fill(std::begin(block_cache), std::end(block_cache), BlockCacheEntry{});

    emitDispatcher();
    emitSigreturnThunk();
    emitUnlinkIndirectThunk();
    start_of_code_cache = as.GetCursorPointer();

    if (g_rsb) {
        // Need to zero out the stack that rsb has used thus far.
        // Because if the cache is cleared and we hit more RETs than calls,
        // we are gonna be returning to potentially invalid places
        constexpr int index = 1;
        static_assert(saved_gprs[index] == sp);
        u64 stack_start = saved_host_gprs[index];
        u64 stack_end = state->current_sp;

        for (u64 i = stack_end; i < stack_start; i++) {
            *(u8*)i = 0;
        }
    }
}

HostAddress Recompiler::compile(ThreadState* state, HostAddress rip) {
    size_t remaining_size = code_cache_size - as.GetCodeBuffer().GetCursorOffset();
    if (remaining_size < 100'000) { // less than ~100KB left, clear cache
        clearCodeCache(state);
    }

    std::lock_guard lock(block_map_mutex);
    HostAddress start{(u64)as.GetCursorPointer()};

    // Map it immediately so we can optimize conditional branch to self
    getBlockMetadata(rip).address = start;

    // A sequence of code. This is so that we can also call it recursively later.
    HostAddress end_rip = compileSequence(rip);

    // If other blocks were waiting for this block to be linked, link them now
    expirePendingLinks(rip);

    // Mark the page as read-only to catch self-modifying code
    markPagesAsReadOnly(rip, end_rip);

    if (g_perf) {
        if (perf_fd == -1) {
            std::string path = "/tmp/perf-" + std::to_string(getpid()) + ".map";
            FILE* file = fopen(path.c_str(), "w");
            ASSERT(file);
            perf_fd = fileno(file);
        }

        // Executed region not found, update the symbols
        if (!has_region(rip.raw())) {
            update_symbols();
        }

        BlockMetadata& metadata = getBlockMetadata(rip);
        std::string symbol = get_perf_symbol(rip.raw());
        static char buffer[4096];
        size_t size = metadata.address_end.raw() - metadata.address.raw();
        int string_size = snprintf(buffer, 4096, "%lx %lx %s\n", metadata.address.raw(), size, symbol.c_str());
        ASSERT(string_size > 0 && string_size < 4095);

        int locked = flock(perf_fd, LOCK_EX);
        ASSERT(locked == 0);
        int written = syscall(SYS_write, perf_fd, buffer, string_size);
        ASSERT(written == string_size);
        flock(perf_fd, LOCK_UN);
    }

    return start;
}

void Recompiler::markPagesAsReadOnly(HostAddress start, HostAddress end) {
    if (g_dont_protect_pages) {
        return;
    }

    u64 start_page = start.raw() & ~0xFFF;
    u64 end_page = (end.raw() & ~0xFFF) + 0x1000;
    u64 size = end_page - start_page;
    int result = mprotect((void*)start_page, size, PROT_READ);
    if (result != 0) {
        ERROR("Failed to protect pages %016lx-%016lx", start_page, end_page);
    }
}

HostAddress Recompiler::getCompiledBlock(ThreadState* state, HostAddress rip) {
    if (g_use_block_cache) {
        BlockCacheEntry& entry = block_cache[rip.raw() & ((1 << block_cache_bits) - 1)];
        if (entry.guest == rip) {
            return entry.host;
        } else if (blockExists(rip)) {
            HostAddress host = getBlockMetadata(rip).address;
            entry.guest = rip;
            entry.host = host;
            return host;
        } else {
            return compile(state, rip);
        }
    } else {
        if (blockExists(rip)) {
            return getBlockMetadata(rip).address;
        } else {
            return compile(state, rip);
        }
    }

    UNREACHABLE();
    return {};
}

HostAddress Recompiler::compileSequence(HostAddress rip) {
    compiling = true;
    scanFlagUsageAhead(rip);
    HandlerMetadata meta = {rip, rip};
    BlockMetadata& block_meta = getBlockMetadata(rip);

    current_meta = &meta;
    current_block_metadata = &block_meta;
    current_sew = SEW::E1024;
    current_vlen = 0;
    current_grouping = LMUL::M1;

    current_block_metadata->guest_address = meta.rip;

    std::fill(zexted_gprs.begin(), zexted_gprs.end(), false);

    while (compiling) {
        resetScratch();

        if (g_breakpoints.find(meta.rip.raw()) != g_breakpoints.end()) {
            u64 current_address = (u64)as.GetCursorPointer();
            g_breakpoints[meta.rip.raw()].push_back(current_address);
            as.GetCodeBuffer().Emit32(0); // UNIMP instruction
        }

        block_meta.instruction_spans.push_back({meta.rip.toGuest(), HostAddress{(u64)as.GetCursorPointer()}});

        ZydisMnemonic mnemonic = decode(meta.rip, instruction, operands);

        if (g_no_sse2 && (instruction.meta.isa_set == ZYDIS_ISA_SET_SSE2)) {
            ERROR("SSE2 instruction %s at %016lx when FELIX86_NO_SSE2 is enabled", ZydisMnemonicGetString(mnemonic), meta.rip.raw());
        }

        if (g_no_sse3 && (instruction.meta.isa_set == ZYDIS_ISA_SET_SSE3)) {
            ERROR("SSE3 instruction %s at %016lx when FELIX86_NO_SSE3 is enabled", ZydisMnemonicGetString(mnemonic), meta.rip.raw());
        }

        if (g_no_ssse3 && (instruction.meta.isa_set == ZYDIS_ISA_SET_SSSE3)) {
            ERROR("SSSE3 instruction %s at %016lx when FELIX86_NO_SSSE3 is enabled", ZydisMnemonicGetString(mnemonic), meta.rip.raw());
        }

        if (g_no_sse4_1 && (instruction.meta.isa_set == ZYDIS_ISA_SET_SSE4)) {
            ERROR("SSE4.1 instruction %s at %016lx when FELIX86_NO_SSE4_1 is enabled", ZydisMnemonicGetString(mnemonic), meta.rip.raw());
        }

        if (g_no_sse4_2 && (instruction.meta.isa_set == ZYDIS_ISA_SET_SSE4)) {
            ERROR("SSE4.2 instruction %s at %016lx when FELIX86_NO_SSE4_2 is enabled", ZydisMnemonicGetString(mnemonic), meta.rip.raw());
        }

        switch (mnemonic) {
#define X(name)                                                                                                                                      \
    case ZYDIS_MNEMONIC_##name:                                                                                                                      \
        fast_##name(*this, meta, as, instruction, operands);                                                                                         \
        break;
#include "felix86/v2/handlers.inc"
#undef X
        default: {
            ZydisDisassembledInstruction disassembled;
            if (ZYAN_SUCCESS(ZydisDisassembleIntel(ZYDIS_MACHINE_MODE_LONG_64, meta.rip.raw(), (u8*)meta.rip.raw(), 15, &disassembled))) {
                ERROR("Unhandled instruction %s (%02x)", disassembled.text, (int)instruction.opcode);
            } else {
                ERROR("Unhandled instruction %s (%02x)", ZydisMnemonicGetString(mnemonic), (int)instruction.opcode);
            }
            break;
        }
        }

        if (!g_dont_inline_syscalls) {
            checkModifiesRax(instruction, operands);
        }

        meta.rip += instruction.length;

        if (g_single_step && compiling) {
            resetScratch();
            biscuit::GPR rip_after = scratch();
            as.LI(rip_after, meta.rip.toGuest().raw());
            setRip(rip_after);
            writebackDirtyState();
            backToDispatcher();
            stopCompiling();
        }
    }

    current_block_metadata->guest_address_end = meta.rip;
    current_block_metadata->address_end = HostAddress{(u64)as.GetCursorPointer()};
    flush_icache();

    current_block_metadata = nullptr;
    current_meta = nullptr;

    return meta.rip;
}

biscuit::GPR Recompiler::scratch() {
    switch (scratch_index++) {
    case 0:
        return x1;
    case 1:
        return x6;
    case 2:
        return x28;
    case 3:
        return x29;
    case 4:
        return x30;
    case 5:
        return x31;
    default:
        ERROR("Tried to use more than 6 scratch GPRs");
        return x0;
    }
}

bool Recompiler::isScratch(biscuit::GPR reg) {
    return reg == x1 || reg == x6 || reg == x28 || reg == x29 || reg == x30 || reg == x31;
}

biscuit::Vec Recompiler::scratchVec() {
    switch (vector_scratch_index++) {
    case 0:
        return v22;
    case 1:
        return v23;
    case 2:
        return v24;
    case 3:
        return v25;
    case 4:
        return v26;
    case 5:
        return v27;
    case 6:
        return v28;
    case 7:
        return v29;
    case 8:
        return v30;
    case 9:
        return v31;
    default:
        ERROR("Tried to use more than 10 scratch vecs");
        return v0;
    }
}

biscuit::FPR Recompiler::scratchFPR() {
    switch (fpu_scratch_index++) {
    case 0:
        return ft0;
    case 1:
        return ft1;
    case 2:
        return ft2;
    case 3:
        return ft3;
    case 4:
        return ft4;
    case 5:
        return ft5;
    case 6:
        return ft6;
    case 7:
        return ft7;
    default:
        ERROR("Tried to use more than 8 scratch FPRs");
        return ft0;
    }
}

void Recompiler::popScratchVec() {
    vector_scratch_index--;
    ASSERT(vector_scratch_index >= 0);
}

void Recompiler::popScratch() {
    scratch_index--;
    ASSERT(scratch_index >= 0);
}

void Recompiler::popScratchFPR() {
    fpu_scratch_index--;
    ASSERT(fpu_scratch_index >= 0);
}

void Recompiler::resetScratch() {
    scratch_index = 0;
    vector_scratch_index = 0;
    fpu_scratch_index = 0;
}

x86_ref_e Recompiler::zydisToRef(ZydisRegister reg) {
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
    case ZYDIS_REGISTER_RIP: {
        return X86_REF_RIP;
    }
    case ZYDIS_REGISTER_ST0 ... ZYDIS_REGISTER_ST7: {
        return (x86_ref_e)(X86_REF_ST0 + (reg - ZYDIS_REGISTER_ST0));
    }
    default: {
        ERROR("Unhandled register %s", ZydisRegisterGetString(reg));
        ref = X86_REF_RAX;
        break;
    }
    }

    return ref;
}

x86_size_e Recompiler::zydisToSize(ZydisRegister reg) {
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
        return X86_SIZE_BYTE;
    }
    case ZYDIS_REGISTER_AH:
    case ZYDIS_REGISTER_CH:
    case ZYDIS_REGISTER_DH:
    case ZYDIS_REGISTER_BH: {
        return X86_SIZE_BYTE_HIGH;
    }
    case ZYDIS_REGISTER_AX ... ZYDIS_REGISTER_R15W: {
        return X86_SIZE_WORD;
    }
    case ZYDIS_REGISTER_EAX ... ZYDIS_REGISTER_R15D: {
        return X86_SIZE_DWORD;
    }
    case ZYDIS_REGISTER_RAX ... ZYDIS_REGISTER_R15: {
        return X86_SIZE_QWORD;
    }
    case ZYDIS_REGISTER_XMM0 ... ZYDIS_REGISTER_XMM15: {
        return X86_SIZE_XMM;
    }
    default: {
        UNREACHABLE();
        return X86_SIZE_BYTE;
    }
    }
}

biscuit::GPR Recompiler::gpr(ZydisRegister reg) {
    x86_ref_e ref = zydisToRef(reg);
    x86_size_e size = zydisToSize(reg);
    return getRefGPR(ref, size);
}

biscuit::Vec Recompiler::vec(ZydisRegister reg) {
    ASSERT(reg >= ZYDIS_REGISTER_XMM0 && reg <= ZYDIS_REGISTER_XMM15);
    x86_ref_e ref = zydisToRef(reg);
    return getRefVec(ref);
}

ZydisMnemonic Recompiler::decode(HostAddress rip, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands) {
    ZyanStatus status = ZydisDecoderDecodeFull(&decoder, (void*)rip.raw(), 15, &instruction, operands);
    if (!ZYAN_SUCCESS(status)) {
        ERROR("Failed to decode instruction at 0x%016lx", rip.raw());
    }
    return instruction.mnemonic;
}

Recompiler::RegisterMetadata& Recompiler::getMetadata(x86_ref_e reg) {
    switch (reg) {
    case X86_REF_RAX ... X86_REF_R15: {
        return metadata[reg - X86_REF_RAX];
    }
    case X86_REF_CF: {
        return metadata[16];
    }
    case X86_REF_AF: {
        return metadata[17];
    }
    case X86_REF_ZF: {
        return metadata[18];
    }
    case X86_REF_SF: {
        return metadata[19];
    }
    case X86_REF_OF: {
        return metadata[20];
    }
    case X86_REF_XMM0 ... X86_REF_XMM15: {
        return metadata[reg - X86_REF_XMM0 + 16 + 5];
    }
    default: {
        UNREACHABLE();
        return metadata[0];
    }
    }
}

x86_size_e Recompiler::getOperandSize(ZydisDecodedOperand* operand) {
    switch (operand->type) {
    case ZYDIS_OPERAND_TYPE_REGISTER: {
        return zydisToSize(operand->reg.value);
    }
    case ZYDIS_OPERAND_TYPE_MEMORY: {
        switch (operand->size) {
        case 8:
            return X86_SIZE_BYTE;
        case 16:
            return X86_SIZE_WORD;
        case 32:
            return X86_SIZE_DWORD;
        case 64:
            return X86_SIZE_QWORD;
        case 128:
            return X86_SIZE_XMM;
        default:
            UNREACHABLE();
            return X86_SIZE_BYTE;
        }
    }
    case ZYDIS_OPERAND_TYPE_IMMEDIATE: {
        switch (operand->imm.size) {
        case 8:
            return X86_SIZE_BYTE;
        case 16:
            return X86_SIZE_WORD;
        case 32:
            return X86_SIZE_DWORD;
        case 64:
            return X86_SIZE_QWORD;
        default:
            UNREACHABLE();
            return X86_SIZE_BYTE;
        }
    }
    default: {
        UNREACHABLE();
        return X86_SIZE_BYTE;
    }
    }
}

biscuit::GPR Recompiler::getOperandGPR(ZydisDecodedOperand* operand) {
    switch (operand->type) {
    case ZYDIS_OPERAND_TYPE_REGISTER: {
        biscuit::GPR reg = gpr(operand->reg.value);
        return reg;
    }
    case ZYDIS_OPERAND_TYPE_MEMORY: {
        biscuit::GPR address = leaAddBase(operand); // avoids having to use a scratch to add the base address
        readMemoryNoBase(address, address, 0, zydisToSize(operand->size));
        return address;
    }
    case ZYDIS_OPERAND_TYPE_IMMEDIATE: {
        u64 value = operand->imm.value.s;
        biscuit::GPR imm = scratch();
        as.LI(imm, value);
        return imm;
    }
    default: {
        UNREACHABLE();
        return x0;
    }
    }
}

biscuit::Vec Recompiler::getOperandVec(ZydisDecodedOperand* operand) {
    switch (operand->type) {
    case ZYDIS_OPERAND_TYPE_REGISTER: {
        biscuit::Vec reg = vec(operand->reg.value);
        return reg;
    }
    case ZYDIS_OPERAND_TYPE_MEMORY: {
        biscuit::GPR address = leaAddBase(operand);
        biscuit::Vec vec = scratchVec();

        switch (operand->size) {
        case 8: {
            setVectorState(SEW::E8, 1);
            as.VLE8(vec, address); // These won't need to be patched as they can't be unaligned
            break;
        }
        case 16: {
            if (g_paranoid) {
                setVectorState(SEW::E8, 2);
                as.VLE8(vec, address);
            } else {
                if (!setVectorState(SEW::E16, 1)) {
                    as.NOP(); // Add a NOP in case this load needs to be patched and we need to insert a vsetivli
                }
                as.VLE16(vec, address);
                as.NOP(); // in case of a patch, the old vsetivli needs to be moved here to maintain integrity
            }
            break;
        }
        case 32: {
            if (g_paranoid) {
                setVectorState(SEW::E8, 4);
                as.VLE8(vec, address);
            } else {
                if (!setVectorState(SEW::E32, 1)) {
                    as.NOP(); // Add a NOP in case this load needs to be patched and we need to insert a vsetivli
                }
                as.VLE32(vec, address);
                as.NOP(); // in case of a patch, the old vsetivli needs to be moved here to maintain integrity
            }
            break;
        }
        case 64: {
            if (g_paranoid) {
                setVectorState(SEW::E8, 8);
                as.VLE8(vec, address);
            } else {
                if (!setVectorState(SEW::E64, 1)) {
                    as.NOP(); // Add a NOP in case this load needs to be patched and we need to insert a vsetivli
                }
                as.VLE64(vec, address);
                as.NOP(); // in case of a patch, the old vsetivli needs to be moved here to maintain integrity
            }
            break;
        }
        case 128: {
            if (g_paranoid) {
                setVectorState(SEW::E8, 16);
                as.VLE8(vec, address);
            } else {
                if (!setVectorState(SEW::E64, 2)) {
                    as.NOP(); // Add a NOP in case this load needs to be patched and we need to insert a vsetivli
                }
                as.VLE64(vec, address);
                as.NOP(); // in case of a patch, the old vsetivli needs to be moved here to maintain integrity
            }
            break;
        }
        default: {
            UNREACHABLE();
            break;
        }
        }

        popScratch();

        return vec;
    }
    default: {
        UNREACHABLE();
        return v0;
    }
    }
}

u64 Recompiler::getImmediate(ZydisDecodedOperand* operand) {
    ASSERT(operand->type == ZYDIS_OPERAND_TYPE_IMMEDIATE);
    return operand->imm.value.u;
}

biscuit::GPR Recompiler::flag(x86_ref_e ref) {
    if (ref == X86_REF_PF) {
        biscuit::GPR reg = scratch();
        as.LBU(reg, offsetof(ThreadState, pf), threadStatePointer());
        return reg;
    }

    biscuit::GPR reg = allocatedGPR(ref);
    loadGPR(ref, reg);
    return reg;
}

biscuit::GPR Recompiler::flagW(x86_ref_e ref) {
    biscuit::GPR reg = allocatedGPR(ref);
    RegisterMetadata& meta = getMetadata(ref);
    meta.dirty = true;
    meta.loaded = true;
    return reg;
}

biscuit::GPR Recompiler::flagWR(x86_ref_e ref) {
    biscuit::GPR reg = allocatedGPR(ref);
    RegisterMetadata& meta = getMetadata(ref);
    loadGPR(ref, reg);
    meta.dirty = true;
    meta.loaded = true;
    return reg;
}

biscuit::GPR Recompiler::getRefGPR(x86_ref_e ref, x86_size_e size) {
    biscuit::GPR gpr = allocatedGPR(ref);

    loadGPR(ref, gpr);

    switch (size) {
    case X86_SIZE_BYTE: {
        biscuit::GPR gpr8 = scratch();
        zext(gpr8, gpr, X86_SIZE_BYTE);
        return gpr8;
    }
    case X86_SIZE_BYTE_HIGH: {
        biscuit::GPR gpr8 = scratch();
        as.SRLI(gpr8, gpr, 8);
        zext(gpr8, gpr8, X86_SIZE_BYTE);
        return gpr8;
    }
    case X86_SIZE_WORD: {
        biscuit::GPR gpr16 = scratch();
        zext(gpr16, gpr, X86_SIZE_WORD);
        return gpr16;
    }
    case X86_SIZE_DWORD: {
        if (!zexted_gprs[ref - X86_REF_RAX] || g_paranoid) {
            // Need to zext and store in scratch
            biscuit::GPR gpr32 = scratch();
            zext(gpr32, gpr, X86_SIZE_DWORD);
            return gpr32;
        } else {
            // Already zexted when this was last stored in this block
            return gpr;
        }
    }
    case X86_SIZE_QWORD: {
        return gpr;
    }
    default: {
        UNREACHABLE();
        return x0;
    }
    }
}

bool Recompiler::isGPR(ZydisRegister reg) {
    return zydisToRef(reg) >= X86_REF_RAX && zydisToRef(reg) <= X86_REF_R15;
}

biscuit::Vec Recompiler::getRefVec(x86_ref_e ref) {
    biscuit::Vec vec = allocatedVec(ref);

    loadVec(ref, vec);

    return vec;
}

void Recompiler::setRefGPR(x86_ref_e ref, x86_size_e size, biscuit::GPR reg) {
    switch (size) {
    case X86_SIZE_BYTE: {
        ASSERT(reg != allocatedGPR(ref));
        biscuit::GPR dest = getRefGPR(ref, X86_SIZE_QWORD);
        biscuit::GPR gpr8 = scratch();
        as.ANDI(gpr8, reg, 0xff);
        as.ANDI(dest, dest, ~0xff);
        as.OR(dest, dest, gpr8);
        popScratch();
        break;
    }
    case X86_SIZE_BYTE_HIGH: {
        ASSERT(reg != allocatedGPR(ref));
        biscuit::GPR dest = getRefGPR(ref, X86_SIZE_QWORD);
        biscuit::GPR gpr8 = scratch();
        biscuit::GPR mask = scratch();
        as.LI(mask, 0xff00);
        as.SLLI(gpr8, reg, 8);
        as.AND(gpr8, gpr8, mask);
        as.NOT(mask, mask);
        as.AND(dest, dest, mask);
        as.OR(dest, dest, gpr8);
        popScratch();
        popScratch();
        break;
    }
    case X86_SIZE_WORD: {
        ASSERT(reg != allocatedGPR(ref));
        biscuit::GPR dest = getRefGPR(ref, X86_SIZE_QWORD);
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
    case X86_SIZE_DWORD: {
        biscuit::GPR dest = allocatedGPR(ref); // don't need to load as the entire register is overwritten
        if (Extensions::B) {                   // TODO: in 32-bit mode we prob don't need to zero extend and we can do the same as 64-bit movs
            as.ZEXTW(dest, reg);
        } else {
            as.SLLI(dest, reg, 32);
            as.SRLI(dest, dest, 32);
        }
        zexted_gprs[ref - X86_REF_RAX] = true;
        break;
    }
    case X86_SIZE_QWORD: {
        biscuit::GPR dest = allocatedGPR(ref); // don't need to load as the entire register is overwritten
        if (dest != reg)
            as.MV(dest, reg);
        zexted_gprs[ref - X86_REF_RAX] = false;
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }

    RegisterMetadata& meta = getMetadata(ref);
    meta.dirty = true;
    meta.loaded = true; // since the value is fresh it's as if we read it from memory
}

void Recompiler::setRefVec(x86_ref_e ref, biscuit::Vec vec) {
    biscuit::Vec dest = allocatedVec(ref);

    if (dest != vec) {
        as.VMV1R(dest, vec);
    }

    RegisterMetadata& meta = getMetadata(ref);
    meta.dirty = true;
    meta.loaded = true; // since the value is fresh it's as if we read it from memory
}

void Recompiler::setOperandGPR(ZydisDecodedOperand* operand, biscuit::GPR reg) {
    switch (operand->type) {
    case ZYDIS_OPERAND_TYPE_REGISTER: {
        x86_ref_e ref = zydisToRef(operand->reg.value);
        x86_size_e size = zydisToSize(operand->reg.value);
        setRefGPR(ref, size, reg);
        break;
    }
    case ZYDIS_OPERAND_TYPE_MEMORY: {
        biscuit::GPR address = leaAddBase(operand);
        writeMemoryNoBase(reg, address, 0, zydisToSize(operand->size));
        break;
    }
    default: {
        UNREACHABLE();
    }
    }
}

void Recompiler::setOperandVec(ZydisDecodedOperand* operand, biscuit::Vec vec) {
    switch (operand->type) {
    case ZYDIS_OPERAND_TYPE_REGISTER: {
        x86_ref_e ref = zydisToRef(operand->reg.value);
        setRefVec(ref, vec);
        break;
    }
    case ZYDIS_OPERAND_TYPE_MEMORY: {
        biscuit::GPR address = leaAddBase(operand);

        switch (operand->size) {
        case 128: {
            if (g_paranoid) { // don't patch vector accesses in paranoid mode
                setVectorState(SEW::E8, 128 / 8);
                as.VSE8(vec, address);
            } else {
                if (!setVectorState(SEW::E64, 2)) {
                    as.NOP(); // Add a NOP in case this store needs to be patched and we need to insert a vsetivli
                }
                as.VSE64(vec, address);
                as.NOP(); // in case of a patch, the old vsetivli needs to be moved here to maintain integrity
            }
            break;
        }
        case 64: {
            if (g_paranoid) {
                setVectorState(SEW::E8, 64 / 8);
                as.VSE8(vec, address);
            } else {
                if (!setVectorState(SEW::E64, 1)) {
                    as.NOP(); // Add a NOP in case this store needs to be patched and we need to insert a vsetivli
                }
                as.VSE64(vec, address);
                as.NOP(); // in case of a patch, the old vsetivli needs to be moved here to maintain integrity
            }
            break;
        }
        case 32: {
            if (g_paranoid) {
                setVectorState(SEW::E8, 32 / 8);
                as.VSE8(vec, address);
            } else {
                if (!setVectorState(SEW::E32, 1)) {
                    as.NOP(); // Add a NOP in case this store needs to be patched and we need to insert a vsetivli
                }
                as.VSE32(vec, address);
                as.NOP(); // in case of a patch, the old vsetivli needs to be moved here to maintain integrity
            }
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

void Recompiler::loadGPR(x86_ref_e reg, biscuit::GPR gpr) {
    RegisterMetadata& meta = getMetadata(reg);
    if (meta.loaded) {
        return;
    }

    meta.loaded = true;
    if (reg >= X86_REF_RAX && reg <= X86_REF_R15) {
        as.LD(gpr, offsetof(ThreadState, gprs) + (reg - X86_REF_RAX) * sizeof(u64), threadStatePointer());
    } else {
        switch (reg) {
        case X86_REF_CF: {
            as.LBU(gpr, offsetof(ThreadState, cf), threadStatePointer());
            break;
        }
        case X86_REF_AF: {
            as.LBU(gpr, offsetof(ThreadState, af), threadStatePointer());
            break;
        }
        case X86_REF_ZF: {
            as.LBU(gpr, offsetof(ThreadState, zf), threadStatePointer());
            break;
        }
        case X86_REF_SF: {
            as.LBU(gpr, offsetof(ThreadState, sf), threadStatePointer());
            break;
        }
        case X86_REF_OF: {
            as.LBU(gpr, offsetof(ThreadState, of), threadStatePointer());
            break;
        }
        default: {
            UNREACHABLE();
            break;
        }
        }
    }
}

void Recompiler::loadVec(x86_ref_e reg, biscuit::Vec vec) {
    RegisterMetadata& meta = getMetadata(reg);
    if (meta.loaded) {
        return;
    }

    meta.loaded = true;
    biscuit::GPR address = scratch();
    u64 offset = offsetof(ThreadState, xmm) + (reg - X86_REF_XMM0) * 16;
    as.ADDI(address, threadStatePointer(), offset);
    setVectorState(SEW::E64, maxVlen() / 64);
    as.VLE64(vec, address);
    popScratch();
}

bool Recompiler::setVectorState(SEW sew, int vlen, LMUL grouping) {
    if (current_sew == sew && current_vlen == vlen && current_grouping == grouping) {
        return false;
    }

    current_sew = sew;
    current_vlen = vlen;
    current_grouping = grouping;

    as.VSETIVLI(x0, vlen, sew, grouping);
    return true;
}

biscuit::GPR Recompiler::leaAddBase(ZydisDecodedOperand* operand) {
    // Make sure the lea does an LI including the address space base
    operand->mem.disp.value += g_address_space_base;
    biscuit::GPR ret = lea(operand);
    operand->mem.disp.value -= g_address_space_base;

    return ret;
}

biscuit::GPR Recompiler::lea(ZydisDecodedOperand* operand) {
    biscuit::GPR address = scratch();

    biscuit::GPR base, index;

    if (operand->mem.base == ZYDIS_REGISTER_RIP) {
        as.LI(address, current_meta->rip.toGuest().raw() + instruction.length + operand->mem.disp.value);
        return address;
    }

    // Load displacement first
    as.LI(address, operand->mem.disp.value);

    if (operand->mem.index != ZYDIS_REGISTER_NONE) {
        index = gpr(operand->mem.index);
        u8 scale = operand->mem.scale;
        if (scale != 1) {
            if (Extensions::B) {
                switch (scale) {
                case 2:
                    as.SH1ADD(address, index, address);
                    break;
                case 4:
                    as.SH2ADD(address, index, address);
                    break;
                case 8: {
                    as.SH3ADD(address, index, address);
                    break;
                }
                default: {
                    UNREACHABLE();
                    break;
                }
                }
            } else {
                switch (scale) {
                case 2:
                    scale = 1;
                    break;
                case 4:
                    scale = 2;
                    break;
                case 8:
                    scale = 3;
                    break;
                default:
                    UNREACHABLE();
                    break;
                }
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

    // Address override prefix
    if (instruction.address_width == 32 && !g_mode32) {
        zext(address, address, X86_SIZE_DWORD);
    }

    return address;
}

void Recompiler::stopCompiling() {
    ASSERT(compiling);
    compiling = false;
}

void Recompiler::pushCalltrace() {
    if (g_calltrace) {
        as.LI(t0, (u64)push_calltrace);
        as.MV(a0, threadStatePointer());
        as.LI(a1, current_meta->rip.raw());
        as.JALR(t0);
    }
}

void Recompiler::popCalltrace() {
    if (g_calltrace) {
        as.LI(t0, (u64)pop_calltrace);
        as.MV(a0, threadStatePointer());
        as.JALR(t0);
    }
}

void Recompiler::setExitReason(ExitReason reason) {
    biscuit::GPR reg = scratch();
    as.LI(reg, (int)reason);
    as.SB(reg, offsetof(ThreadState, exit_reason), threadStatePointer());
    popScratch();
}

void Recompiler::writebackDirtyState() {
    for (int i = 0; i < 16; i++) {
        x86_ref_e ref = (x86_ref_e)(X86_REF_RAX + i);
        if (getMetadata(ref).dirty) {
            as.SD(allocatedGPR(ref), offsetof(ThreadState, gprs) + i * sizeof(u64), threadStatePointer());
        }
    }

    biscuit::GPR address = scratch();
    for (int i = 0; i < 16; i++) {
        x86_ref_e ref = (x86_ref_e)(X86_REF_XMM0 + i);
        if (getMetadata(ref).dirty) {
            setVectorState(SEW::E64, maxVlen() / 64);
            as.ADDI(address, threadStatePointer(), offsetof(ThreadState, xmm) + i * 16);
            as.VSE64(allocatedVec(ref), address);
        }
    }
    popScratch();

    if (getMetadata(X86_REF_CF).dirty) {
        as.SB(allocatedGPR(X86_REF_CF), offsetof(ThreadState, cf), threadStatePointer());
    }

    if (getMetadata(X86_REF_AF).dirty) {
        as.SB(allocatedGPR(X86_REF_AF), offsetof(ThreadState, af), threadStatePointer());
    }

    if (getMetadata(X86_REF_ZF).dirty) {
        as.SB(allocatedGPR(X86_REF_ZF), offsetof(ThreadState, zf), threadStatePointer());
    }

    if (getMetadata(X86_REF_SF).dirty) {
        as.SB(allocatedGPR(X86_REF_SF), offsetof(ThreadState, sf), threadStatePointer());
    }

    if (getMetadata(X86_REF_OF).dirty) {
        as.SB(allocatedGPR(X86_REF_OF), offsetof(ThreadState, of), threadStatePointer());
    }

    for (size_t i = 0; i < metadata.size(); i++) {
        metadata[i].dirty = false;
        metadata[i].loaded = false;
    }

    current_sew = SEW::E1024;
    current_vlen = 0;
    current_grouping = LMUL::M1;
    rounding_mode_set = false;
}

void Recompiler::invalidStateUntilJump() {
    // This instruction hints to the state reconstruction in the signal handler that the state was written back
    // and is invalid, until a jump. For example, if a block does something like
    // writebackDirtyState()
    // modify a0, a1, t0
    // jump to function to handle stuff
    // Then we need a way of communicating that between writebackDirtyState and the jump, the state (a0, a1, t0) is invalid
    // and to not reconstruct ThreadState using those registers. This NOP instruction below will serve this purpose.
    as.SRLI(x0, x0, 42);
}

void Recompiler::restoreRoundingMode() {
    biscuit::GPR rm = scratch();
    as.LBU(rm, offsetof(ThreadState, rmode), threadStatePointer());
    as.FSRM(x0, rm);
    popScratch();
}

void Recompiler::backToDispatcher(bool use_rsb) {
    const u64 offset = (u64)compile_next_handler - (u64)as.GetCursorPointer();
    ASSERT(IsValid2GBImm(offset));
    const auto hi20 = static_cast<int32_t>(((static_cast<uint32_t>(offset) + 0x800) >> 12) & 0xFFFFF);
    const auto lo12 = static_cast<int32_t>(offset << 20) >> 20;
    as.AUIPC(t0, hi20);
    if (use_rsb) {
        as.JALR(ra, lo12, t0);
    } else {
        as.JR(t0, lo12);
    }
}

void Recompiler::enterDispatcher(ThreadState* state) {
    enter_dispatcher(state);
}

void Recompiler::exitDispatcher(ThreadState* state) {
    exit_dispatcher(state);
    __builtin_unreachable();
}

void* Recompiler::getCompileNext() {
    return compile_next_handler;
}

// TODO: this is bad. Make it so we emit flags right before an instruction that needs them as we go through emitting them
// instead of scanning forward
void Recompiler::scanFlagUsageAhead(HostAddress rip) {
    for (int i = 0; i < 6; i++) {
        flag_access_cpazso[i].clear();
    }

    while (true) {
        ZydisDecodedInstruction instruction;
        ZydisDecodedOperand operands[10];
        ZydisMnemonic mnemonic = decode(rip, instruction, operands);
        bool is_jump = instruction.meta.branch_type != ZYDIS_BRANCH_TYPE_NONE;
        bool is_ret = mnemonic == ZYDIS_MNEMONIC_RET;
        bool is_call = mnemonic == ZYDIS_MNEMONIC_CALL;
        bool is_illegal = mnemonic == ZYDIS_MNEMONIC_UD2;
        bool is_hlt = mnemonic == ZYDIS_MNEMONIC_HLT;

        if (instruction.attributes & ZYDIS_ATTRIB_CPUFLAG_ACCESS) {
            u32 changed =
                instruction.cpu_flags->modified | instruction.cpu_flags->set_0 | instruction.cpu_flags->set_1 | instruction.cpu_flags->undefined;
            u32 used = instruction.cpu_flags->tested;

            if (used & ZYDIS_CPUFLAG_CF || flag_passthrough(instruction.mnemonic, X86_REF_CF)) {
                flag_access_cpazso[0].push_back({false, rip});
            } else if (changed & ZYDIS_CPUFLAG_CF) {
                flag_access_cpazso[0].push_back({true, rip});
            }

            if (used & ZYDIS_CPUFLAG_PF || flag_passthrough(instruction.mnemonic, X86_REF_PF)) {
                flag_access_cpazso[1].push_back({false, rip});
            } else if (changed & ZYDIS_CPUFLAG_PF) {
                flag_access_cpazso[1].push_back({true, rip});
            }

            if (used & ZYDIS_CPUFLAG_AF || flag_passthrough(instruction.mnemonic, X86_REF_AF)) {
                flag_access_cpazso[2].push_back({false, rip});
            } else if (changed & ZYDIS_CPUFLAG_AF) {
                flag_access_cpazso[2].push_back({true, rip});
            }

            if (used & ZYDIS_CPUFLAG_ZF || flag_passthrough(instruction.mnemonic, X86_REF_ZF)) {
                flag_access_cpazso[3].push_back({false, rip});
            } else if (changed & ZYDIS_CPUFLAG_ZF) {
                flag_access_cpazso[3].push_back({true, rip});
            }

            if (used & ZYDIS_CPUFLAG_SF || flag_passthrough(instruction.mnemonic, X86_REF_SF)) {
                flag_access_cpazso[4].push_back({false, rip});
            } else if (changed & ZYDIS_CPUFLAG_SF) {
                flag_access_cpazso[4].push_back({true, rip});
            }

            if (used & ZYDIS_CPUFLAG_OF || flag_passthrough(instruction.mnemonic, X86_REF_OF)) {
                flag_access_cpazso[5].push_back({false, rip});
            } else if (changed & ZYDIS_CPUFLAG_OF) {
                flag_access_cpazso[5].push_back({true, rip});
            }
        }

        if (is_jump || is_ret || is_call || is_illegal || is_hlt) {
            break;
        }

        rip += instruction.length;
    }
}

bool Recompiler::shouldEmitFlag(HostAddress rip, x86_ref_e ref) {
    if (g_paranoid) {
        return true;
    }

    int index = 0;
    switch (ref) {
    case X86_REF_CF: {
        index = 0;
        break;
    case X86_REF_PF:
        index = 1;
        break;
    case X86_REF_AF:
        index = 2;
        break;
    case X86_REF_ZF:
        index = 3;
        break;
    case X86_REF_SF:
        index = 4;
        break;
    case X86_REF_OF:
        index = 5;
        break;
    default:
        UNREACHABLE();
        break;
    }
    }

    for (auto& [changed, r] : flag_access_cpazso[index]) {
        if (r > rip && !changed) {
            return true;
        }

        if (r > rip && changed) {
            return false;
        }
    }

    return true;
}

// (res & (~d | s)) | (~d & s), xor top 2 bits
void Recompiler::updateOverflowSub(biscuit::GPR lhs, biscuit::GPR rhs, biscuit::GPR result, x86_size_e size_e) {
    int size = getBitSize(size_e);
    biscuit::GPR of = flagW(X86_REF_OF);
    biscuit::GPR temp = scratch();
    as.NOT(temp, lhs);
    as.OR(of, temp, rhs);
    as.AND(of, of, result);
    as.AND(temp, temp, rhs);
    as.OR(of, of, temp);
    as.SRLI(temp, of, size - 2);
    as.SRLI(of, of, size - 1);
    as.XOR(of, of, temp);
    as.ANDI(of, of, 1);
    popScratch();
}

// ((s & d) | ((~res) & (s | d))), xor top 2 bits
void Recompiler::updateOverflowAdd(biscuit::GPR lhs, biscuit::GPR rhs, biscuit::GPR result, x86_size_e size_e) {
    int size = getBitSize(size_e);
    biscuit::GPR of = flagW(X86_REF_OF);
    biscuit::GPR temp = scratch();
    as.OR(of, lhs, rhs);
    as.NOT(temp, result);
    as.AND(of, temp, of);
    as.AND(temp, lhs, rhs);
    as.OR(of, of, temp);
    as.SRLI(temp, of, size - 2);
    as.SRLI(of, of, size - 1);
    as.XOR(of, of, temp);
    as.ANDI(of, of, 1);
    popScratch();
}

void Recompiler::updateAuxiliaryAdd(biscuit::GPR lhs, biscuit::GPR result) {
    biscuit::GPR af = flagW(X86_REF_AF);
    biscuit::GPR temp = scratch();
    as.ANDI(af, result, 0xF);
    as.ANDI(temp, lhs, 0xF);
    as.SLTU(af, af, temp);
    popScratch();
}

void Recompiler::updateAuxiliarySub(biscuit::GPR lhs, biscuit::GPR rhs) {
    biscuit::GPR af = flagW(X86_REF_AF);
    biscuit::GPR temp = scratch();
    as.ANDI(af, rhs, 0xF);
    as.ANDI(temp, lhs, 0xF);
    as.SLTU(af, temp, af);
    popScratch();
}

void Recompiler::updateAuxiliaryAdc(biscuit::GPR lhs, biscuit::GPR result, biscuit::GPR cf, biscuit::GPR result_2) {
    biscuit::GPR af = flagW(X86_REF_AF);
    biscuit::GPR temp = scratch();
    as.ANDI(af, result, 0xF);
    as.ANDI(temp, lhs, 0xF);
    as.SLTU(af, af, temp);
    as.ANDI(temp, result_2, 0xF);
    as.SLTU(temp, temp, cf);
    as.OR(af, af, temp);
    popScratch();
}

void Recompiler::updateAuxiliarySbb(biscuit::GPR lhs, biscuit::GPR rhs, biscuit::GPR result, biscuit::GPR cf) {
    biscuit::GPR af = flagW(X86_REF_AF);
    biscuit::GPR temp = scratch();
    as.ANDI(af, rhs, 0xF);
    as.ANDI(temp, lhs, 0xF);
    as.SLTU(af, temp, af);
    as.ANDI(temp, result, 0xF);
    as.SLTU(temp, temp, cf);
    as.OR(af, af, temp);
    popScratch();
}

void Recompiler::updateCarryAdd(biscuit::GPR lhs, biscuit::GPR result, x86_size_e size) {
    biscuit::GPR cf = flagW(X86_REF_CF);
    zext(cf, result, size);
    as.SLTU(cf, cf, lhs);
}

void Recompiler::updateCarrySub(biscuit::GPR lhs, biscuit::GPR rhs) {
    biscuit::GPR cf = flagW(X86_REF_CF);
    as.SLTU(cf, lhs, rhs);
}

void Recompiler::updateCarryAdc(biscuit::GPR lhs, biscuit::GPR result, biscuit::GPR result_2, x86_size_e size) {
    biscuit::GPR temp = scratch();
    biscuit::GPR temp2 = scratch();
    biscuit::GPR cf = flagWR(X86_REF_CF);
    zext(temp, result, size);
    zext(temp2, result_2, size);
    as.SLTU(temp, temp, lhs);
    as.SLTU(temp2, temp2, cf);
    as.OR(cf, temp, temp2);
    popScratch();
    popScratch();
}

void Recompiler::zeroFlag(x86_ref_e flag) {
    biscuit::GPR f = flagW(flag);
    as.LI(f, 0);
}

void Recompiler::setFlag(x86_ref_e flag) {
    biscuit::GPR f = flagW(flag);
    as.LI(f, 1);
}

void Recompiler::zext(biscuit::GPR dest, biscuit::GPR src, x86_size_e size) {
    switch (size) {
    case X86_SIZE_BYTE:
    case X86_SIZE_BYTE_HIGH: {
        as.ANDI(dest, src, 0xff);
        break;
    }
    case X86_SIZE_WORD: {
        if (Extensions::B) {
            as.ZEXTH(dest, src);
        } else {
            as.SLLI(dest, src, 48);
            as.SRLI(dest, dest, 48);
        }
        break;
    }
    case X86_SIZE_DWORD: {
        if (Extensions::B) {
            as.ZEXTW(dest, src);
        } else {
            as.SLLI(dest, src, 32);
            as.SRLI(dest, dest, 32);
        }
        break;
    }
    case X86_SIZE_QWORD: {
        if (dest != src)
            as.MV(dest, src);
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }
}

int Recompiler::getBitSize(x86_size_e size) {
    switch (size) {
    case X86_SIZE_BYTE:
    case X86_SIZE_BYTE_HIGH:
        return 8;
    case X86_SIZE_WORD:
        return 16;
    case X86_SIZE_DWORD:
        return 32;
    case X86_SIZE_QWORD:
        return 64;
    case X86_SIZE_XMM:
        return 128;
    default:
        UNREACHABLE();
        return 0;
    }
}

u64 Recompiler::getSignMask(x86_size_e size_e) {
    u16 size = getBitSize(size_e);
    return 1ull << (size - 1);
}

void Recompiler::updateParity(biscuit::GPR result) {
    if (Extensions::B) {
        biscuit::GPR pf = scratch();
        as.ANDI(pf, result, 0xFF);
        as.CPOPW(pf, pf);
        as.ANDI(pf, pf, 1);
        as.XORI(pf, pf, 1);
        as.SB(pf, offsetof(ThreadState, pf), threadStatePointer());
        popScratch();
    } else {
        static bool bitcount[] = {
            1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0,
            1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1,
            1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0,
            1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0,
            1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0,
            1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
        };
        static_assert(sizeof(bitcount) == 256, "Invalid bitcount table size");

        bool* addy = bitcount;
        Literal address((u64)addy);
        Label end;
        biscuit::GPR pf = scratch();

        // We need another scratch but on many instructions there isn't one available at this point :(
        ASSERT(pf != t0);
        as.LD(pf, &address);
        as.ADDI(sp, sp, -8);
        as.SD(t0, 0, sp);
        as.ANDI(t0, result, 0xFF);
        as.ADD(pf, pf, t0);
        as.LD(t0, 0, sp);
        as.ADDI(sp, sp, 8);
        as.LBU(pf, 0, pf);
        as.SB(pf, offsetof(ThreadState, pf), threadStatePointer());
        as.J(&end);
        as.Place(&address);
        as.Bind(&end);
        popScratch();
    }
}

void Recompiler::updateZero(biscuit::GPR result, x86_size_e size) {
    biscuit::GPR zf = flagW(X86_REF_ZF);
    zext(zf, result, size);
    as.SEQZ(zf, zf);
}

void Recompiler::updateSign(biscuit::GPR result, x86_size_e size) {
    biscuit::GPR sf = flagW(X86_REF_SF);
    as.SRLI(sf, result, getBitSize(size) - 1);
    as.ANDI(sf, sf, 1);
}

void Recompiler::setRip(biscuit::GPR rip) {
    as.SD(rip, offsetof(ThreadState, rip), threadStatePointer());
}

biscuit::GPR Recompiler::getRip() {
    biscuit::GPR rip = scratch();
    as.LD(rip, offsetof(ThreadState, rip), threadStatePointer());
    return rip;
}

void Recompiler::jumpAndLink(HostAddress rip, bool use_rsb) {
    if (g_dont_link) {
        // Just emit jump to dispatcher
        backToDispatcher(use_rsb);
        return;
    }

    u8* start = as.GetCursorPointer();
    if (!blockExists(rip)) {
        u8* link_me = as.GetCursorPointer();
        backToDispatcher(use_rsb);

        getBlockMetadata(rip).pending_links.push_back(link_me);
    } else {
        auto& target_meta = getBlockMetadata(rip);
        u64 target = target_meta.address.raw();

        u8* link_me = as.GetCursorPointer();
        target_meta.links.push_back(link_me); // for when we need to unlink

        u64 offset = target - (u64)(as.GetCursorPointer() + 4);
        if (IsValidJTypeImm(offset)) {
            if (offset != 4) {
                as.NOP();
                if (use_rsb) {
                    as.JAL(ra, offset);
                } else {
                    as.J(offset);
                }
            } else {
                // Can just be inlined as target is just ahead
                // Replace the AUIPC+JR with 2 NOPs
                as.NOP();
                as.NOP();
            }
        } else {
            // Too far for a regular jump, use AUIPC+JR
            ASSERT(IsValid2GBImm(offset));
            u64 offset = target - (u64)as.GetCursorPointer();
            const auto hi20 = static_cast<int32_t>(((static_cast<uint32_t>(offset) + 0x800) >> 12) & 0xFFFFF);
            const auto lo12 = static_cast<int32_t>(offset << 20) >> 20;

            as.AUIPC(t0, hi20);
            if (use_rsb) {
                as.JALR(ra, lo12, t0); // hint to the rsb to push
            } else {
                as.JR(t0, lo12);
            }
        }
    }

    // These jumps are always 2 instructions to keep consistent when backpatching is needed
    ASSERT(as.GetCursorPointer() - start == 2 * 4);
}

void Recompiler::jumpAndLinkConditional(biscuit::GPR condition, biscuit::GPR gpr_true, biscuit::GPR gpr_false, HostAddress rip_true,
                                        HostAddress rip_false) {
    Label false_label;
    as.BEQZ(condition, &false_label);

    setRip(gpr_true);
    jumpAndLink(rip_true);

    as.Bind(&false_label);

    setRip(gpr_false);
    jumpAndLink(rip_false);
}

void Recompiler::expirePendingLinks(HostAddress rip) {
    if (g_dont_link) {
        return;
    }

    if (!blockExists(rip)) {
        return;
    }

    auto& block_meta = getBlockMetadata(rip);
    auto& pending_links = block_meta.pending_links;
    for (u8* link : pending_links) {
        bool use_rsb = false;
        u32 jump_inst = *(u32*)(link + 4);
        // If it uses `ra`, we need to emit an rsb hinting jump
        // `jalr t0` is emitted from backToDispatcher when needing RSB (ie from calls to reg)
        static biscuit::Decoder decoder;
        DecodedInstruction instruction;
        DecodedOperand operands[4];
        DecoderStatus status = decoder.Decode(&jump_inst, 4, instruction, operands);
        if (status != DecoderStatus::Ok) {
            WARN("Couldn't decode instruction during expirePendingLinks");
        }

        if (instruction.mnemonic == Mnemonic::JALR && operands[0].GPR() == ra) {
            ASSERT(g_rsb);
            use_rsb = true;
        }

        u8* cursor = as.GetCursorPointer();
        as.SetCursorPointer(link);
        jumpAndLink(rip, use_rsb);
        as.SetCursorPointer(cursor);
    }

    flush_icache();

    block_meta.links.insert(block_meta.links.end(), pending_links.begin(), pending_links.end());
    block_meta.pending_links.clear();
}

u64 Recompiler::sextImmediate(u64 imm, ZyanU8 size) {
    switch (size) {
    case 8: {
        return (i64)(i8)imm;
    }
    case 16: {
        return (i64)(i16)imm;
    }
    case 32: {
        return (i64)(i32)imm;
    }
    case 64: {
        return imm;
    }
    default: {
        UNREACHABLE();
        return 0;
    }
    }
}

void Recompiler::addi(biscuit::GPR dst, biscuit::GPR src, u64 imm) {
    if (imm == 0 && dst == src) {
        return;
    }

    if ((i64)imm >= -2048 && (i64)imm < 2048) {
        as.ADDI(dst, src, imm);
    } else {
        biscuit::GPR reg = scratch();
        as.LI(reg, imm);
        as.ADD(dst, src, reg);
        popScratch();
    }
}

void Recompiler::setFlagUndefined(x86_ref_e ref) {
    // Once a flag has been set to undefined state it doesn't need to be written back
    // it's as if it was written with a random value, which we don't care to emulate
    RegisterMetadata& meta = getMetadata(ref);
    meta.loaded = false;
    meta.dirty = false;
}

void Recompiler::sextb(biscuit::GPR dest, biscuit::GPR src) {
    if (Extensions::B) {
        as.SEXTB(dest, src);
    } else {
        as.SLLI(dest, src, 56);
        as.SRAI(dest, dest, 56);
    }
}

void Recompiler::sexth(biscuit::GPR dest, biscuit::GPR src) {
    if (Extensions::B) {
        as.SEXTH(dest, src);
    } else {
        as.SLLI(dest, src, 48);
        as.SRAI(dest, dest, 48);
    }
}

biscuit::GPR Recompiler::getCond(int cond) {
    switch (cond & 0xF) {
    case 0:
        return flag(X86_REF_OF);
    case 1: {
        biscuit::GPR of = scratch();
        as.XORI(of, flag(X86_REF_OF), 1);
        return of;
    }
    case 2:
        return flag(X86_REF_CF);
    case 3: {
        biscuit::GPR cf = scratch();
        as.XORI(cf, flag(X86_REF_CF), 1);
        return cf;
    }
    case 4:
        return flag(X86_REF_ZF);
    case 5: {
        biscuit::GPR zf = scratch();
        as.XORI(zf, flag(X86_REF_ZF), 1);
        return zf;
    }
    case 6: {
        biscuit::GPR cond = scratch();
        as.OR(cond, flag(X86_REF_CF), flag(X86_REF_ZF));
        return cond;
    }
    case 7: {
        biscuit::GPR cond = scratch();
        as.OR(cond, flag(X86_REF_CF), flag(X86_REF_ZF));
        as.XORI(cond, cond, 1);
        return cond;
    }
    case 8:
        return flag(X86_REF_SF);
    case 9: {
        biscuit::GPR sf = scratch();
        as.XORI(sf, flag(X86_REF_SF), 1);
        return sf;
    }
    case 10:
        return flag(X86_REF_PF);
    case 11: {
        biscuit::GPR pf = flag(X86_REF_PF);
        as.XORI(pf, pf, 1);
        return pf;
    }
    case 12: {
        biscuit::GPR cond = scratch();
        as.XOR(cond, flag(X86_REF_SF), flag(X86_REF_OF));
        return cond;
    }
    case 13: {
        biscuit::GPR cond = scratch();
        as.XOR(cond, flag(X86_REF_SF), flag(X86_REF_OF));
        as.XORI(cond, cond, 1);
        return cond;
    }
    case 14: {
        biscuit::GPR cond = scratch();
        as.XOR(cond, flag(X86_REF_SF), flag(X86_REF_OF));
        as.OR(cond, cond, flag(X86_REF_ZF));
        return cond;
    }
    case 15: {
        biscuit::GPR cond = scratch();
        as.XOR(cond, flag(X86_REF_SF), flag(X86_REF_OF));
        as.OR(cond, cond, flag(X86_REF_ZF));
        as.XORI(cond, cond, 1);
        return cond;
    }
    }

    UNREACHABLE();
    return x0;
}

void Recompiler::readMemory(biscuit::GPR dest, biscuit::GPR address, i64 offset, x86_size_e size) {
    if (g_address_space_base) {
        biscuit::GPR temp = scratch();
        as.LI(temp, g_address_space_base);
        as.ADD(temp, temp, address);
        address = temp;
        popScratch();
    }

    readMemoryNoBase(dest, address, offset, size);
}

void Recompiler::readMemoryNoBase(biscuit::GPR dest, biscuit::GPR address, i64 offset, x86_size_e size) {
    switch (size) {
    case X86_SIZE_BYTE: {
        as.LBU(dest, offset, address);
        break;
    }
    case X86_SIZE_WORD: {
        as.LHU(dest, offset, address);
        break;
    }
    case X86_SIZE_DWORD: {
        as.LWU(dest, offset, address);
        break;
    }
    case X86_SIZE_QWORD: {
        as.LD(dest, offset, address);
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }

    if (g_always_tso && !Extensions::TSO) {
        as.FENCE(FenceOrder::R, FenceOrder::RW);
    }
}

void Recompiler::writeMemory(biscuit::GPR src, biscuit::GPR address, i64 offset, x86_size_e size) {
    if (g_address_space_base) {
        biscuit::GPR temp = scratch();
        as.LI(temp, g_address_space_base);
        as.ADD(temp, temp, address);
        address = temp;
        popScratch();
    }

    writeMemoryNoBase(src, address, offset, size);
}

void Recompiler::writeMemoryNoBase(biscuit::GPR src, biscuit::GPR address, i64 offset, x86_size_e size) {
    if (g_always_tso && !Extensions::TSO) {
        as.FENCE(FenceOrder::RW, FenceOrder::W);
    }

    switch (size) {
    case X86_SIZE_BYTE: {
        as.SB(src, offset, address);
        break;
    }
    case X86_SIZE_WORD: {
        as.SH(src, offset, address);
        break;
    }
    case X86_SIZE_DWORD: {
        as.SW(src, offset, address);
        break;
    }
    case X86_SIZE_QWORD: {
        as.SD(src, offset, address);
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }
}

x86_size_e Recompiler::zydisToSize(ZyanU8 size) {
    switch (size) {
    case 8:
        return X86_SIZE_BYTE;
    case 16:
        return X86_SIZE_WORD;
    case 32:
        return X86_SIZE_DWORD;
    case 64:
        return X86_SIZE_QWORD;
    case 80:
        return X86_SIZE_ST;
    case 128:
        return X86_SIZE_XMM;
    default:
        UNREACHABLE();
        return X86_SIZE_BYTE;
    }
}

void Recompiler::repPrologue(Label* loop_end, biscuit::GPR rcx) {
    // Signal handling would get tricky if we had to account for this looping mess of an instruction
    disableSignals();
    as.BEQZ(rcx, loop_end);
}

void Recompiler::repEpilogue(Label* loop_body, biscuit::GPR rcx) {
    as.ADDI(rcx, rcx, -1);
    as.BNEZ(rcx, loop_body);
    enableSignals();
}

void Recompiler::repzEpilogue(Label* loop_body, Label* loop_end, biscuit::GPR rcx, bool is_repz) {
    as.ADDI(rcx, rcx, -1);
    as.BEQZ(rcx, loop_end);

    if (is_repz) {
        biscuit::GPR zf = flag(X86_REF_ZF);
        as.BNEZ(zf, loop_body);
    } else {
        biscuit::GPR zf = flag(X86_REF_ZF);
        as.BEQZ(zf, loop_body);
    }
    enableSignals();
}

void Recompiler::sext(biscuit::GPR dst, biscuit::GPR src, x86_size_e size) {
    switch (size) {
    case X86_SIZE_BYTE: {
        sextb(dst, src);
        break;
    }
    case X86_SIZE_WORD: {
        sexth(dst, src);
        break;
    }
    case X86_SIZE_DWORD: {
        as.ADDIW(dst, src, 0);
        break;
    }
    case X86_SIZE_QWORD: {
        as.MV(dst, src);
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }
}

bool Recompiler::blockExists(HostAddress rip) {
    return !getBlockMetadata(rip).address.isNull();
}

void Recompiler::vrgather(biscuit::Vec dst, biscuit::Vec src, biscuit::Vec iota, VecMask mask) {
    if (dst == src || dst == iota) {
        biscuit::Vec temp = scratchVec();
        as.VRGATHER(temp, src, iota, mask);
        as.VMV(dst, temp);
        popScratch();
    } else {
        as.VRGATHER(dst, src, iota, mask);
    }
}

biscuit::GPR Recompiler::getFlags() {
    biscuit::GPR reg = scratch();
    biscuit::GPR temp = scratch();
    biscuit::GPR cf = flag(X86_REF_CF);
    biscuit::GPR pf = flag(X86_REF_PF);
    biscuit::GPR af = flag(X86_REF_AF);
    biscuit::GPR zf = flag(X86_REF_ZF);
    biscuit::GPR sf = flag(X86_REF_SF);
    biscuit::GPR of = flag(X86_REF_OF);
    as.LBU(temp, offsetof(ThreadState, df), threadStatePointer());
    as.SLLI(temp, temp, 10);
    as.SLLI(reg, of, 11);
    as.OR(reg, reg, temp);
    as.SLLI(temp, sf, 7);
    as.OR(reg, reg, temp);
    as.SLLI(temp, zf, 6);
    as.OR(reg, reg, temp);
    as.SLLI(temp, af, 4);
    as.OR(reg, reg, temp);
    as.SLLI(temp, pf, 2);
    as.OR(reg, reg, temp);
    as.OR(reg, reg, cf);
    as.ORI(reg, reg, 0b10);  // bit 1 always set in flags
    as.ORI(reg, reg, 0x200); // IE bit
    as.LB(temp, offsetof(ThreadState, cpuid_bit), threadStatePointer());
    as.SLLI(temp, temp, 21);
    as.OR(reg, reg, temp);
    popScratch();
    return reg;
}

void Recompiler::disableSignals() {
    biscuit::GPR i_love_risc_architecture = scratch();
    as.LI(i_love_risc_architecture, 1);
    as.SB(i_love_risc_architecture, offsetof(ThreadState, signals_disabled), threadStatePointer());
    popScratch();
}

void Recompiler::enableSignals() {
    as.SB(x0, offsetof(ThreadState, signals_disabled), threadStatePointer());
}

biscuit::GPR Recompiler::getTOP() { // TODO: allocate a reg for this maybe
    biscuit::GPR top = scratch();
    as.LB(top, offsetof(ThreadState, fpu_top), threadStatePointer());
    return top;
}

biscuit::FPR Recompiler::getST(biscuit::GPR top, int index) {
    biscuit::FPR st = scratchFPR();
    biscuit::GPR address = scratch();
    if (index != 0) {
        as.ADDI(address, top, index);
        as.ANDI(address, address, 0b111);
        as.SLLI(address, address, 3);
    } else {
        as.SLLI(address, top, 3);
    }
    as.ADD(address, address, threadStatePointer());
    as.FLD(st, offsetof(ThreadState, fp), address);
    popScratch();
    return st;
}

biscuit::FPR Recompiler::getST(biscuit::GPR top, ZydisDecodedOperand* operand) {
    if (operand->type == ZYDIS_OPERAND_TYPE_REGISTER) {
        ASSERT(operand->reg.value >= ZYDIS_REGISTER_ST0 && operand->reg.value <= ZYDIS_REGISTER_ST7);
        return getST(top, operand->reg.value - ZYDIS_REGISTER_ST0);
    } else if (operand->type == ZYDIS_OPERAND_TYPE_MEMORY) {
        switch (operand->size) {
        case 32: {
            biscuit::FPR st = scratchFPR();
            as.FLW(st, 0, leaAddBase(operand));
            as.FCVT_D_S(st, st);
            popScratch(); // the gpr address scratch
            return st;
        }
        case 64: {
            biscuit::FPR st = scratchFPR();
            as.FLD(st, 0, leaAddBase(operand));
            popScratch(); // the gpr address scratch
            return st;
        }
        case 80: {
            UNREACHABLE();
            return ft0;
        }
        default: {
            UNREACHABLE();
            return f0;
        }
        }
    } else {
        UNREACHABLE();
        return f0;
    }
}

void Recompiler::pushST(biscuit::GPR top, biscuit::FPR st) {
    biscuit::GPR address = scratch();
    as.ADDI(address, top, -1);
    as.ANDI(address, address, 0b111);
    setTOP(address);
    as.ADD(address, address, threadStatePointer());
    as.FSD(st, offsetof(ThreadState, fp), address);
}

void Recompiler::popST(biscuit::GPR top) {
    biscuit::GPR address = scratch();
    as.ADDI(address, top, 1);
    as.ANDI(address, address, 0b111);
    setTOP(address);
}

void Recompiler::setST(biscuit::GPR top, int index, biscuit::FPR st) {
    biscuit::GPR address = scratch();
    if (index != 0) {
        as.ADDI(address, top, index);
        as.ANDI(address, address, 0b111);
        as.SLLI(address, address, 3);
    } else {
        as.SLLI(address, top, 3);
    }
    as.ADD(address, address, threadStatePointer());
    as.FSD(st, offsetof(ThreadState, fp), address);
    popScratch();
}

void Recompiler::setST(biscuit::GPR top, ZydisDecodedOperand* operand, biscuit::FPR value) {
    if (operand->type == ZYDIS_OPERAND_TYPE_REGISTER) {
        ASSERT(operand->reg.value >= ZYDIS_REGISTER_ST0 && operand->reg.value <= ZYDIS_REGISTER_ST7);
        return setST(top, operand->reg.value - ZYDIS_REGISTER_ST0, value);
    } else if (operand->type == ZYDIS_OPERAND_TYPE_MEMORY) {
        switch (operand->size) {
        case 32: {
            biscuit::FPR temp = scratchFPR();
            as.FCVT_S_D(temp, value);
            as.FSW(temp, 0, leaAddBase(operand));
            popScratch(); // the gpr address scratch
            break;
        }
        case 64: {
            as.FSD(value, 0, leaAddBase(operand));
            popScratch(); // the gpr address scratch
            break;
        }
        }
    } else {
        UNREACHABLE();
    }
}

void Recompiler::setTOP(biscuit::GPR new_top) {
    as.SB(new_top, offsetof(ThreadState, fpu_top), threadStatePointer());
}

void Recompiler::unlinkBlock(ThreadState* state, HostAddress rip) {
    auto metadata = state->recompiler->getBlockMetadata(rip);

    if (metadata.address_end.isNull()) {
        // Not yet compiled, we are fine
        return;
    }

    u8* rewind_address = (u8*)metadata.address_end.raw() - 4 * 3; // 3 instructions for the ending jump/link
    unlinkAt(rewind_address);
    flush_icache();
}

void Recompiler::invalidateBlock(BlockMetadata* block) {
    // This code assumes you've locked the map mutex
    // Unlink everywhere this block was linked
    for (u8* link : block->links) {
        unlinkAt(link);
    }

    // Unlink ourselves, jump back to dispatcher at end
    u8* rewind_address = (u8*)block->address_end.raw() - 4 * 3; // 3 instructions for the ending jump/link
    unlinkAt(rewind_address);

    // Remove the block from the map
    bool was_present = block_metadata.erase(block->guest_address.raw());
    ASSERT(was_present);
    flush_icache();
}

void Recompiler::unlinkAt(u8* address_of_jump) {
    u8* current_address = as.GetCursorPointer();

    // Replace whatever was there with a jump back to dispatcher
    as.SetCursorPointer(address_of_jump);
    backToDispatcher();
    as.SetCursorPointer(current_address);
}

bool Recompiler::tryInlineSyscall() {
    switch (rax_value) {
#define CASE(sysno, argcount)                                                                                                                        \
    case sysno:                                                                                                                                      \
        inlineSyscall(match_host(sysno), argcount);                                                                                                  \
        return true

        CASE(felix86_x86_64_read, 3);
        CASE(felix86_x86_64_write, 3);
        CASE(felix86_x86_64_mprotect, 3);
        CASE(felix86_x86_64_munmap, 2);
        CASE(felix86_x86_64_ioctl, 3);
        CASE(felix86_x86_64_pread64, 4);
        CASE(felix86_x86_64_pwrite64, 4);
        CASE(felix86_x86_64_ppoll, 5);
        CASE(felix86_x86_64_readv, 3);
        CASE(felix86_x86_64_writev, 3);
        CASE(felix86_x86_64_clock_gettime, 2);
        CASE(felix86_x86_64_gettimeofday, 2);
        CASE(felix86_x86_64_futex, 6);

#undef CASE
    default: {
        return false;
    }
    }
}

void Recompiler::inlineSyscall(int sysno, int argcount) {
    // Check if they were loaded before writing them to state, so we don't load them again
    bool a0_was_loaded = getMetadata(X86_REF_RDI).loaded;
    bool a1_was_loaded = getMetadata(X86_REF_RSI).loaded;
    bool a2_was_loaded = getMetadata(X86_REF_RDX).loaded;
    bool a3_was_loaded = getMetadata(X86_REF_R10).loaded;
    bool a4_was_loaded = getMetadata(X86_REF_R8).loaded;
    bool a5_was_loaded = getMetadata(X86_REF_R9).loaded;
    static_assert(allocatedGPR(X86_REF_RDI) == a0);
    static_assert(allocatedGPR(X86_REF_RSI) == a1);
    static_assert(allocatedGPR(X86_REF_RDX) == a2);
    static_assert(allocatedGPR(X86_REF_R10) == a3);
    static_assert(allocatedGPR(X86_REF_R8) == a4);
    static_assert(allocatedGPR(X86_REF_R9) == a5);

    writebackDirtyState(); // I don't think we can count on the kernel not clobbering our regs

    as.LI(a7, sysno);

    if (!a0_was_loaded && argcount > 0) {
        as.LD(a0, offsetof(ThreadState, gprs) + (X86_REF_RDI - X86_REF_RAX) * sizeof(u64), threadStatePointer());
    }

    if (!a1_was_loaded && argcount > 1) {
        as.LD(a1, offsetof(ThreadState, gprs) + (X86_REF_RSI - X86_REF_RAX) * sizeof(u64), threadStatePointer());
    }

    if (!a2_was_loaded && argcount > 2) {
        as.LD(a2, offsetof(ThreadState, gprs) + (X86_REF_RDX - X86_REF_RAX) * sizeof(u64), threadStatePointer());
    }

    if (!a3_was_loaded && argcount > 3) {
        as.LD(a3, offsetof(ThreadState, gprs) + (X86_REF_R10 - X86_REF_RAX) * sizeof(u64), threadStatePointer());
    }

    if (!a4_was_loaded && argcount > 4) {
        as.LD(a4, offsetof(ThreadState, gprs) + (X86_REF_R8 - X86_REF_RAX) * sizeof(u64), threadStatePointer());
    }

    if (!a5_was_loaded && argcount > 5) {
        as.LD(a5, offsetof(ThreadState, gprs) + (X86_REF_R9 - X86_REF_RAX) * sizeof(u64), threadStatePointer());
    }

    as.ECALL();

    setRefGPR(X86_REF_RAX, X86_SIZE_QWORD, a0);
}

void Recompiler::checkModifiesRax(ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands) {
    u8 opcount = instruction.operand_count;

    if (instruction.mnemonic == ZYDIS_MNEMONIC_MOV) {
        bool is_rax = operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER &&
                      (operands[0].reg.value == ZYDIS_REGISTER_RAX || operands[0].reg.value == ZYDIS_REGISTER_EAX);
        bool is_imm = operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE;
        if (is_rax && is_imm) {
            // We don't care to zero extend this in the EAX case as any negative number wouldn't be a valid syscall anyway
            // Compilers won't emit a write to ax/al/ah because that would keep the upper bits
            rax_value = operands[1].imm.value.s;
            return;
        }
    }

    // If any of the operands modifies RAX/EAX/AX/AL/AH we discard its old value by setting it to -1, which will not
    // inline to any syscall
    for (int i = 0; i < opcount; i++) {
        if (operands[i].type == ZYDIS_OPERAND_TYPE_REGISTER) {
            bool is_rax = false;
            switch (operands[i].reg.value) {
            case ZYDIS_REGISTER_AL:
            case ZYDIS_REGISTER_AH:
            case ZYDIS_REGISTER_AX:
            case ZYDIS_REGISTER_EAX:
            case ZYDIS_REGISTER_RAX:
                is_rax = true;
                break;
            default: {
                break;
            }
            }
            bool modified = operands[i].actions & ZYDIS_OPERAND_ACTION_MASK_WRITE;
            if (is_rax && modified) {
                rax_value = -1;
                return;
            }
        }
    }
}

void Recompiler::trace(u64 address) {
    block_trace[block_trace_index] = address;
    block_trace_index++;
    block_trace_index %= block_trace.size();
}

void Recompiler::printTrace() {
    for (size_t i = 0; i < block_trace.size(); i++) {
        int j = (block_trace_index + i) % block_trace.size();
        u64 address = block_trace[j];
        printf("#%zu ", i);
        print_address(address);
    }
}

void Recompiler::linkIndirect() {
    if (g_rsb) {
        as.SD(sp, offsetof(ThreadState, current_sp), threadStatePointer());
    }

    // Self modifying piece of code that rewrites itself as a check + link, where
    // if the check fails it unlinks itself and always jumps to dispatcher
    // We assume that we can use every register as they have been written back at this point.
    Label back_here;
    as.Bind(&back_here);

    u8* start = as.GetCursorPointer();
    Literal link_address((u64)start);
    Literal compile_next((u64)Emulator::CompileNext);
    Literal link_indirect((u64)Emulator::LinkIndirect);

    // Get host address for block we wanna link to, get the guest address that should match when we jump there.
    // AUIPC + LD + MV + JALR + LD + AUIPC + LD + MV + AUIPC + LD + JALR = 11 instructions we can replace at most
    as.LD(t0, &compile_next);
    as.MV(a0, threadStatePointer());
    as.JALR(t0);
    // At this point, a0 has the host address, load a1 with the expected guest address
    as.LD(a1, offsetof(ThreadState, rip), threadStatePointer());
    // Put link address in a2
    as.LD(a2, &link_address);
    as.MV(a3, threadStatePointer());
    as.LD(t0, &link_indirect);
    as.JALR(t0); // (guest address, host address, link address, thread state)

    // Emulator::LinkIndirect depends on the above sequence being 11 instructions
    u8* here = as.GetCursorPointer();
    ASSERT(here - start == 11 * 4);

    as.J(&back_here);
    as.Place(&compile_next);
    as.Place(&link_indirect);
    as.Place(&link_address);
}