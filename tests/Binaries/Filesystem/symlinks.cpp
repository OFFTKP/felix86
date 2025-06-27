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
        printf("No root permission?\n");
        return 1;
    }

    char buffer[PATH_MAX];
    ssize_t size = readlink(linked.c_str(), buffer, PATH_MAX);
    buffer[size] = 0;

    printf("Buffer: %s\n", buffer);

    if (std::string(buffer) != linked) {
        printf("Comparison failed\n");
        return 1;
    }

    return FELIX86_BTEST_SUCCESS;
}