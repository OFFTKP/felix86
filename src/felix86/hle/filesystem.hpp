#pragma once

#include <filesystem>
#include <optional>
#include <linux/limits.h>
#include "felix86/common/elf.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/utility.hpp"

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

            std::filesystem::path interpreter_path_sandboxed = rootfs_path / interpreter_path.lexically_normal();
            interpreter = std::make_unique<Elf>(/* is_interpreter */ true);
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
        } else if (elf) {
            return elf->GetEntrypoint();
        } else {
            ERROR("No ELF file loaded");
            return nullptr;
        }
    }

    ssize_t ReadLinkAt(int dirfd, const char* pathname, char* buf, u32 bufsiz);

    ssize_t ReadLink(const char* pathname, char* buf, u32 bufsiz);

    int FAccessAt(int dirfd, const char* pathname, int mode, int flags);

    int OpenAt(int dirfd, const char* pathname, int flags, int mode);

    int Chdir(const char* pathname);

    int GetCwd(char* buf, u32 bufsiz);

    int Statx(int dirfd, const char* pathname, int flags, int mask, struct statx* statxbuf);

    std::optional<std::filesystem::path> AtPath(int dirfd, const char* pathname);

    std::filesystem::path GetRootFSPath() {
        return rootfs_path;
    }

    std::shared_ptr<Elf> GetExecutable() {
        return elf;
    }

    std::shared_ptr<Elf> GetInterpreter() {
        return interpreter;
    }

    u64 GetBRK() {
        return elf->GetBRK();
    }

    int Error() {
        return error;
    }

private:
    bool validatePath(const std::filesystem::path& path);

    std::filesystem::path rootfs_path;
    std::string rootfs_path_string;
    std::filesystem::path executable_path;
    std::filesystem::path cwd_path;
    std::shared_ptr<Elf> elf;
    std::shared_ptr<Elf> interpreter;
    int error = 0;
};