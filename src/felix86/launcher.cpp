#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
#include <grp.h>
#include <linux/limits.h>
#include <spawn.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "biscuit/cpuinfo.hpp"
#include "felix86/common/info.hpp"
#undef ERROR
#undef ASSERT

#define ERROR(format, ...)                                                                                                                           \
    {                                                                                                                                                \
        printf(format "\n", ##__VA_ARGS__);                                                                                                          \
        exit(1);                                                                                                                                     \
    }

#define ASSERT(condition, msg)                                                                                                                       \
    do {                                                                                                                                             \
        if (!(condition)) {                                                                                                                          \
            ERROR("Assertion failed: %s - %s", #condition, msg);                                                                                     \
        }                                                                                                                                            \
    } while (false)

std::vector<std::string> mounts;

void mountme(const char* path, const std::filesystem::path& dest, const char* fs_type, unsigned flags = 0) {
    std::filesystem::create_directories(dest);

    int result = mount(path, dest.c_str(), fs_type, flags, NULL);
    if (result < 0) {
        ERROR("Failed to mount %s to %s. Error: %d", path, dest.c_str(), errno);
    }
    printf("Mounting %s to %s\n", path, dest.c_str());

    mounts.push_back(dest);
}

// void copy_lib(const std::filesystem::path& lib, const std::filesystem::path& dest) {
//     if (std::filesystem::exists(dest / lib)) {
//         // Already there
//         return;
//     }

//     std::filesystem::path full_path = find_lib(lib);
//     if (full_path.empty()) {
//         ERROR("Library not found: %s", lib.c_str());
//     }

//     std::error_code ec;
//     std::filesystem::copy(full_path, dest / lib, ec);
//     if (ec) {
//         ERROR("Error while copying %s: %s", ec.message().c_str(), full_path.c_str());
//     }
// }

void copy_recursive(const char* dir_cstr, const std::filesystem::path& dest) {
    std::filesystem::path dir = dir_cstr;
    if (!std::filesystem::exists(dir)) {
        printf("I couldn't find %s to copy to the rootfs, may cause problems with some games", dir.c_str());
        return;
    }

    std::error_code ec;
    std::filesystem::copy(dir, dest, std::filesystem::copy_options::overwrite_existing | std::filesystem::copy_options::recursive, ec);
    if (ec) {
        ERROR("Error while copying %s: %s", dir.c_str(), ec.message().c_str());
    }
}

std::string version_full = get_version_full();

int print_version_stuff() {
    printf("%s\n", version_full.c_str());

    using namespace biscuit;
    biscuit::CPUInfo info;
    bool I = info.Has(Extension::I) && info.Has(Extension::M) && info.Has(Extension::A) && info.Has(Extension::F) && info.Has(Extension::D);
    bool V = info.Has(Extension::V);
    int len = 0;
    if (V) {
        len = info.GetVlenb();
    }

    if (!I) {
        printf("Is this really RISC-V?\n");
    } else {
        if (I && V && len >= 128) {
            printf("You have all the necessary extensions to use felix86!\n");
        } else if (!V) {
            printf("Your RISC-V system is missing the V extension!\n");
        }
    }

    std::vector<const char*> args = {"neofetch", "cpu", nullptr};

    pid_t pid;
    int status;
    int ok = posix_spawnp(&pid, "neofetch", nullptr, nullptr, (char**)args.data(), environ);
    if (ok != 0)
        goto error;
    waitpid(pid, &status, 0);

    args[1] = "gpu";
    ok = posix_spawnp(&pid, "neofetch", nullptr, nullptr, (char**)args.data(), environ);
    if (ok != 0)
        goto error;
    waitpid(pid, &status, 0);

    args[1] = "model";
    ok = posix_spawnp(&pid, "neofetch", nullptr, nullptr, (char**)args.data(), environ);
    if (ok != 0)
        goto error;
    waitpid(pid, &status, 0);

    args[1] = "distro";
    ok = posix_spawnp(&pid, "neofetch", nullptr, nullptr, (char**)args.data(), environ);
    if (ok != 0)
        goto error;
    waitpid(pid, &status, 0);

    args[1] = "de";
    ok = posix_spawnp(&pid, "neofetch", nullptr, nullptr, (char**)args.data(), environ);
    if (ok != 0)
        goto error;
    waitpid(pid, &status, 0);

    args[1] = "wm";
    ok = posix_spawnp(&pid, "neofetch", nullptr, nullptr, (char**)args.data(), environ);
    if (ok != 0)
        goto error;
    waitpid(pid, &status, 0);

    args[1] = "kernel";
    ok = posix_spawnp(&pid, "neofetch", nullptr, nullptr, (char**)args.data(), environ);
    if (ok != 0)
        goto error;
    waitpid(pid, &status, 0);

    args[1] = "memory";
    ok = posix_spawnp(&pid, "neofetch", nullptr, nullptr, (char**)args.data(), environ);
    if (ok != 0)
        goto error;
    waitpid(pid, &status, 0);

    return 0;

error:
    printf("Please install neofetch for more information\n");
    return ok;
}

int main(int argc, const char** argv) {
    if (argc < 2) {
        printf("Usage: ./felix86 <executable> <args to executable>\n");
        exit(1);
    }

    if (std::string(argv[1]) == "-v") {
        int ret = print_version_stuff();
        exit(ret);
    }

    const char* rootfs_env = getenv("FELIX86_ROOTFS");
    ASSERT(rootfs_env, "Please specify a rootfs path with the environment variable FELIX86_ROOTFS");

    if (geteuid() != 0) {
        // Try to restart app with sudo
        printf("I need administrator permissions to chroot and mount if necessary. Requesting administrator privileges...");
        std::vector<const char*> sudo_args = {"sudo"};
        sudo_args.push_back("-E");
        for (int i = 0; i < argc; i++) {
            sudo_args.push_back(argv[i]);
        }
        sudo_args.push_back(nullptr);
        execvpe("sudo", (char* const*)sudo_args.data(), environ);
        ERROR("felix86 needs administrator privileges to chroot and mount. Failed to restart felix86 with sudo. Please run felix86 with "
              "administrator privileges. Error code: %d",
              errno);
    }

    std::filesystem::path current_path;
    {
        char buffer[PATH_MAX];
        int size = readlink("/proc/self/exe", buffer, PATH_MAX);
        ASSERT(size != -1, "readlink failed");
        buffer[size] = 0;
        current_path = buffer;
    }

    const std::filesystem::path rootfs = rootfs_env;
    const std::filesystem::path libpath = rootfs / "felix86" / "lib";
    const std::filesystem::path felix_jit_path = current_path.parent_path() / "felix86_jit";
    ASSERT(std::filesystem::exists(felix_jit_path), "I couldn't find the `felix86_jit` executable, is it in the same directory as `felix86`?");

    // Copy every time to make rebuilding less painful
    std::filesystem::create_directories(libpath);
    chmod(libpath.c_str(), 0777);
    std::filesystem::copy(felix_jit_path, rootfs / "felix86", std::filesystem::copy_options::overwrite_existing);

    // These are the necessary libraries for felix86_jit. However, instead of copying them
    // we are just gonna mount /usr/lib
    // copy_lib("libstdc++.so.6", libpath);
    // copy_lib("libm.so.6", libpath);
    // copy_lib("libgcc_s.so.1", libpath);
    // copy_lib("libc.so.6", libpath);
    // copy_lib("ld-linux-riscv64-lp64d.so.1", libpath);

    copy_recursive("/var/lib/dbus", rootfs / "var" / "lib" / "dbus");
    copy_recursive("/etc/mtab", rootfs / "etc" / "mtab");
    copy_recursive("/etc/passwd", rootfs / "etc" / "passwd");
    copy_recursive("/etc/passwd-", rootfs / "etc" / "passwd");

    std::filesystem::path has_mounted_var_path = "/run/felix86.mounted";

    if (!std::filesystem::exists("/run") || !std::filesystem::is_directory("/run")) {
        ERROR("/run does not exist?");
    }

    int has_mounted_var = open(has_mounted_var_path.c_str(), 0, 0666);
    if (has_mounted_var != -1) {
        // This file was already created, which means a previous instance of felix86 mounted the directories
        close(has_mounted_var);
    } else {
        // Mount the necessary filesystems
        mountme("proc", rootfs / "proc", "proc");
        mountme("sysfs", rootfs / "sys", "sysfs");
        mountme("udev", rootfs / "dev", "devtmpfs");
        mountme("devpts", rootfs / "dev/pts", "devpts");
        mountme("/run", rootfs / "run", "none", MS_BIND | MS_REC);
        mountme("/tmp", rootfs / "tmp", "none", MS_BIND);       // mounting it for perf (the profiler)
        mountme("/usr/lib", libpath, "none", MS_BIND | MS_REC); // mount /usr/lib (host) to /felix86/lib (in rotfs)

        int fd = open(has_mounted_var_path.c_str(), O_CREAT | O_EXCL, 0666);
        if (fd == -1) {
            ERROR("Failed to create the mount variable file");
        } else {
            close(fd);
        }
    }

    int result = chroot(rootfs.c_str());
    if (result < 0) {
        ERROR("Failed to chroot to %s. Error: %d", rootfs.c_str(), errno);
        return 1;
    }

    ASSERT(getuid() == 0, "Does not have root?");

    result = chdir("/");
    if (result < 0) {
        ERROR("Failed to change directory to / after dropping root privileges. Error: %d", errno);
        return 1;
    }

    const char* allow_root_env = getenv("FELIX86_ALLOW_ROOT");
    bool allow_root = false;
    if (allow_root_env && std::string(allow_root_env) == "1") {
        allow_root = true;
    }

    if (!allow_root) {
        const char* gid_env = getenv("SUDO_GID");
        const char* uid_env = getenv("SUDO_UID");

        std::string suggestion = "If you want to run felix86 with root privileges (not recommended), "
                                 "set the FELIX86_ALLOW_ROOT environment variable to 1. Otherwise run without root privileges.";

        if (!uid_env || !gid_env) {
            ERROR("SUDO_UID or SUDO_GID not set, can't drop root privileges. %s", suggestion.c_str());
            return 1;
        }

        std::string user = getenv("SUDO_USER");
        gid_t gid = std::stoul(gid_env);
        uid_t uid = std::stoul(uid_env);

        if (initgroups(user.c_str(), gid) != 0) {
            ERROR("initgroups failed when trying to drop root privileges. %s", suggestion.c_str());
            return 1;
        }

        if (setgid(gid) != 0) {
            ERROR("setgid failed when trying to drop root privileges. %s", suggestion.c_str());
            return 1;
        }

        if (setuid(uid) != 0) {
            ERROR("setuid failed when trying to drop root privileges. %s", suggestion.c_str());
            return 1;
        }

        ASSERT(geteuid() != 0, "Failed to drop root privileges?");
        ASSERT(getuid() != 0, "Failed to drop root privileges?");

        // TODO: use this instead?
        // drop_capabilities();
    }

    constexpr static const char* jit_path_chroot = "/felix86/felix86_jit";
    ASSERT(std::filesystem::exists(jit_path_chroot), "felix86_jit not copied?");

    std::vector<const char*> jit_args;

    jit_args.push_back(jit_path_chroot);
    for (int i = 1; i < argc; i++) {
        jit_args.push_back(argv[i]);
    }
    jit_args.push_back(nullptr);

    constexpr static const char* launched = "__FELIX86_LAUNCHED=1";
    constexpr static const char* ld_lib_path = "LD_LIBRARY_PATH=/felix86/lib:/felix86/lib/riscv64-linux-gnu";
    char** environ_copy = environ;
    std::vector<const char*> jit_envs;
    while (*environ_copy) {
        jit_envs.push_back(*environ_copy);
        environ_copy++;
    }
    jit_envs.push_back(launched);
    jit_envs.push_back(ld_lib_path);
    jit_envs.push_back(nullptr);

    execvpe(jit_path_chroot, (char**)jit_args.data(), (char**)jit_envs.data());
}