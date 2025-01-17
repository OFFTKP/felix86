#pragma once

#include <array>
#include <unordered_map>
#include <Zydis/Utils.h>
#include "Zydis/Decoder.h"
#include "biscuit/registers.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/common/x86.hpp"

struct FastRecompiler {
    FastRecompiler(Emulator& emulator);
    ~FastRecompiler();
    FastRecompiler(const FastRecompiler&) = delete;
    FastRecompiler& operator=(const FastRecompiler&) = delete;
    FastRecompiler(FastRecompiler&&) = delete;
    FastRecompiler& operator=(FastRecompiler&&) = delete;

    void* compile(u64 rip);

private:
    struct RegisterMetadata {
        x86_ref_e reg;
        bool dirty = false;  // whether an instruction modified this value, so we know to store it to memory before exiting execution
        bool loaded = false; // whether a previous instruction loaded this value from memory, so we don't load it again
                             // if a syscall happens for example, this would be set to false so we load it again
    };

    enum FlagNeed {
        NOT_NEEDED = 0,        // not set by this instruction
        MAYBE_NEEDED = 1,      // we don't know if it's used so it must be set
        ABSOLUTELY_NEEDED = 2, // a use is found and this flag must be set
    };

    struct FlagsNeeded {
        FlagNeed cf = NOT_NEEDED;
        FlagNeed pf = NOT_NEEDED;
        FlagNeed af = NOT_NEEDED;
        FlagNeed zf = NOT_NEEDED;
        FlagNeed sf = NOT_NEEDED;
        FlagNeed of = NOT_NEEDED;
    };

    void compileSequence(u64 rip);

    // Get the allocated register for the given register reference
    biscuit::GPR gpr(x86_ref_e reg);

    biscuit::Vec vec(x86_ref_e reg);

    constexpr biscuit::GPR threadStatePointer();

    constexpr biscuit::GPR scratch(int index);

    void emitDispatcher();

    // Scan until a control flow instruction to see if anything uses the flags, or if anything modifies them.
    // If something uses the flags they are set to ABSOLUTELY_NEEDED. If nothing is found that uses them they remain at MAYBE_NEEDED.
    // If something overwrites them they are set to NOT_NEEDED.
    FlagsNeeded scanFlagUsageAhead(u64 rip, FlagsNeeded flags);

    ZydisMnemonic decode(u64 rip, ZydisDecodedInstruction& instruction, ZydisDecodedOperand* operands);

    Emulator& emulator;

    u8* code_cache{};
    biscuit::Assembler as{};
    ZydisDecoder decoder{};

    ZydisDecodedInstruction instruction{};
    ZydisDecodedOperand operands[10]{};

    void (*enter_dispatcher)(ThreadState*){};

    void* compile_next_handler{};

    // 16 gprs, 6 flags, 16 xmm registers
    std::array<RegisterMetadata, 16 + 6 + 16> metadata{};

    std::unordered_map<u64, std::pair<void*, u64>> map{};

    bool compiling{};
};