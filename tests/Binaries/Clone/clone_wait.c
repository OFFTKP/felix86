#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <linux/sched.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include "common.h"

int clone_handler(void* memory) {
    printf("Hello from handler\n");
    return FELIX86_BTEST_SUCCESS;
}

int main() {
    // Spawn child and wait for exit
    void* stack = malloc(1024);
    int pid = clone(clone_handler, stack, CLONE_VM, NULL);
    if (pid == -1) {
        printf("clone failed\n");
        return 1;
    }

    int status;
    waitpid(pid, &status, 0);
    printf("Child exited with status %d\n", status);

    if (WEXITSTATUS(status) != FELIX86_BTEST_SUCCESS) {
        printf("Bad exit status: First\n");
        return 1;
    }

    // Now without stack
    pid = clone(clone_handler, NULL, 0, NULL);
    if (pid == -1) {
        printf("clone failed\n");
        return 1;
    }

    waitpid(pid, &status, 0);
    printf("Child exited with status %d\n", status);

    if (WEXITSTATUS(status) != FELIX86_BTEST_SUCCESS) {
        printf("Bad exit status: Second\n");
        return 1;
    }

    return FELIX86_BTEST_SUCCESS;
}