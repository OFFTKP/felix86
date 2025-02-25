#pragma once

#include <filesystem>

struct Script {
    enum class PeekResult {
        NotScript,
        Script,
    };

    Script(const std::filesystem::path& script);

    PeekResult static Peek(const std::filesystem::path& path);

private:
    std::filesystem::path interpreter;
    std::filesystem::path script;
};