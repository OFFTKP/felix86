#pragma once

#include <filesystem>
#include <optional>
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

        auto elf2 = std::make_unique<Elf>(/* is_interpreter */ false);
        elf2->LoadOld(executable_path);

        u64 start_old = g_executable_start;
        u64 end_old = g_executable_end;

        elf = std::make_unique<Elf>(/* is_interpreter */ false);
        elf->Load(executable_path);

        u64 start = g_executable_start;
        u64 end = g_executable_end;

        u64 size = end - start;
        u64 size_old = end_old - start_old;

        if (size != size_old) {
            ERROR("Size mismatch between old and new ELF loader: %lu vs %lu", size, size_old);
            return false;
        }

        for (u64 i = 0; i < size; i++) {
            u8* ptr = (u8*)start + i;
            u8* ptr_old = (u8*)start_old + i;
            if (*ptr != *ptr_old) {
                ERROR("Data mismatch between old and new ELF loader at offset %lu: %02x vs %02x", i, *ptr, *ptr_old);
                return false;
            }
        }

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

    void* GetEntrypoint() {
        if (interpreter) {
            return interpreter->GetEntrypoint();
        } else if (elf) {
            return elf->GetEntrypoint();
        } else {
            ERROR("No ELF file loaded");
            return nullptr;
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