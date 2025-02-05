#include <sys/mman.h>
#include <unistd.h>
#include "Zydis/Disassembler.h"
#include "felix86/emulator.hpp"
#include "felix86/v2/recompiler.hpp"

#define X(name) void fast_##name(Recompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands);
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

    emitDispatcher();
    emitSigreturnThunk();

    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
    ZydisDecoderEnableMode(&decoder, ZYDIS_DECODER_MODE_AMD_BRANCHES, ZYAN_TRUE);
}

Recompiler::~Recompiler() {
    deallocateCodeCache(code_cache);
}

void Recompiler::emitDispatcher() {
    enter_dispatcher = (decltype(enter_dispatcher))as.GetCursorPointer();

    // Give it an initial valid state
    as.VSETIVLI(x0, SUPPORTED_VLEN / 8, SEW::E8);

    // Save the current register state of callee-saved registers and return address
    static_assert(sizeof(ThreadState::saved_host_gprs) == saved_gprs.size() * 8);
    as.ADDI(t0, a0, offsetof(ThreadState, saved_host_gprs));
    for (size_t i = 0; i < saved_gprs.size(); i++) {
        as.SD(saved_gprs[i], i * sizeof(u64), t0);
    }

    as.MV(threadStatePointer(), a0);

    compile_next_handler = as.GetCursorPointer();

    Label exit_dispatcher_label;

    // If it's not zero it has some exit reason, exit the dispatcher
    as.MV(a0, threadStatePointer());
    as.LBU(t0, offsetof(ThreadState, exit_reason), threadStatePointer());
    as.BNEZ(t0, &exit_dispatcher_label);
    as.LI(t0, (u64)Emulator::CompileNext);
    as.JALR(t0); // returns the function pointer to the compiled function
    as.JR(a0);   // jump to the compiled function

    as.Bind(&exit_dispatcher_label);

    exit_dispatcher = (decltype(exit_dispatcher))as.GetCursorPointer();

    as.ADDI(t0, a0, offsetof(ThreadState, saved_host_gprs));
    for (size_t i = 0; i < saved_gprs.size(); i++) {
        as.LD(saved_gprs[i], i * sizeof(u64), t0);
    }

    as.JR(ra);

    flush_icache((void*)enter_dispatcher, as.GetCursorPointer());
}

void* Recompiler::emitSigreturnThunk() {
    // This piece of code is responsible for moving the thread state pointer to the right place (so we don't have to find it using tid)
    // calling sigreturn, returning and going back to the dispatcher.
    void* here = as.GetCursorPointer();
    block_metadata[Signals::magicSigreturnAddress()].address = here;

    as.MV(a0, threadStatePointer());
    as.LI(t0, (u64)Signals::sigreturn);
    as.JALR(t0);
    backToDispatcher();

    return here;
}

void* Recompiler::compile(u64 rip) {
    size_t remaining_size = code_cache_size - (as.GetCursorPointer() - as.GetCodeBuffer().GetOffsetPointer(0));
    if (remaining_size < 100'000) { // less than ~100KB left, clear cache
        WARN("Clearing cache on thread %u", gettid());
        as.GetCodeBuffer().RewindCursor();
        block_metadata.clear();
    }

    void* start = as.GetCursorPointer();

    // Map it immediately so we can optimize conditional branch to self
    block_metadata[rip].address = start;

    // A sequence of code. This is so that we can also call it recursively later.
    compileSequence(rip);

    expirePendingLinks(rip);

    return start;
}

void* Recompiler::getCompiledBlock(u64 rip) {
    if (blockExists(rip)) {
        return block_metadata[rip].address;
    }

    return nullptr;
}

