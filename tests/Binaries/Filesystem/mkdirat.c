#define _GNU_SOURCE
#include <fcntl.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "common.h"

int main(void) {
    char template[] = "/tmp/testdir.XXXXXX";
    char* tmpdir = mkdtemp(template);
    if (!tmpdir) {
        perror("mkdtemp");
        return 2;
    }

    int dirfd = open(tmpdir, O_PATH | O_DIRECTORY);
    if (dirfd == -1) {
        perror("open");
        return 3;
    }

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/proc/self/fd/%d/foo.12345", dirfd);

    if (mkdirat(AT_FDCWD, path, 0755) == -1) {
        perror("mkdirat");
        close(dirfd);
        return 4;
    }

    int checkfd = open(path, O_RDONLY | O_DIRECTORY);
    if (checkfd == -1) {
        perror("open");
        close(dirfd);
        return 5;
    }

    close(checkfd);
    close(dirfd);
    return FELIX86_BTEST_SUCCESS;
}
