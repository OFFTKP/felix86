#pragma once

#include <memory>
#include "felix86/common/utility.hpp"

struct Elf {
    Elf();
    ~Elf();

    u8* program;
    u64 entry;
    std::string interpreter;
    u8* stack_base;
    u8* stack_pointer;
    u8* brk_base;

    u8* phdr;
    u64 phnum;
    u64 phent;
};

std::unique_ptr<Elf> elf_load(const std::string& path, bool is_interpreter);
