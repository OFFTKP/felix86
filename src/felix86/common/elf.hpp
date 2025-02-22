#pragma once

#include <filesystem>
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/utility.hpp"

constexpr u64 brk_size = 512 * 1024 * 1024;

struct Elf {
    Elf(bool is_interpreter);

    ~Elf();

    void Load(const std::filesystem::path& path);

    // static void LoadSymbols(const std::string& name, const std::filesystem::path& path, void* base);

    bool Okay() const {
        return ok;
    }

    std::filesystem::path GetInterpreterPath() const {
        std::filesystem::path result;
        if (g_testing) {
            ASSERT(!g_rootfs_path.empty());
            result = g_rootfs_path / interpreter; // not chrooted, need to look in rootfs
        } else {
            result = interpreter;
        }
        return result;
    }

    void* GetEntrypoint() const {
        return program_base + entry;
    }

    void* GetStackPointer() const {
        return stack_pointer;
    }

    void* GetProgramBase() const {
        return program_base;
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

private:
    bool ok = false;
    bool is_interpreter = false;
    u64 entry = 0;
    std::filesystem::path interpreter{};
    u8* stack_pointer = nullptr;

    u8* phdr = nullptr;
    u8* program_base = nullptr;
    u64 phnum = 0;
    u64 phent = 0;
};
