#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "common.h"

extern char** environ;

static void dump_child_stdout(char* buffer, int filedes) {
    ssize_t num_read;
    char buf[1];

    int i = 0;
    for (;;) {
        num_read = read(filedes, buf, sizeof(buf));
        if (num_read > 0) {
            sprintf(buffer + i, "%c", buf[0]);
        } else {
            break;
        }
        i++;
    }
}

int main(int argc, char* argv[]) {
    int status;
    pid_t pid;
    int out[2];
    posix_spawn_file_actions_t action;
    char* args[] = {"/usr/bin/echo", "Hello World!", NULL};

    posix_spawn_file_actions_init(&action);

    pipe(out);

    posix_spawn_file_actions_adddup2(&action, out[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&action, out[0]);

    char buffer[4096];
    memset(buffer, 0, 4096);
    status = posix_spawn(&pid, args[0], &action, NULL, args, environ);
    if (status == 0) {
        printf("child pid: %d\n", pid);
        if (waitpid(pid, &status, 0) < 0) {
            perror("waitpid");
        } else {
            if (WIFEXITED(status)) {
                printf("child exit status: %d\n", WEXITSTATUS(status));
            } else {
                printf("child died an unnatural death.\n");
            }

            close(out[1]);
            dump_child_stdout(buffer, out[0]);
            printf("Child had this to say: %s\n", buffer);
        }
    } else {
        fprintf(stderr, "posix_spawn: %s\n", strerror(status));
        close(out[1]);
    }

    posix_spawn_file_actions_destroy(&action);

    int same = strcmp(buffer, "Hello World!\n");
    if (same == 0) {
        return FELIX86_BTEST_SUCCESS;
    } else {
        return 1;
    }
}