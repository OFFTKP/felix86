#include <sys/mman.h>
#include "felix86/emulator.hpp"
#include "felix86/v2/fast_recompiler.hpp"

#define X(name)                                                                                                                                      \
    void fast_##name(FastRecompiler& rec, const HandlerMetadata& meta, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands);
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
    ZydisDecoderEnableMode(&decoder, ZYDIS_DECODER_MODE_AMD_BRANCHES, ZYAN_TRUE);
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
    as.LBU(a0, offsetof(ThreadState, exit_reason), threadStatePointer());
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

    // Map it immediately so we can optimize conditional branch to self
    map[rip] = {start, (u64)as.GetCursorPointer() - (u64)start};

    // A sequence of code. This is so that we can also call it recursively later.
    compileSequence(rip);

    expirePendingLinks(rip);

    // Make code visible to instruction fetches.
    flushICache();

    return start;
}

void FastRecompiler::compileSequence(u64 rip) {
    compiling = true;
    scanFlagUsageAhead(rip);

    HandlerMetadata meta = {rip, rip};

    current_meta = &meta;

    current_sew = SEW::E1024;
    current_vlen = 0;

    while (compiling) {
        resetScratch();
        ZydisMnemonic mnemonic = decode(meta.rip, instruction, operands);
        switch (mnemonic) {
#define X(name)                                                                                                                                      \
    case ZYDIS_MNEMONIC_##name:                                                                                                                      \
        fast_##name(*this, meta, instruction, operands);                                                                                             \
        break;
#include "felix86/v2/handlers.inc"
#undef X
        default: {
            ERROR("Unhandled instruction %s (%02x)", ZydisMnemonicGetString(mnemonic), (int)instruction.opcode);
            break;
        }
        }
        meta.rip += instruction.length;
    }

    current_meta = nullptr;
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

biscuit::GPR FastRecompiler::scratch() {
    switch (scratch_index++) {
    case 0:
        return x1;
    case 1:
        return x28;
    case 2:
        return x29;
    case 3:
        return x30;
    case 4:
        return x31;
    default:
        ERROR("Tried to use more than 5 scratch registers");
        return x0;
    }
}

biscuit::Vec FastRecompiler::scratchVec() {
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

void FastRecompiler::popScratchVec() {
    vector_scratch_index--;
    ASSERT(vector_scratch_index >= 0);
}

void FastRecompiler::popScratch() {
    scratch_index--;
    ASSERT(scratch_index >= 0);
}

void FastRecompiler::resetScratch() {
    scratch_index = 0;
    vector_scratch_index = 0;
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
        ERROR("Unhandled register %s", ZydisRegisterGetString(reg));
        ref = X86_REF_RAX;
        break;
    }
    }

    return ref;
}

x86_size_e FastRecompiler::zydisToSize(ZydisRegister reg) {
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

biscuit::GPR FastRecompiler::gpr(ZydisRegister reg) {
    x86_ref_e ref = zydisToRef(reg);
    x86_size_e size = zydisToSize(reg);
    return getRefGPR(ref, size);
}

biscuit::Vec FastRecompiler::vec(ZydisRegister reg) {
    ASSERT(reg >= ZYDIS_REGISTER_XMM0 && reg <= ZYDIS_REGISTER_XMM15);
    x86_ref_e ref = zydisToRef(reg);
    return getRefVec(ref);
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

x86_size_e FastRecompiler::getOperandSize(ZydisDecodedOperand* operand) {
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
            as.LBU(address, 0, address);
            break;
        }
        case 16: {
            as.LHU(address, 0, address);
            break;
        }
        case 32: {
            as.LWU(address, 0, address);
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
        as.LI(imm, operand->imm.value.s);
        return imm;
    }
    default: {
        UNREACHABLE();
        return x0;
    }
    }
}

