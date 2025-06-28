#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include "common.h"

int main() {
    const char* home = getenv("HOME");
    int fd = open(home, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    if (futimens(fd, NULL) < 0) {
        perror("futimens");
        close(fd);
        return 1;
    }

    close(fd);
    return FELIX86_BTEST_SUCCESS;
}