#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <grp.h>
#include <sys/mount.h>
#include <unistd.h>

#define ERROR(format, ...)                                                                                                                           \
    {                                                                                                                                                \
        printf(format, ##__VA_ARGS__);                                                                                                               \
        exit(1);                                                                                                                                     \
    }

#define ASSERT(condition, msg)                                                                                                                       \
    do {                                                                                                                                             \
        if (!(condition)) {                                                                                                                          \
            ERROR("Assertion failed: %s - %s", #condition, msg);                                                                                     \
        }                                                                                                                                            \
    } while (false)

std::filesystem::path find_lib(const std::filesystem::path& lib) {
#define CHECK(dir)                                                                                                                                   \
    if (std::filesystem::exists(dir / lib)) {                                                                                                        \
        return dir / lib;                                                                                                                            \
    }

    CHECK("/lib64")
    CHECK("/usr/lib")
    CHECK("/lib")

    return "";
}

std::vector<std::string> mounts;

void mountme(const char* path, const std::filesystem::path& dest, const char* fs_type, unsigned flags = 0) {
    std::filesystem::create_directories(dest);

    int result = mount(path, dest.c_str(), fs_type, flags, NULL);
    if (result < 0) {
        ERROR("Failed to mount %s to %s. Error: %d", path, dest.c_str(), errno);
    }
    printf("Mounting %s to %s", path, dest.c_str());

    mounts.push_back(dest);
}

void copy_lib(const std::filesystem::path& lib, const std::filesystem::path& rootfs) {
    if (std::filesystem::exists(rootfs / lib)) {
        // Already there
        return;
    }

    std::filesystem::path full_path = find_lib(lib);
    if (full_path.empty()) {
        ERROR("Library not found: %s", lib.c_str());
    }

    std::error_code ec;
    std::filesystem::copy(full_path, rootfs / "felix86" / "lib" / lib, ec);
    if (ec) {
        ERROR("Error while copying: %s", ec.message().c_str());
    }
}

int main(int argc, const char** argv) {
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
        std::ifstream ifs("/proc/self/exe");
        std::string line;
        std::getline(ifs, line);
        current_path = line;
    }

    const std::filesystem::path rootfs = rootfs_env;
    const std::filesystem::path felix_jit_path = current_path.parent_path() / "felix86_jit";

    // Copy every time to make rebuilding less painful
    std::filesystem::copy(felix_jit_path, rootfs);

    copy_lib("libstdc++.so.6", rootfs);
    copy_lib("libm.so.6", rootfs);
    copy_lib("libgcc_s.so.1", rootfs);
    copy_lib("libc.so.6", rootfs);

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
        mountme("/tmp", rootfs / "tmp", "none", MS_BIND); // mounting it for perf

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

    // Pass all the args, except the path to the loader and `sudo -E` if it was added
    int index = 0;
    for (; index < argc; index++) {
        std::string arg = argv[index];
        if (arg == "sudo" && index == 0) {
            ASSERT(std::string(argv[index + 1]) == "-E", "Unexpected argument after sudo");
            index += 2;
        } else {
            // Skip loader path
            index++;
            break;
        }
    }

    jit_args.push_back(jit_path_chroot);
    for (int i = index; i < argc; i++) {
        jit_args.push_back(argv[i]);
    }
    jit_args.push_back(nullptr);

    constexpr static const char* launched = "__FELIX86_LAUNCHED";
    char** environ_copy = environ;
    std::vector<const char*> jit_envs;
    while (*environ_copy) {
        jit_envs.push_back(*environ_copy);
        environ_copy++;
    }
    jit_envs.push_back(launched);
    jit_envs.push_back(nullptr);

    execvpe(jit_path_chroot, (char**)jit_args.data(), (char**)jit_envs.data());
}