biscuit::GPR FastRecompiler::getOperandGPRDontZext(ZydisDecodedOperand* operand) {
    switch (operand->type) {
    case ZYDIS_OPERAND_TYPE_REGISTER: {
        biscuit::GPR reg = getRefGPR(zydisToRef(operand->reg.value), X86_SIZE_QWORD);
        loadGPR(zydisToRef(operand->reg.value), reg);

        if (zydisToSize(operand->reg.value) == X86_SIZE_BYTE_HIGH) {
            as.SRLI(reg, reg, 8);
        }

        return reg;
    }
    case ZYDIS_OPERAND_TYPE_MEMORY: {
        return getOperandGPR(operand);
    }
    case ZYDIS_OPERAND_TYPE_IMMEDIATE: {
        return getOperandGPR(operand);
    }
    default: {
        UNREACHABLE();
        return x0;
    }
    }
}

biscuit::Vec FastRecompiler::getOperandVec(ZydisDecodedOperand* operand) {
    switch (operand->type) {
    case ZYDIS_OPERAND_TYPE_REGISTER: {
        biscuit::Vec reg = vec(operand->reg.value);
        return reg;
    }
    case ZYDIS_OPERAND_TYPE_MEMORY: {
        biscuit::GPR address = lea(operand);
        biscuit::Vec vec = scratchVec();

        switch (operand->size) {
        case 32: {
            setVectorState(SEW::E32, 1);
            as.VLE32(vec, address);
            break;
        }
        case 64: {
            setVectorState(SEW::E64, 1);
            as.VLE64(vec, address);
            break;
        }
        case 128: {
            setVectorState(SEW::E64, 2);
            as.VLE64(vec, address);
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

biscuit::GPR FastRecompiler::flag(x86_ref_e ref) {
    biscuit::GPR reg = allocatedGPR(ref);
    loadGPR(ref, reg);
    return reg;
}

biscuit::GPR FastRecompiler::flagW(x86_ref_e ref) {
    biscuit::GPR reg = allocatedGPR(ref);
    RegisterMetadata& meta = getMetadata(ref);
    meta.dirty = true;
    meta.loaded = true;
    return reg;
}

biscuit::GPR FastRecompiler::getRefGPR(x86_ref_e ref, x86_size_e size) {
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

bool FastRecompiler::isGPR(ZydisRegister reg) {
    return zydisToRef(reg) >= X86_REF_RAX && zydisToRef(reg) <= X86_REF_R15;
}

biscuit::Vec FastRecompiler::getRefVec(x86_ref_e ref) {
    biscuit::Vec vec = allocatedVec(ref);

    loadVec(ref, vec);

    return vec;
}

void FastRecompiler::setRefGPR(x86_ref_e ref, x86_size_e size, biscuit::GPR reg) {
    biscuit::GPR dest = allocatedGPR(ref);

    switch (size) {
    case X86_SIZE_BYTE: {
        biscuit::GPR gpr8 = scratch();
        as.ANDI(gpr8, reg, 0xff);
        as.ANDI(dest, dest, ~0xff);
        as.OR(dest, dest, gpr8);
        popScratch();
        break;
    }
    case X86_SIZE_BYTE_HIGH: {
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
        if (Extensions::B) {
            as.ZEXTW(dest, reg);
        } else {
            as.SLLI(dest, reg, 32);
            as.SRLI(dest, dest, 32);
        }
        break;
    }
    case X86_SIZE_QWORD: {
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
    meta.loaded = true; // since the value is fresh it's as if we read it from memory
}

void FastRecompiler::setRefVec(x86_ref_e ref, biscuit::Vec vec) {
    biscuit::Vec dest = allocatedVec(ref);

    if (dest != vec) {
        as.VMV(dest, vec);
    }

    RegisterMetadata& meta = getMetadata(ref);
    meta.dirty = true;
    meta.loaded = true; // since the value is fresh it's as if we read it from memory
}

void FastRecompiler::setOperandGPR(ZydisDecodedOperand* operand, biscuit::GPR reg) {
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

void FastRecompiler::setOperandVec(ZydisDecodedOperand* operand, biscuit::Vec vec) {
    switch (operand->type) {
    case ZYDIS_OPERAND_TYPE_REGISTER: {
        x86_ref_e ref = zydisToRef(operand->reg.value);
        setRefVec(ref, vec);
        break;
    }
    case ZYDIS_OPERAND_TYPE_MEMORY: {
        switch (operand->size) {
        case 128: {
            biscuit::GPR address = lea(operand);
            setVectorState(SEW::E64, 2);
            as.VSE64(vec, address);
            break;
        }
        case 64: {
            biscuit::GPR address = lea(operand);
            setVectorState(SEW::E64, 1);
            as.VSE64(vec, address);
            break;
        }
        case 32: {
            biscuit::GPR address = lea(operand);
            setVectorState(SEW::E32, 1);
            as.VSE32(vec, address);
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

    meta.loaded = true;
    if (reg >= X86_REF_RAX && reg <= X86_REF_R15) {
        as.LD(gpr, offsetof(ThreadState, gprs) + (reg - X86_REF_RAX) * sizeof(u64), threadStatePointer());
    } else {
        switch (reg) {
        case X86_REF_CF: {
            as.LBU(gpr, offsetof(ThreadState, cf), threadStatePointer());
            break;
        }
        case X86_REF_PF: {
            as.LBU(gpr, offsetof(ThreadState, pf), threadStatePointer());
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

void FastRecompiler::loadVec(x86_ref_e reg, biscuit::Vec vec) {
    RegisterMetadata& meta = getMetadata(reg);
    if (meta.loaded) {
        return;
    }

    meta.loaded = true;
    biscuit::GPR address = scratch();
    u64 offset = offsetof(ThreadState, xmm) + (reg - X86_REF_XMM0) * 16;
    as.ADDI(address, threadStatePointer(), offset);
    setVectorState(SEW::E64, max_vlen / 64);
    as.VLE64(vec, address);
    popScratch();
}

void FastRecompiler::setVectorState(SEW sew, int vlen) {
    if (current_sew == sew && current_vlen == vlen) {
        return;
    }

    current_sew = sew;
    current_vlen = vlen;

    as.VSETIVLI(x0, vlen, sew);
}

biscuit::GPR FastRecompiler::lea(ZydisDecodedOperand* operand) {
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

void FastRecompiler::writebackDirtyState() {
    for (int i = 0; i < 16; i++) {
        if (metadata[i].dirty) {
            as.SD(allocatedGPR((x86_ref_e)(X86_REF_RAX + i)), offsetof(ThreadState, gprs) + i * sizeof(u64), threadStatePointer());
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

    if (getMetadata(X86_REF_PF).dirty) {
        as.SB(allocatedGPR(X86_REF_PF), offsetof(ThreadState, pf), threadStatePointer());
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

    for (int i = 0; i < metadata.size(); i++) {
        metadata[i].dirty = false;
        metadata[i].loaded = false;
    }
}

void FastRecompiler::backToDispatcher() {
    biscuit::GPR address = scratch();
    as.LD(address, offsetof(ThreadState, compile_next_handler), threadStatePointer());
    as.JR(address);
    popScratch();
}

void FastRecompiler::enterDispatcher(ThreadState* state) {
    g_thread_state = state;
    enter_dispatcher(state);
}

void* FastRecompiler::getCompileNext() {
    return compile_next_handler;
}

void FastRecompiler::scanFlagUsageAhead(u64 rip) {
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

        if (is_jump || is_ret || is_call || is_illegal || is_hlt) {
            break;
        }

        if (instruction.attributes & ZYDIS_ATTRIB_CPUFLAG_ACCESS) {
            u32 changed =
                instruction.cpu_flags->modified | instruction.cpu_flags->set_0 | instruction.cpu_flags->set_1 | instruction.cpu_flags->undefined;
            u32 used = instruction.cpu_flags->tested;

            if (used & ZYDIS_CPUFLAG_CF) {
                flag_access_cpazso[0].push_back({false, rip});
            } else if (changed & ZYDIS_CPUFLAG_CF) {
                flag_access_cpazso[0].push_back({true, rip});
            }

            if (used & ZYDIS_CPUFLAG_PF) {
                flag_access_cpazso[1].push_back({false, rip});
            } else if (changed & ZYDIS_CPUFLAG_PF) {
                flag_access_cpazso[1].push_back({true, rip});
            }

            if (used & ZYDIS_CPUFLAG_AF) {
                flag_access_cpazso[2].push_back({false, rip});
            } else if (changed & ZYDIS_CPUFLAG_AF) {
                flag_access_cpazso[2].push_back({true, rip});
            }

            if (used & ZYDIS_CPUFLAG_ZF) {
                flag_access_cpazso[3].push_back({false, rip});
            } else if (changed & ZYDIS_CPUFLAG_ZF) {
                flag_access_cpazso[3].push_back({true, rip});
            }

            if (used & ZYDIS_CPUFLAG_SF) {
                flag_access_cpazso[4].push_back({false, rip});
            } else if (changed & ZYDIS_CPUFLAG_SF) {
                flag_access_cpazso[4].push_back({true, rip});
            }

            if (used & ZYDIS_CPUFLAG_OF) {
                flag_access_cpazso[5].push_back({false, rip});
            } else if (changed & ZYDIS_CPUFLAG_OF) {
                flag_access_cpazso[5].push_back({true, rip});
            }
        }

        rip += instruction.length;
    }
}

bool FastRecompiler::shouldEmitFlag(u64 rip, x86_ref_e ref) {
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

void FastRecompiler::zext(biscuit::GPR dest, biscuit::GPR src, x86_size_e size) {
    switch (size) {
    case X86_SIZE_BYTE: {
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
        as.MV(dest, src);
        break;
    }
    default: {
        UNREACHABLE();
        break;
    }
    }
}

int FastRecompiler::getBitSize(x86_size_e size) {
    switch (size) {
    case X86_SIZE_BYTE:
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

u64 FastRecompiler::getSignMask(x86_size_e size_e) {
    u16 size = getBitSize(size_e);
    return 1ull << (size - 1);
}

void FastRecompiler::updateParity(biscuit::GPR result) {
    if (Extensions::B) {
        biscuit::GPR pf = flagW(X86_REF_PF);
        as.ANDI(pf, result, 0xFF);
        as.CPOPW(pf, pf);
        as.ANDI(pf, pf, 1);
        as.XORI(pf, pf, 1);
    } else {
        ERROR("This needs B extension");
    }
}

void FastRecompiler::updateZero(biscuit::GPR result) {
    biscuit::GPR zf = flagW(X86_REF_ZF);
    as.SEQZ(zf, result);
}

void FastRecompiler::updateSign(biscuit::GPR result, x86_size_e size) {
    biscuit::GPR sf = flagW(X86_REF_SF);
    as.SRLI(sf, result, getBitSize(size) - 1);
    as.ANDI(sf, sf, 1);
}

void FastRecompiler::setRip(biscuit::GPR rip) {
    as.SD(rip, offsetof(ThreadState, rip), threadStatePointer());
}

biscuit::GPR FastRecompiler::getRip() {
    biscuit::GPR rip = scratch();
    as.LD(rip, offsetof(ThreadState, rip), threadStatePointer());
    return rip;
}

void FastRecompiler::jumpAndLink(u64 rip) {
    if (map.find(rip) == map.end()) {
        biscuit::GPR address = scratch();
        // 3 instructions of space to be overwritten with:
        // AUIPC
        // ADDI
        // JR
        u64 link_me = (u64)as.GetCodeBuffer().GetCursorOffset();
        as.NOP();
        as.LD(address, offsetof(ThreadState, compile_next_handler), threadStatePointer());
        as.JR(address);
        popScratch();

        if (!g_dont_link) {
            pending_links[rip].push_back(link_me);
        }
    } else {
        u64 target = (u64)map[rip].first;
        u64 offset = target - (u64)as.GetCursorPointer();

        if (IsValidJTypeImm(offset)) {
            if (offset != 3 * 4) {
                as.J(offset);
            } else {
                as.NOP(); // offset is just ahead, inline it
            }
            as.NOP();
            as.NOP();
        } else {
            const auto hi20 = static_cast<int32_t>((static_cast<uint32_t>(offset) + 0x800) >> 12 & 0xFFFFF);
            const auto lo12 = static_cast<int32_t>(offset << 20) >> 20;
            biscuit::GPR reg = scratch();
            as.AUIPC(reg, hi20);
            as.ADDI(reg, reg, lo12);
            as.JR(reg);
            popScratch();
        }
    }
}

void FastRecompiler::jumpAndLinkConditional(biscuit::GPR condition, biscuit::GPR gpr_true, biscuit::GPR gpr_false, u64 rip_true, u64 rip_false) {
    bool ok = false;
    if (map.find(rip_true) != map.end()) {
        // The -4 is due to the setRip emitting an SD instruction
        auto offset_true = (u64)map[rip_true].first - (u64)as.GetCursorPointer() - 4;
        if (IsValidBTypeImm(offset_true)) {
            setRip(gpr_true);
            as.BNEZ(condition, offset_true);
            setRip(gpr_false);
            jumpAndLink(rip_false);
            ok = true;
        } else if (map.find(rip_false) != map.end()) {
            auto offset_false = (u64)map[rip_false].first - (u64)as.GetCursorPointer() - 4;
            if (IsValidBTypeImm(offset_false)) {
                setRip(gpr_false);
                as.BEQZ(condition, offset_false);
                setRip(gpr_true);
                jumpAndLink(rip_true);
                ok = true;
            }
        }
    } else if (map.find(rip_false) != map.end()) {
        auto offset_false = (u64)map[rip_false].first - (u64)as.GetCursorPointer() - 4;
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

void FastRecompiler::expirePendingLinks(u64 rip) {
    if (g_dont_link) {
        return;
    }

    if (pending_links.find(rip) == pending_links.end()) {
        return;
    }

    ASSERT(map.find(rip) != map.end());

    auto& links = pending_links[rip];
    for (u64 link : links) {
        auto current_offset = as.GetCodeBuffer().GetCursorOffset();

        as.RewindBuffer(link);
        jumpAndLink(rip);
        as.AdvanceBuffer(current_offset);
    }

    pending_links.erase(rip);
}

u64 FastRecompiler::sextImmediate(u64 imm, ZyanU8 size) {
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

void FastRecompiler::addi(biscuit::GPR dst, biscuit::GPR src, u64 imm) {
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

void FastRecompiler::setFlagUndefined(x86_ref_e ref) {
    // Once a flag has been set to undefined state it doesn't need to be written back
    // it's as if it was written with a random value, which we don't care to emulate
    RegisterMetadata& meta = getMetadata(ref);
    meta.loaded = false;
    meta.dirty = false;
}

void FastRecompiler::sextb(biscuit::GPR dest, biscuit::GPR src) {
    if (Extensions::B) {
        as.SEXTB(dest, src);
    } else {
        as.SLLI(dest, src, 56);
        as.SRAI(dest, dest, 56);
    }
}

void FastRecompiler::sexth(biscuit::GPR dest, biscuit::GPR src) {
    if (Extensions::B) {
        as.SEXTH(dest, src);
    } else {
        as.SLLI(dest, src, 48);
        as.SRAI(dest, dest, 48);
    }
}

biscuit::GPR FastRecompiler::getCond(int cond) {
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
        biscuit::GPR temp = scratch();
        as.XORI(cond, flag(X86_REF_CF), 1);
        as.XORI(temp, flag(X86_REF_ZF), 1);
        as.AND(cond, cond, temp);
        popScratch();
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

void FastRecompiler::readMemory(biscuit::GPR dest, biscuit::GPR address, i64 offset, x86_size_e size) {
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

void FastRecompiler::writeMemory(biscuit::GPR src, biscuit::GPR address, i64 offset, x86_size_e size) {
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

x86_size_e FastRecompiler::zydisToSize(ZyanU8 size) {
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

void FastRecompiler::repPrologue(Label* loop_end) {
    biscuit::GPR rcx = getRefGPR(X86_REF_RCX, X86_SIZE_QWORD);
    as.BEQZ(rcx, loop_end);
}

void FastRecompiler::repEpilogue(Label* loop_body) {
    biscuit::GPR rcx = getRefGPR(X86_REF_RCX, X86_SIZE_QWORD);
    as.ADDI(rcx, rcx, -1);
    setRefGPR(X86_REF_RCX, X86_SIZE_QWORD, rcx);
    as.BNEZ(rcx, loop_body);
}

void FastRecompiler::repzEpilogue(Label* loop_body, bool is_repz) {
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
}