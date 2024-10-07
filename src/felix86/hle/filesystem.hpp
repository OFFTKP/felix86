#pragma once

#include <filesystem>
#include <linux/limits.h>
#include "felix86/common/log.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/loader/elf.hpp"

struct Filesystem {
    Filesystem() = default;

    bool LoadRootFS(const std::filesystem::path& path);

    bool LoadExecutable(const std::filesystem::path& path) {
        if (!executable_path.empty()) {
            ERROR("Executable already loaded");
            return false;
        }

        if (!validatePath(path)) {
            ERROR("Invalid executable path %s", path.c_str());
            return false;
        }

        executable_path = path;

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

            std::filesystem::path interpreter_path_sandboxed = rootfs_path / interpreter_path.lexically_normal();
            interpreter->Load(interpreter_path_sandboxed);
            if (!interpreter->Okay()) {
                ERROR("Failed to load interpreter ELF file %s", interpreter_path_sandboxed.c_str());
                return false;
            }
        }

        return true;
    }

    void* GetEntrypoint() {
        if (interpreter) {
            return interpreter->GetEntrypoint();
        } else {
            return elf->GetEntrypoint();
        }
    }

    ssize_t ReadLinkAt(u32 dirfd, const char* pathname, char* buf, u32 bufsiz);

    ssize_t ReadLink(const char* pathname, char* buf, u32 bufsiz);

    std::filesystem::path GetRootFSPath() {
        return rootfs_path;
    }

private:
    bool validatePath(const std::filesystem::path& path);

    std::filesystem::path rootfs_path;
    std::string rootfs_path_string;
    std::filesystem::path executable_path;
    std::filesystem::path cwd_path;
    std::unique_ptr<Elf> elf;
    std::unique_ptr<Elf> interpreter;
};