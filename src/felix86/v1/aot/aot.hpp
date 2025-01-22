#pragma once

#include <unordered_set>
#include "felix86/common/elf.hpp"
#include "felix86/common/utility.hpp"

struct Emulator;

// These analyses find jump targets and add them to the addresses set
// They are not very good but they can find some stuff for now
struct AOT {
    AOT(Emulator& emulator, std::shared_ptr<Elf> elf);
    AOT(const AOT&) = delete;
    AOT& operator=(const AOT&) = delete;
    AOT(AOT&&) = delete;
    AOT& operator=(AOT&&) = delete;
    ~AOT() = default;

    void PreloadAll();

    void CompileAll();

private:
    // Follow the control flow of the executable (follow cond/uncond branches) to find call instructions
    void controlFlowAnalysis();

    // Scan the executable for instructions that might appear at the start of a function on x86-64
    void functionStartFinder();

    void runAnalysis();

    Emulator& emulator;
    bool analyzed = false;
    std::shared_ptr<Elf> elf;
    std::unordered_set<u64> addresses;
};