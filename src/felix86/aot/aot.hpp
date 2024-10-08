#pragma once

#include <unordered_set>
#include "Zycore/Status.h"
#include "Zydis/Decoder.h"
#include "Zydis/DecoderTypes.h"
#include "felix86/common/elf.hpp"
#include "felix86/common/utility.hpp"

// These analyses find jump targets and add them to the addresses set
// They are not very good but they can find some stuff for now
struct AOT {
    AOT(const Elf& elf);

    // Follow the control flow of the executable (follow cond/uncond branches) to find call instructions
    void ControlFlowAnalysis();

    // Scan the executable for instructions that might appear at the start of a function on x86-64
    void FunctionStartFinder();

private:
    const Elf& elf;
    std::unordered_set<u64> addresses;
    static ZydisDecoder decoder;
    static ZyanStatus decodeInstruction(ZydisDecodedInstruction& inst, ZydisDecodedOperand* operands, u8* data, u64 size = 20);
};