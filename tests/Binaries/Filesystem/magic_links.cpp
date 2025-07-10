#include <string>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "common.h"

#define FLAGS_SET(v, flags) ((~(v) & (flags)) == 0)
bool statx_inode_same(const struct statx* a, const struct statx* b) {
    return (a && a->stx_mask != 0) && (b && b->stx_mask != 0) && FLAGS_SET(a->stx_mask, STATX_TYPE | STATX_INO) &&
           FLAGS_SET(b->stx_mask, STATX_TYPE | STATX_INO) && ((a->stx_mode ^ b->stx_mode) & S_IFMT) == 0 && a->stx_dev_major == b->stx_dev_major &&
           a->stx_dev_minor == b->stx_dev_minor && a->stx_ino == b->stx_ino;
}

int main() {
    // Chromium does this to detect if we have namespaces
    // Our old path resolving would say file not found
    // This is because /proc/self/ns/user is a magic-link, which we shouldn't resolve ourselves and let the kernel handle
    int f = access("/proc/self/ns/user", F_OK);
    if (f != 0) {
        return 1;
    }

    int fd = open("/", O_PATH | O_DIRECTORY);
    if (fd <= 0) {
        return 2;
    }

    std::string fdpath = "/proc/self/fd/" + std::to_string(fd) + "/etc";

    struct statx stat1, stat2;
    int result = statx(AT_FDCWD, "/etc", AT_EMPTY_PATH, STATX_TYPE | STATX_INO | STATX_MNT_ID, &stat1);
    if (result != 0) {
        return 3;
    }

    result = statx(AT_FDCWD, fdpath.c_str(), AT_EMPTY_PATH, STATX_TYPE | STATX_INO | STATX_MNT_ID, &stat2);
    if (result != 0) {
        return 4;
    }

    if (!statx_inode_same(&stat1, &stat2)) {
        return 5;
    }

    return FELIX86_BTEST_SUCCESS;
}