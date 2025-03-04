#pragma once

#include <filesystem>

struct Script {
    enum class PeekResult {
        NotScript,
        Script,
    };

    Script(const std::filesystem::path& script);

    static PeekResult Peek(const std::filesystem::path& path);

    const std::filesystem::path& GetInterpreter() const {
        return interpreter;
    }

    const std::string& GetArgs() const {
        return args;
    }

private:
    std::filesystem::path interpreter;
    std::string args;
};