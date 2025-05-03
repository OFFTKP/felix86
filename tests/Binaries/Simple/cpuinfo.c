#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include "common.h"

#define BUFFER_SIZE 16384 * 4

int main() {
    FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
    if (!cpuinfo) {
        return 1;
    }

    int proc = open("/proc", O_DIRECTORY);
    if (proc <= 0) {
        return 1;
    }

    int cpuinfo_fd = openat(proc, "cpuinfo", O_RDONLY);
    if (cpuinfo_fd <= 0) {
        return 1;
    }

    FILE* cpuinfo2 = fdopen(cpuinfo_fd, "r");
    if (!cpuinfo2) {
        return 1;
    }

    char buffer_fread[BUFFER_SIZE];
    char buffer_fread2[BUFFER_SIZE];

    size_t bytes_fread = fread(buffer_fread, 1, BUFFER_SIZE, cpuinfo);
    size_t bytes_fread2 = fread(buffer_fread2, 1, BUFFER_SIZE, cpuinfo2);

    if (bytes_fread == bytes_fread2) {
        return FELIX86_BTEST_SUCCESS;
    } else {
        return 1;
    }
}