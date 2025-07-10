#pragma once

#include <filesystem>
#include <functional>
#include <fcntl.h>
#include <linux/limits.h>
#include <linux/stat.h>
#include "felix86/common/elf.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/types.hpp"

// Like path but can also be null
struct NullablePath {
    NullablePath() : is_null(true) {}
    NullablePath(const char* path) {
        if (!path) {
            is_null = true;
        } else {
            this->path = path;
        }
    }
    NullablePath(const std::filesystem::path& path) : path(path) {}

    const char* get_str() {
        if (is_null) {
            return nullptr;
        } else {
            return path.c_str();
        }
    }

private:
    std::filesystem::path path{};
    bool is_null = false;
};

struct FdPath {
    static FdPath create(int fd, const NullablePath& path) {
        ASSERT(fd == AT_FDCWD || fd >= 0);
        FdPath ret;
        ret.fd_path = std::make_pair(fd, path);
        return ret;
    }

    static FdPath error(int error) {
        ASSERT(error > 0);
        FdPath ret;
        ret.inner_errno = error;
        return ret;
    }

    int fd() {
        return fd_path.first;
    }

    const char* path() {
        return fd_path.second.get_str();
    }

    const char* full_path() {
        bool is_absolute = fd_path.second.get_str() && fd_path.second.get_str()[0] == '/';
        if (is_absolute) {
            return path();
        } else {
            // We may need to query /proc/self/fd to get a full path
            if (fd() == AT_FDCWD) {
                char buffer[4096];
                char* cwd = getcwd(buffer, PATH_MAX);
                ASSERT(cwd == buffer);
                std::filesystem::path new_path = cwd;
                if (fd_path.second.get_str()) {
                    new_path /= fd_path.second.get_str();
                }
                fd_path.second = new_path;
                ASSERT_MSG(new_path.is_absolute(), "Path: %s / %s", buffer, fd_path.second.get_str());
                fd_path.second = new_path;
                return path();
            } else {
                int fd = fd_path.first;
                fd_path.first = AT_FDCWD;
                std::filesystem::path proc_fd = "/proc/self/fd";
                proc_fd /= std::to_string(fd);
                char buffer[4096];
                int result = readlink(proc_fd.c_str(), buffer, PATH_MAX - 1);
                ASSERT(result > 0);
                buffer[result] = 0;
                std::filesystem::path new_path = buffer;
                if (fd_path.second.get_str()) {
                    new_path /= fd_path.second.get_str();
                }
                if (!new_path.is_absolute()) {
                    printf("ATTACH ME: %d\n", gettid());
                    sleep(500);
                }
                ASSERT_MSG(new_path.is_absolute(), "Path: %s / %s", buffer, fd_path.second.get_str());
                fd_path.second = new_path;
                return path();
            }
        }
    }

    bool is_error() {
        return inner_errno != 0;
    }

    int get_errno() {
        return inner_errno;
    }

private:
    std::pair<int, NullablePath> fd_path;
    int inner_errno = 0;
};

struct Filesystem {
    void initializeEmulatedNodes();

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

        if (!elf->GetInterpreterPath().empty()) {
            FdPath fd_path = Filesystem::resolve(elf->GetInterpreterPath().c_str(), true);
            ASSERT(fd_path.full_path());
            std::filesystem::path interpreter_path = fd_path.full_path();
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

    u64 GetEntrypoint() {
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
    int OpenAt(int fd, const char* filename, int flags, u64 mode);

    static int FAccessAt(int fd, const char* filename, int mode, int flags);

    static int FStatAt(int fd, const char* filename, struct stat* host_stat, int flags);

    static int FStatAt64(int fd, const char* filename, struct stat64* host_stat, int flags);

    static int StatFs(const char* path, struct statfs* buf);

    static int ReadlinkAt(int fd, const char* filename, char* buf, int bufsiz);

    static int SymlinkAt(const char* oldname, int newfd, const char* newname);

    static int RenameAt2(int oldfd, const char* oldname, int newfd, const char* newname, int flags);

    static int Chmod(const char* path, u64 mode);

    static int Statx(int fd, const char* filename, int flags, u32 mask, struct statx* statxbuf);

    static int UnlinkAt(int fd, const char* path, int flags);

    static int LinkAt(int oldfd, const char* oldpath, int newfd, const char* newpath, int flags);

    static int Chown(const char* filename, u64 owner, u64 group);

    static int LChown(const char* filename, u64 owner, u64 group);

    static int Creat(const char* filename, u64 mode);

    static int Chdir(const char* filename);

    static int MkdirAt(int fd, const char* filename, u64 mode);

    static int MknodAt(int fd, const char* filename, u64 mode, u64 dev);

    static int Getcwd(char* buf, size_t size);

    static int GetXAttr(const char* filename, const char* name, void* value, size_t size);

    static int LGetXAttr(const char* filename, const char* name, void* value, size_t size);

    static int SetXAttr(const char* filename, const char* name, void* value, size_t size, int flags);

    static int LSetXAttr(const char* filename, const char* name, void* value, size_t size, int flags);

    static int RemoveXAttr(const char* filename, const char* name);

    static int LRemoveXAttr(const char* filename, const char* name);

    static int UtimensAt(int fd, const char* filename, struct timespec* spec, int flags);

    static int FChmodAt(int fd, const char* filename, u64 mode);

    static int Rmdir(const char* path);

    static int Chroot(const char* path);

    static int Mount(const char* source, const char* target, const char* fstype, u64 flags, const void* data);

    static int Umount(const char* path, int flags);

    static int INotifyAddWatch(int fd, const char* path, u32 mask);

    static int Truncate(const char* path, u64 length);

    static ssize_t Listxattr(const char* path, char* list, size_t size, bool llist);

    static FdPath resolve(const char* path, bool resolve_symlinks);

    static FdPath resolve(int fd, const char* path, bool resolve_symlinks);

    static void removeRootfsPrefix(std::string& path);

private:
    int openatInternal(int fd, const char* filename, int flags, u64 mode);

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

    static int rmdirInternal(const char* path);

    static bool isProcSelfExe(const char* path);

    static FdPath resolveImpl(int fd, const char* path, bool resolve_final);

    static std::pair<int, NullablePath> resolveImplOld(int fd, const char* path, bool resolve_symlinks);

    std::filesystem::path executable_path;
    std::shared_ptr<Elf> elf;
    std::shared_ptr<Elf> interpreter;

    struct EmulatedNode {
        std::filesystem::path path;

        // The statx of the actual file for comparison
        struct statx stat{};

        std::function<int(const char* path, int flags)> open_func{};
    };

    enum {
        PROC_CPUINFO,
        PROC_SELF_MAPS,
        EMULATED_NODE_COUNT,
    };

    std::array<EmulatedNode, EMULATED_NODE_COUNT> emulated_nodes;
};