#include <filesystem>
#include <fcntl.h>
#include <linux/limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include "common.h"

void touch(const std::filesystem::path& path) {
    int fd = open(path.c_str(), O_CREAT | O_WRONLY, 0644);
    if (fd > 0) {
        close(fd);
    } else {
        printf("Failed to touch?\n");
        exit(1);
    }
}

int main() {
    // Unshare so we have chroot perms
    unshare(CLONE_NEWNS | CLONE_NEWUSER);

    char temp[] = "/tmp/felix86-fstest-XXXXXX";
    const char* cpath = mkdtemp(temp);
    std::filesystem::path dir = cpath;
    std::filesystem::path dir2 = dir / "tempdir";

    std::filesystem::path file = dir / "file.txt";

    if (mkdir(dir2.c_str(), 0777) != 0) {
        printf("Failed mkdir?\n");
        return 1;
    }

    touch(file);

    int dirfd = open(dir.c_str(), O_DIRECTORY | O_PATH);
    int ch = chroot(dir2.c_str());
    if (ch != 0) {
        return 4;
    }
    chdir("/");

    int fd = openat(dirfd, "file.txt", O_PATH);
    if (fd <= 0) {
        return 2;
    }

    int fd2 = open(file.c_str(), O_PATH);
    if (fd2 > 0) { // must error
        return 3;
    }

    return FELIX86_BTEST_SUCCESS;
}