void Recompiler::compileSequence(u64 rip) {
    compiling = true;
    scanFlagUsageAhead(rip);
    HandlerMetadata meta = {rip, rip};
    BlockMetadata& block_meta = block_metadata[rip];

    current_meta = &meta;
    current_block_metadata = &block_meta;
    current_sew = SEW::E1024;
    current_vlen = 0;
    current_grouping = LMUL::M1;

    while (compiling) {
        resetScratch();

        if (g_breakpoints.find(rip) != g_breakpoints.end()) {
            u64 current_address = (u64)as.GetCursorPointer();
            g_breakpoints[rip].push_back(current_address);
            as.GetCodeBuffer().Emit32(0); // UNIMP instruction
        }

        block_meta.instruction_spans.push_back({meta.rip, (u64)as.GetCursorPointer()});

        ZydisMnemonic mnemonic = decode(meta.rip, instruction, operands);
        switch (mnemonic) {
#define X(name)                                                                                                                                      \
    case ZYDIS_MNEMONIC_##name:                                                                                                                      \
        fast_##name(*this, meta, instruction, operands);                                                                                             \
        break;
#include "felix86/v2/handlers.inc"
#undef X
        default: {
            ZydisDisassembledInstruction disassembled;
            if (ZYAN_SUCCESS(ZydisDisassembleIntel(ZYDIS_MACHINE_MODE_LONG_64, meta.rip, (u8*)meta.rip, 15, &disassembled))) {
                ERROR("Unhandled instruction %s (%02x)", disassembled.text, (int)instruction.opcode);
            } else {
                ERROR("Unhandled instruction %s (%02x)", ZydisMnemonicGetString(mnemonic), (int)instruction.opcode);
            }
            break;
        }
        }

        // When we want to print all instructions used
        // static std::unordered_set<ZydisMnemonic> seen;
        // static bool start = true;

        // if (rip == 0x401690) {
        //     start = true;
        // }

        // if (start && seen.find(mnemonic) == seen.end()) {
        //     seen.insert(mnemonic);
        //     fflush(stdout);
        //     printf("Instruction %s\n", ZydisMnemonicGetString(mnemonic));
        //     fflush(stdout);
        // }

        // Checks that we didn't forget to emulate any flags
        // if (g_paranoid && mnemonic != ZYDIS_MNEMONIC_SYSCALL) {
        //     u32 changed = instruction.cpu_flags->modified | instruction.cpu_flags->set_0 | instruction.cpu_flags->set_1;
        //     u32 undefined = instruction.cpu_flags->undefined;

        //     if ((changed & ZYDIS_CPUFLAG_CF) && !getMetadata(X86_REF_CF).dirty) {
        //         ERROR("Instruction %s should've modified CF", ZydisMnemonicGetString(mnemonic));
        //     } else if (!(changed & ZYDIS_CPUFLAG_CF) && getMetadata(X86_REF_CF).dirty && !(undefined & ZYDIS_CPUFLAG_CF)) {
        //         ERROR("Instruction %s should've not modified CF", ZydisMnemonicGetString(mnemonic));
        //     }

        //     if ((changed & ZYDIS_CPUFLAG_AF) && !getMetadata(X86_REF_AF).dirty) {
        //         ERROR("Instruction %s should've modified AF", ZydisMnemonicGetString(mnemonic));
        //     } else if (!(changed & ZYDIS_CPUFLAG_AF) && getMetadata(X86_REF_AF).dirty && !(undefined & ZYDIS_CPUFLAG_AF)) {
        //         ERROR("Instruction %s should've not modified AF", ZydisMnemonicGetString(mnemonic));
        //     }

        //     if ((changed & ZYDIS_CPUFLAG_ZF) && !getMetadata(X86_REF_ZF).dirty) {
        //         ERROR("Instruction %s should've modified ZF", ZydisMnemonicGetString(mnemonic));
        //     } else if (!(changed & ZYDIS_CPUFLAG_ZF) && getMetadata(X86_REF_ZF).dirty && !(undefined & ZYDIS_CPUFLAG_ZF)) {
        //         ERROR("Instruction %s should've not modified ZF", ZydisMnemonicGetString(mnemonic));
        //     }

        //     if ((changed & ZYDIS_CPUFLAG_SF) && !getMetadata(X86_REF_SF).dirty) {
        //         ERROR("Instruction %s should've modified SF", ZydisMnemonicGetString(mnemonic));
        //     } else if (!(changed & ZYDIS_CPUFLAG_SF) && getMetadata(X86_REF_SF).dirty && !(undefined & ZYDIS_CPUFLAG_SF)) {
        //         ERROR("Instruction %s should've not modified SF", ZydisMnemonicGetString(mnemonic));
        //     }

        //     if ((changed & ZYDIS_CPUFLAG_OF) && !getMetadata(X86_REF_OF).dirty) {
        //         ERROR("Instruction %s should've modified OF", ZydisMnemonicGetString(mnemonic));
        //     } else if (!(changed & ZYDIS_CPUFLAG_OF) && getMetadata(X86_REF_OF).dirty && !(undefined & ZYDIS_CPUFLAG_OF)) {
        //         ERROR("Instruction %s should've not modified OF", ZydisMnemonicGetString(mnemonic));
        //     }

        //     writebackDirtyState();
        // }

        meta.rip += instruction.length;
    }

    current_block_metadata->address_end = as.GetCursorPointer();
    flush_icache(current_block_metadata->address, current_block_metadata->address_end);

    current_block_metadata = nullptr;
    current_meta = nullptr;
}

