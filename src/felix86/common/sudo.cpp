#include <cstdlib>
#include <vector>
#include <sys/mount.h>
#include <unistd.h>
#include "felix86/common/log.hpp"
#include "felix86/common/sudo.hpp"

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
