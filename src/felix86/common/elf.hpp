#pragma once

#include <filesystem>
#include <map>
#include <vector>
#include "felix86/common/address.hpp"
#include "felix86/common/global.hpp"

struct Elf {
    enum class PeekResult {
        NotElf,
        Elf32,
        Elf64,
    };

    Elf(bool is_interpreter);
    ~Elf();

    void Load(const std::filesystem::path& path);

    // static void LoadSymbols(const std::string& name, const std::filesystem::path& path, void* base);

    bool Okay() const {
        return ok;
    }

    std::filesystem::path GetInterpreterPath() const {
        return interpreter;
    }

    GuestAddress GetEntrypoint() const {
        return HostAddress{(u64)(program_base + entry)}.toGuest();
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

    static PeekResult Peek(const std::filesystem::path& path);

    // Path is needed for static symbols, runtime address is needed for dynamic symbols
    static void AddSymbols(std::map<u64, Symbol>& symbols, const std::filesystem::path& path, u8* start, u8* end);

private:
    bool ok = false;
    bool is_interpreter = false;
    u64 entry = 0;
    std::filesystem::path interpreter{};

    u8* program_base = nullptr;
    u8* phdr = nullptr;
    u64 phnum = 0;
    u64 phent = 0;

    std::vector<std::pair<void*, size_t>> unmap_me;
};
