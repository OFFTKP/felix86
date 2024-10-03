#pragma once

#include <memory>
#include <vector>
#include "felix86/common/utility.hpp"

struct Elf {
    ~Elf();

    u8* program = nullptr;
    std::vector<std::pair<u8*, u64>> executable_segments{};
    u64 entry = 0;
    std::string interpreter{};
    u8* stack_base = nullptr;
    u8* stack_pointer = nullptr;
    u8* brk_base = nullptr;

    u8* phdr = nullptr;
    u64 phnum = 0;
    u64 phent = 0;
};

std::unique_ptr<Elf> elf_load(const std::string& path, bool is_interpreter);
