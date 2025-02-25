#pragma once

#include <filesystem>
#include <linux/limits.h>
#include "felix86/common/elf.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/utility.hpp"

struct Filesystem {
    bool LoadExecutable(const std::filesystem::path& path) {
        if (!executable_path.empty()) {
            ERROR("Executable already loaded");
            return false;
        }

        executable_path = path;

        elf = std::make_unique<Elf>(/* is_interpreter */ false);
        elf->Load(executable_path);

        if (!elf->Okay()) {
            ERROR("Failed to load ELF file %s", executable_path.c_str());
            return false;
        }

        std::filesystem::path interpreter_path = elf->GetInterpreterPath();
        if (!interpreter_path.empty()) {
            if (!interpreter_path.is_absolute()) {
                ERROR("Interpreter path %s is not absolute", interpreter_path.c_str());
                return false;
            }

            interpreter = std::make_unique<Elf>(/* is_interpreter */ true);
            interpreter->Load(interpreter_path);

            if (!interpreter->Okay()) {
                ERROR("Failed to load interpreter ELF file %s", interpreter_path.c_str());
                return false;
            }
        }

        const char* cwd = getenv("FELIX86_CWD");

        if (cwd) {
            int res = chdir(cwd);
            if (res == -1) {
                WARN("Failed to chdir to %s", cwd);
            }
        } else {
            int res = chdir(executable_path.parent_path().c_str());
            if (res == -1) {
                WARN("Failed to chdir to %s", executable_path.parent_path().c_str());
            }
        }

        return true;
    }

    GuestAddress GetEntrypoint() {
        if (interpreter) {
            return interpreter->GetEntrypoint();
        } else if (elf) {
            return elf->GetEntrypoint();
        } else {
            ERROR("No ELF file loaded");
            return {};
        }
    }

    std::shared_ptr<Elf> GetExecutable() {
        return elf;
    }

    std::shared_ptr<Elf> GetInterpreter() {
        return interpreter;
    }

    std::filesystem::path GetExecutablePath() {
        return executable_path;
    }

private:
    std::filesystem::path executable_path;
    std::shared_ptr<Elf> elf;
    std::shared_ptr<Elf> interpreter;
};