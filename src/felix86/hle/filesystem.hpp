#pragma once

#include <filesystem>
#include <linux/limits.h>
#include "felix86/common/elf.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/hle/guest_types.hpp"

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

    const std::filesystem::path& GetExecutablePath() {
        return executable_path;
    }

    // Emulated syscall functions
    static int OpenAt(int fd, const char* filename, int flags, u64 mode);

    static int FAccessAt(int fd, const char* filename, int mode, int flags);

    static int FStatAt(int fd, const char* filename, x86_stat* guest_stat, int flags);

    static int StatFs(const char* path, struct statfs* buf);

    static int ReadlinkAt(int fd, const char* filename, char* buf, int bufsiz);

    static int Rename(const char* oldname, const char* newname);

    static int Symlink(const char* oldname, const char* newname);

    static int Chmod(const char* path, u64 mode);

    static int Statx(int fd, const char* filename, int flags, u32 mask, struct statx* statxbuf);

    static int UnlinkAt(int fd, const char* path, int flags);

    static int LinkAt(int oldfd, const char* oldpath, int newfd, const char* newpath, int flags);

    static int Chown(const char* filename, u64 owner, u64 group);

    static int Chdir(const char* filename);

    static int Mkdir(const char* filename, u64 mode);

    static int Getcwd(char* buf, size_t size);

    static int GetXAttr(const char* filename, const char* name, void* value, size_t size);

    static int LGetXAttr(const char* filename, const char* name, void* value, size_t size);

    static int SetXAttr(const char* filename, const char* name, void* value, size_t size, int flags);

    static int LSetXAttr(const char* filename, const char* name, void* value, size_t size, int flags);

    static int RemoveXAttr(const char* filename, const char* name);

    static int LRemoveXAttr(const char* filename, const char* name);

    static int UtimensAt(int fd, const char* filename, struct timespec* spec, int flags);

    static int FChmodAt(int fd, const char* filename, u64 mode);

    static std::filesystem::path resolve(const char* path);

    static void removeRootfsPrefix(std::string& path);

private:
    static int openatInternal(int fd, const char* filename, int flags, u64 mode);

    static int faccessatInternal(int fd, const char* filename, int mode, int flags);

    static int fstatatInternal(int fd, const char* filename, struct stat* host_stat, int flags);

    static int statfsInternal(const std::filesystem::path& path, struct statfs* buf);

    static int readlinkatInternal(int fd, const char* filename, char* buf, int bufsiz);

    static int statxInternal(int fd, const char* filename, int flags, u32 mask, struct statx* statxbuf);

    static int linkatInternal(int oldfd, const char* oldpath, int newfd, const char* newpath, int flags);

    static int unlinkatInternal(int fd, const char* filename, int flags);

    static int getxattrInternal(const char* filename, const char* name, void* value, size_t size);

    static int lgetxattrInternal(const char* filename, const char* name, void* value, size_t size);

    static int setxattrInternal(const char* filename, const char* name, void* value, size_t size, int flags);

    static int lsetxattrInternal(const char* filename, const char* name, void* value, size_t size, int flags);

    static int removexattrInternal(const char* filename, const char* name);

    static int lremovexattrInternal(const char* filename, const char* name);

    static int utimensatInternal(int fd, const char* filename, struct timespec* spec, int flags);

    static int fchmodatInternal(int fd, const char* filename, u64 mode);

    static std::pair<int, const char*> resolve(int fd, const char* path);

    static bool isProcSelfExe(const char* path);

    std::filesystem::path executable_path;
    std::shared_ptr<Elf> elf;
    std::shared_ptr<Elf> interpreter;
};