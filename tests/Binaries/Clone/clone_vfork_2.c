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

int main() {
    int pid = vfork();
    if (pid == 0) {
        // vfork process
        _exit(FELIX86_BTEST_SUCCESS);
        perror("exit"); // Should never reach here
    }

    // Parent process
    int status;
    waitpid(pid, &status, 0);

    int exit_status = WEXITSTATUS(status);
    if (exit_status == FELIX86_BTEST_SUCCESS) {
        return FELIX86_BTEST_SUCCESS;
    } else {
        printf("Bad exit status: %d\n", exit_status);
        return 1;
    }
}