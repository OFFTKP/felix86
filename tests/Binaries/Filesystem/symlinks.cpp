#include <filesystem>
#include <linux/limits.h>
#include <stdlib.h>
#include <unistd.h>
#include "common.h"

int main() {
    // Unshare so we have chroot perms
    unshare(CLONE_NEWNS | CLONE_NEWUSER);

    char temp[] = "/tmp/felix86-fstest-XXXXXX";
    const char* cpath = mkdtemp(temp);
    std::filesystem::path dir = cpath;
    std::filesystem::path original = dir / "original";
    std::filesystem::path linked = dir / "linked";
    if (symlink(original.c_str(), linked.c_str()) != 0) {
        printf("Failed symlink?\n");
        return 1;
    }

    if (chroot(cpath) != 0) {
        printf("No root permission: %d?\n", errno);
        return 1;
    }

    chdir("/");

    char buffer[PATH_MAX];
    ssize_t size = readlink("./linked", buffer, PATH_MAX);
    if (size < 0) {
        printf("Error: %d\n", errno);
        return 1;
    }

    buffer[size] = 0;

    if (std::string(buffer) != original) {
        printf("Comparison failed\n");
        return 1;
    }

    return FELIX86_BTEST_SUCCESS;
}