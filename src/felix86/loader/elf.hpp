#pragma once

#include <memory>
#include "felix86/common/utility.hpp"

struct Elf {
    ~Elf();

    u8* program = nullptr;
    u64 entry = 0;
    std::string interpreter {};
    u8* stack_base = nullptr;
    u8* stack_pointer = nullptr;
    u8* brk_base = nullptr;

    u8* phdr = nullptr;
    u64 phnum = 0;
    u64 phent = 0;
};

std::unique_ptr<Elf> elf_load(const std::string& path, bool is_interpreter);
