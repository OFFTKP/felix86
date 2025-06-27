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
    if (mkdir(dir2.c_str(), 0777) != 0) {
        printf("Failed mkdir?\n");
        return 1;
    }

    touch(dir / "file1_felix86");
    touch(dir2 / "file2_felix86");

    chdir("/");

    char buffer1[PATH_MAX];
    char* cwd1 = getcwd(buffer1, PATH_MAX);
    if (std::string(cwd1) != "/") {
        printf("Bad cwd1?\n");
        return 1;
    }

    if (::chroot(cpath) != 0) {
        printf("No perms?\n");
        return 1;
    }

    chdir("/");

    char buffer2[PATH_MAX];
    char* cwd2 = getcwd(buffer2, PATH_MAX);
    if (std::string(cwd2) != "/") {
        printf("Bad cwd2? %s\n", cwd2);
        return 1;
    }

    int fd = open("/file1_felix86", O_RDONLY, 0644);
    if (fd <= 0) {
        printf("Failed to open /file1_felix86?\n");
        return 1;
    }
    close(fd);

    if (::chroot("/tempdir") != 0) {
        printf("Failed to chroot to /tempdir?\n");
        return 1;
    }

    chdir("/");

    fd = open("/file2_felix86", O_RDONLY, 0644);
    if (fd <= 0) {
        printf("Failed to open /file2_felix86?\n");
        return 1;
    }
    close(fd);

    return FELIX86_BTEST_SUCCESS;
}