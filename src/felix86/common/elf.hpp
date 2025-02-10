#pragma once

#include <filesystem>
#include <memory>
#include <vector>
#include "felix86/common/utility.hpp"

constexpr u64 brk_size = 512 * 1024 * 1024;

struct Elf {
    Elf(bool is_interpreter);

    ~Elf();

    void Load(const std::filesystem::path& path);

    static void LoadSymbols(const std::string& name, const std::filesystem::path& path, void* base);

    bool Okay() const {
        return ok;
    }

    std::filesystem::path GetInterpreterPath() const {
        return interpreter;
    }

    void* GetEntrypoint() const {
        return (void*)(program + entry);
    }

    void* GetStackPointer() const {
        return stack_pointer;
    }

    void* GetProgramBase() const {
        return program;
    }

    void* GetPhdr() const {
        return phdr;
    }

    u64 GetPhnum() const {
        return phnum;
    }

    u64 GetPhent() const {
        return phent;
    }

    auto& GetExecutableSegments() {
        return executable_segments;
    }

private:
    bool ok = false;
    bool is_interpreter = false;
    u8* program = nullptr;
    std::vector<std::pair<u8*, u64>> executable_segments{};
    u64 entry = 0;
    std::filesystem::path interpreter{};
    u8* stack_pointer = nullptr;

    u8* phdr = nullptr;
    u64 phnum = 0;
    u64 phent = 0;
};