biscuit::GPR Recompiler::allocatedGPR(x86_ref_e reg) {
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
    case X86_REF_AF: {
        return biscuit::x22;
    }
    case X86_REF_ZF: {
        return biscuit::x23;
    }
    case X86_REF_SF: {
        return biscuit::x24;
    }
    case X86_REF_OF: {
        return biscuit::x25;
    }
    default: {
        UNREACHABLE();
        return x0;
    }
    }
}

biscuit::Vec Recompiler::allocatedVec(x86_ref_e reg) {
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

biscuit::GPR Recompiler::scratch() {
    switch (scratch_index++) {
    case 0:
        return x1;
    case 1:
        return x26;
    case 2:
        return x28;
    case 3:
        return x29;
    case 4:
        return x30;
    case 5:
        return x31;
    default:
        ERROR("Tried to use more than 6 scratch registers");
        return x0;
    }
}

biscuit::Vec Recompiler::scratchVec() {
    switch (vector_scratch_index++) {
    case 0:
        return v26;
    case 1:
        return v27;
    case 2:
        return v28;
    case 3:
        return v29;
    case 4:
        return v30;
    case 5:
        return v31;
    default:
        ERROR("Tried to use more than 6 scratch registers");
        return v0;
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

void Recompiler::resetScratch() {
    scratch_index = 0;
    vector_scratch_index = 0;
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

ZydisMnemonic Recompiler::decode(u64 rip, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands) {
    ZyanStatus status = ZydisDecoderDecodeFull(&decoder, (void*)rip, 15, &instruction, operands);
    if (!ZYAN_SUCCESS(status)) {
        ERROR("Failed to decode instruction at 0x%016lx", rip);
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
        biscuit::GPR address = lea(operand);

        readMemory(address, address, 0, zydisToSize(operand->size));

        return address;
    }
    case ZYDIS_OPERAND_TYPE_IMMEDIATE: {
        biscuit::GPR imm = scratch();
        as.LI(imm, operand->imm.value.s);
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
        biscuit::GPR address = lea(operand);
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
    addRegisterAccess(ref, true); // this is not quite right, as flags may be used as temporaries sometimes
                                  // but it's good enough for now. Surely the signal handler behavior won't care about flags that much :cluegi:
    return reg;
}

biscuit::GPR Recompiler::flagW(x86_ref_e ref) {
    biscuit::GPR reg = allocatedGPR(ref);
    RegisterMetadata& meta = getMetadata(ref);
    meta.dirty = true;
    meta.loaded = true;
    addRegisterAccess(ref, true);
    return reg;
}

biscuit::GPR Recompiler::flagWR(x86_ref_e ref) {
    biscuit::GPR reg = allocatedGPR(ref);
    RegisterMetadata& meta = getMetadata(ref);
    loadGPR(ref, reg);
    meta.dirty = true;
    meta.loaded = true;
    addRegisterAccess(ref, true);
    return reg;
}

biscuit::GPR Recompiler::getRefGPR(x86_ref_e ref, x86_size_e size) {
    biscuit::GPR gpr = allocatedGPR(ref);

    loadGPR(ref, gpr);
    addRegisterAccess(ref, true); // mark register state as valid at this address

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
        biscuit::GPR gpr32 = scratch();
        zext(gpr32, gpr, X86_SIZE_DWORD);
        return gpr32;
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
    addRegisterAccess(ref, true); // mark register state as valid at this address

    return vec;
}

void Recompiler::setRefGPR(x86_ref_e ref, x86_size_e size, biscuit::GPR reg) {
    switch (size) {
    case X86_SIZE_BYTE: {
        biscuit::GPR dest = getRefGPR(ref, X86_SIZE_QWORD);
        biscuit::GPR gpr8 = scratch();
        as.ANDI(gpr8, reg, 0xff);
        as.ANDI(dest, dest, ~0xff);
        as.OR(dest, dest, gpr8);
        popScratch();
        break;
    }
    case X86_SIZE_BYTE_HIGH: {
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
        if (Extensions::B) {
            as.ZEXTW(dest, reg);
        } else {
            as.SLLI(dest, reg, 32);
            as.SRLI(dest, dest, 32);
        }
        break;
    }
    case X86_SIZE_QWORD: {
        biscuit::GPR dest = allocatedGPR(ref); // don't need to load as the entire register is overwritten
        if (dest != reg)
            as.MV(dest, reg);
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }

    RegisterMetadata& meta = getMetadata(ref);
    meta.dirty = true;
    meta.loaded = true;           // since the value is fresh it's as if we read it from memory
    addRegisterAccess(ref, true); // mark register state as valid at this address
}

void Recompiler::setRefVec(x86_ref_e ref, biscuit::Vec vec) {
    biscuit::Vec dest = allocatedVec(ref);

    if (dest != vec) {
        as.VMV(dest, vec);
    }

    RegisterMetadata& meta = getMetadata(ref);
    meta.dirty = true;
    meta.loaded = true;           // since the value is fresh it's as if we read it from memory
    addRegisterAccess(ref, true); // mark register state as valid at this address
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
        biscuit::GPR address = lea(operand);

        switch (operand->size) {
        case 8: {
            as.SB(reg, 0, address);
            break;
        }
        case 16: {
            as.SH(reg, 0, address);
            break;
        }
        case 32: {
            as.SW(reg, 0, address);
            break;
        }
        case 64: {
            as.SD(reg, 0, address);
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

void Recompiler::setOperandVec(ZydisDecodedOperand* operand, biscuit::Vec vec) {
    switch (operand->type) {
    case ZYDIS_OPERAND_TYPE_REGISTER: {
        x86_ref_e ref = zydisToRef(operand->reg.value);
        setRefVec(ref, vec);
        break;
    }
    case ZYDIS_OPERAND_TYPE_MEMORY: {
        biscuit::GPR address = lea(operand);
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

void Recompiler::addRegisterAccess(x86_ref_e ref, bool valid) {
    std::vector<RegisterAccess>* access = nullptr;
    switch (ref) {
    case X86_REF_RAX ... X86_REF_R15: {
        access = &current_block_metadata->register_accesses[ref - X86_REF_RAX];
        break;
    }
    case X86_REF_CF: {
        access = &current_block_metadata->register_accesses[16];
        break;
    }
    case X86_REF_AF: {
        access = &current_block_metadata->register_accesses[17];
        break;
    }
    case X86_REF_ZF: {
        access = &current_block_metadata->register_accesses[18];
        break;
    }
    case X86_REF_SF: {
        access = &current_block_metadata->register_accesses[19];
        break;
    }
    case X86_REF_OF: {
        access = &current_block_metadata->register_accesses[20];
        break;
    }
    case X86_REF_XMM0 ... X86_REF_XMM15: {
        access = &current_block_metadata->register_accesses[ref - X86_REF_XMM0 + 16 + 5];
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }

    u64 address = (u64)as.GetCursorPointer();
    if (access->empty()) {
        ASSERT(valid);
        access->push_back({address, valid});
    } else {
        RegisterAccess& last = access->back();
        if (!last.valid) {
            ASSERT(valid); // should never go from invalid to invalid
            access->push_back({address, valid});
        } else if (last.valid && !valid) { // only push a state change if it goes from valid to invalid
            access->push_back({address, valid});
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
    if (current_sew == sew && current_vlen == vlen && current_grouping != grouping) {
        return false;
    }

    current_sew = sew;
    current_vlen = vlen;
    current_grouping = grouping;

    as.VSETIVLI(x0, vlen, sew, grouping);
    return true;
}

biscuit::GPR Recompiler::lea(ZydisDecodedOperand* operand) {
    biscuit::GPR address = scratch();

    biscuit::GPR base, index;

    if (operand->mem.base == ZYDIS_REGISTER_RIP) {
        as.LD(address, offsetof(ThreadState, rip), threadStatePointer());
        addi(address, address, operand->mem.disp.value + instruction.length + current_meta->rip - current_meta->block_start);
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

    if (instruction.address_width == 32) {
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
        if (metadata[i].dirty) {
            x86_ref_e ref = (x86_ref_e)(X86_REF_RAX + i);
            as.SD(allocatedGPR(ref), offsetof(ThreadState, gprs) + i * sizeof(u64), threadStatePointer());
            addRegisterAccess(ref, false);
        }
    }

    biscuit::GPR address = scratch();
    for (int i = 0; i < 16; i++) {
        x86_ref_e ref = (x86_ref_e)(X86_REF_XMM0 + i);
        if (getMetadata(ref).dirty) {
            setVectorState(SEW::E64, maxVlen() / 64);
            as.ADDI(address, threadStatePointer(), offsetof(ThreadState, xmm) + i * 16);
            as.VSE64(allocatedVec(ref), address);
            addRegisterAccess(ref, false);
        }
    }
    popScratch();

    if (getMetadata(X86_REF_CF).dirty) {
        as.SB(allocatedGPR(X86_REF_CF), offsetof(ThreadState, cf), threadStatePointer());
        addRegisterAccess(X86_REF_CF, false);
    }

    if (getMetadata(X86_REF_AF).dirty) {
        as.SB(allocatedGPR(X86_REF_AF), offsetof(ThreadState, af), threadStatePointer());
        addRegisterAccess(X86_REF_AF, false);
    }

    if (getMetadata(X86_REF_ZF).dirty) {
        as.SB(allocatedGPR(X86_REF_ZF), offsetof(ThreadState, zf), threadStatePointer());
        addRegisterAccess(X86_REF_ZF, false);
    }

    if (getMetadata(X86_REF_SF).dirty) {
        as.SB(allocatedGPR(X86_REF_SF), offsetof(ThreadState, sf), threadStatePointer());
        addRegisterAccess(X86_REF_SF, false);
    }

    if (getMetadata(X86_REF_OF).dirty) {
        as.SB(allocatedGPR(X86_REF_OF), offsetof(ThreadState, of), threadStatePointer());
        addRegisterAccess(X86_REF_OF, false);
    }

    for (size_t i = 0; i < metadata.size(); i++) {
        metadata[i].dirty = false;
        metadata[i].loaded = false;
    }

    current_sew = SEW::E1024;
    current_vlen = 0;
    current_grouping = LMUL::M1;
}

void Recompiler::backToDispatcher() {
    biscuit::GPR address = scratch();
    as.LD(address, offsetof(ThreadState, compile_next_handler), threadStatePointer());
    as.JR(address);
    popScratch();
}

void Recompiler::enterDispatcher(ThreadState* state) {
    enter_dispatcher(state);
}

void Recompiler::exitDispatcher(ThreadState* state) {
    exit_dispatcher(state);
}

void* Recompiler::getCompileNext() {
    return compile_next_handler;
}

void Recompiler::scanFlagUsageAhead(u64 rip) {
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

bool Recompiler::shouldEmitFlag(u64 rip, x86_ref_e ref) {
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
        ERROR("This needs B extension");
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

void Recompiler::jumpAndLink(u64 rip) {
    if (g_dont_link) {
        // Just emit jump to dispatcher
        as.NOP();
        as.LD(t0, offsetof(ThreadState, compile_next_handler), threadStatePointer());
        as.JR(t0);
        return;
    }

    if (!blockExists(rip)) {
        // 3 instructions of space to be overwritten with:
        // AUIPC
        // ADDI
        // JR
        u64 link_me = (u64)as.GetCodeBuffer().GetCursorOffset();
        as.NOP();
        as.LD(t0, offsetof(ThreadState, compile_next_handler), threadStatePointer());
        as.JR(t0);

        block_metadata[rip].pending_links.push_back(link_me);
    } else {
        u64 target = (u64)block_metadata[rip].address;
        u64 offset = target - (u64)as.GetCursorPointer();

        if (IsValidJTypeImm(offset)) {
            if (offset != 3 * 4) {
                u32 mem;
                Assembler tempas((u8*)&mem, 4);
                tempas.J(offset);

                // TODO: remove atomic stuff
                // Atomically replace the NOP with a J instruction
                // The instructions after that J can stay as they are
                __atomic_store_n((u32*)as.GetCursorPointer(), mem, __ATOMIC_SEQ_CST);
                as.AdvanceBuffer(as.GetCodeBuffer().GetCursorOffset() + 4);
            } else {
                // Offset is just ahead, we can inline
                as.NOP(); // Skip the first NOP

                // Atomically replace the LD + JR with 2 NOPs
                u64 mem;
                Assembler tempas((u8*)&mem, 8);
                tempas.NOP();
                tempas.NOP();

                // mem now has the 2 NOPs, store them atomically
                __atomic_store_n((u64*)as.GetCursorPointer(), mem, __ATOMIC_SEQ_CST);
                as.AdvanceBuffer(as.GetCodeBuffer().GetCursorOffset() + 8);
            }

        } else {
            const auto hi20 = static_cast<int32_t>((static_cast<uint32_t>(offset) + 0x800) >> 12 & 0xFFFFF);
            const auto lo12 = static_cast<int32_t>(offset << 20) >> 20;
            u64 mem;
            biscuit::Assembler tempas((u8*)&mem, 8);
            tempas.AUIPC(t0, hi20);
            tempas.ADDI(t0, t0, lo12);

            // Atomically replace the NOP + LD with AUIPC and ADDI, JR doesn't need to be replaced atomically
            __atomic_store_n((u64*)as.GetCursorPointer(), mem, __ATOMIC_SEQ_CST);

            as.AdvanceBuffer(as.GetCodeBuffer().GetCursorOffset() + 8);
            as.JR(t0);
        }
    }
}

void Recompiler::jumpAndLinkConditional(biscuit::GPR condition, biscuit::GPR gpr_true, biscuit::GPR gpr_false, u64 rip_true, u64 rip_false) {
    bool ok = false;
    if (blockExists(rip_true)) {
        // The -4 is due to the setRip emitting an SD instruction
        auto offset_true = (u64)block_metadata[rip_true].address - (u64)as.GetCursorPointer() - 4;
        if (IsValidBTypeImm(offset_true)) {
            setRip(gpr_true);
            as.BNEZ(condition, offset_true);
            setRip(gpr_false);
            jumpAndLink(rip_false);
            ok = true;
        } else if (blockExists(rip_false)) {
            auto offset_false = (u64)block_metadata[rip_false].address - (u64)as.GetCursorPointer() - 4;
            if (IsValidBTypeImm(offset_false)) {
                setRip(gpr_false);
                as.BEQZ(condition, offset_false);
                setRip(gpr_true);
                jumpAndLink(rip_true);
                ok = true;
            }
        }
    } else if (blockExists(rip_false)) {
        auto offset_false = (u64)block_metadata[rip_false].address - (u64)as.GetCursorPointer() - 4;
        if (IsValidBTypeImm(offset_false)) {
            setRip(gpr_false);
            as.BEQZ(condition, offset_false);
            setRip(gpr_true);
            jumpAndLink(rip_true);
            ok = true;
        }
    }

    if (!ok) {
        Label false_label;
        as.BEQZ(condition, &false_label);

        setRip(gpr_true);
        jumpAndLink(rip_true);

        as.Bind(&false_label);

        setRip(gpr_false);
        jumpAndLink(rip_false);
    }
}

void Recompiler::expirePendingLinks(u64 rip) {
    if (g_dont_link) {
        return;
    }

    if (!blockExists(rip)) {
        return;
    }

    auto& links = block_metadata[rip].pending_links;
    for (u64 link : links) {
        auto current_offset = as.GetCodeBuffer().GetCursorOffset();
        void* start = as.GetCursorPointer();

        as.RewindBuffer(link);

        // This will atomically replace the instructions with appropriate ones that jump directly
        // to the next block
        jumpAndLink(rip);

        void* end = as.GetCursorPointer();

        flush_icache(start, end);

        as.AdvanceBuffer(current_offset);
    }

    links.clear();
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
        biscuit::GPR pf = scratch();
        as.XORI(pf, flag(X86_REF_PF), 1);
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
}

void Recompiler::writeMemory(biscuit::GPR src, biscuit::GPR address, i64 offset, x86_size_e size) {
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
    case 128:
        return X86_SIZE_XMM;
    default:
        UNREACHABLE();
        return X86_SIZE_BYTE;
    }
}

void Recompiler::repPrologue(Label* loop_end) {
    // Signal handling would get tricky if we had to account for this looping mess of an instruction
    disableSignals();
    biscuit::GPR rcx = getRefGPR(X86_REF_RCX, X86_SIZE_QWORD);
    as.BEQZ(rcx, loop_end);
}

void Recompiler::repEpilogue(Label* loop_body) {
    biscuit::GPR rcx = getRefGPR(X86_REF_RCX, X86_SIZE_QWORD);
    as.ADDI(rcx, rcx, -1);
    setRefGPR(X86_REF_RCX, X86_SIZE_QWORD, rcx);
    as.BNEZ(rcx, loop_body);
    enableSignals();
}

void Recompiler::repzEpilogue(Label* loop_body, bool is_repz) {
    biscuit::GPR rcx = getRefGPR(X86_REF_RCX, X86_SIZE_QWORD);
    as.ADDI(rcx, rcx, -1);
    setRefGPR(X86_REF_RCX, X86_SIZE_QWORD, rcx);
    as.BNEZ(rcx, loop_body);

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
    default: {
        UNREACHABLE();
        break;
    }
    }
}

BlockMetadata& Recompiler::getBlockMetadata(u64 rip) {
    ASSERT(block_metadata.find(rip) != block_metadata.end());
    return block_metadata[rip];
}

bool Recompiler::blockExists(u64 rip) {
    return block_metadata[rip].address != nullptr;
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
    as.ORI(reg, reg, 0b10); // bit 1 always set in flags
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

void Recompiler::readBitstring(biscuit::GPR dest, ZydisDecodedOperand* operand, biscuit::GPR bit) {
    biscuit::GPR shift = scratch();
    biscuit::GPR address = lea(operand);

    u8 shr = 0;
    u8 shl = 0;
    switch (operands[0].size) {
    case 16:
        shr = 4;
        shl = 1;
        break;
    case 32:
        shr = 5;
        shl = 2;
        break;
    case 64:
        shr = 6;
        shl = 3;
        break;
    default:
        UNREACHABLE();
    }

    // Point to the exact word in memory
    Label gtzero;
    as.BGEZ(bit, &gtzero);

    // If the shift is less than zero, this is possible but we don't handle it yet so let's panic
    as.LI(shift, EXIT_REASON_NEGATIVE_BITSTRING);
    as.SB(shift, offsetof(ThreadState, exit_reason), threadStatePointer());
    as.LI(shift, (u64)exit_dispatcher);
    as.JR(shift);

    as.Bind(&gtzero);
    as.SRLI(shift, bit, shr);
    as.SLLI(shift, shift, shl);
    as.ADD(address, address, shift);
    readMemory(dest, address, 0, zydisToSize(operands[0].size));
    popScratch();
    popScratch();
}

void Recompiler::tryFastReturn(biscuit::GPR rip) {
    // Implement me
}