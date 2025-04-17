#include <sys/mount.h>
#include "felix86/common/sudo.hpp"

// When the system is rebooted, files in /run are deleted -- and mounts are also unmounted
const static std::filesystem::path mounted_path = "/run/felix86.mounted";

bool Sudo::hasPermissions() {
    return geteuid() == 0;
}

void Sudo::requestPermissions(int argc, char** argv) {
    std::vector<const char*> sudo_args = {"sudo"};
    sudo_args.push_back("-E");
    for (int i = 0; i < argc; i++) {
        sudo_args.push_back(argv[i]);
    }
    sudo_args.push_back(nullptr);
    execvpe("sudo", (char* const*)sudo_args.data(), environ);
    ERROR("Failed to elevate permissions");
    __builtin_unreachable();
}

bool Sudo::dropPermissions() {
    const char* gid_env = getenv("SUDO_GID");
    const char* uid_env = getenv("SUDO_UID");

    if (!uid_env || !gid_env) {
        WARN("SUDO_UID or SUDO_GID not set, can't drop root privileges");
        return false;
    }

    std::string user = getenv("SUDO_USER");
    gid_t gid = std::stoul(gid_env);
    uid_t uid = std::stoul(uid_env);

    if (initgroups(user.c_str(), gid) != 0) {
        WARN("initgroups failed when trying to drop root privileges");
        return false;
    }

    if (setgid(gid) != 0) {
        WARN("setgid failed when trying to drop root privileges");
        return false;
    }

    if (setuid(uid) != 0) {
        WARN("setuid failed when trying to drop root privileges");
        return false;
    }

    ASSERT_MSG(geteuid() != 0, "Failed to drop root privileges?");
    ASSERT_MSG(getuid() != 0, "Failed to drop root privileges?");
    return true;
}

void Sudo::mount(const char* path, const std::filesystem::path& dest, const char* fs_type, u32 flags) {
    std::filesystem::create_directories(dest);

    int result = ::mount(path, dest.c_str(), fs_type, flags, nullptr);
    if (result < 0) {
        ERROR("Failed to mount %s to %s. Error: %d", path, dest.c_str(), errno);
    }

    LOG("Mounting %s to %s", path, dest.c_str());
}

bool Sudo::isMounted() {
    return std::filesystem::exists(mounted_path);
}

bool Sudo::chroot(const std::filesystem::path& path) {
    VERBOSE("Chrooting into %s", path.c_str());
    return ::chroot(path.c_str()) == 0;
}