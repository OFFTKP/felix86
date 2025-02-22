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
    static const char* argv[] = {
        "/usr/bin/ls",
        0,
    };

    int pid = vfork();
    if (pid == 0) {
        // vfork process
        // We must immediatelly execvpe without hurting the parent process
        execve(argv[0], (char* const*)argv, (char* const*)environ);
        perror("execvpe"); // Should never reach here
    }

    // Parent process
    int status;
    waitpid(pid, &status, 0);

    int exit_status = WEXITSTATUS(status);
    if (exit_status == 0) {
        return FELIX86_BTEST_SUCCESS;
    } else {
        return 1;
    }
}