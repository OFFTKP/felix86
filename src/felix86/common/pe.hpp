#pragma once

#include <filesystem>

struct PE {
    enum class PeekResult {
        NotPE,
        PE_i386,
        PE_x64,
    };

    static PeekResult Peek(const std::filesystem::path& path);
};