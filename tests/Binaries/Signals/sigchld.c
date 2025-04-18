#include <stdio.h>
#include <sys/signal.h>
#include <unistd.h>
#include "common.h"

volatile int waitme = 1;

void signal_handler(int sig, siginfo_t* info, void* ctx) {
    waitme = 0;
}

int main() {
    struct sigaction sa;
    sa.sa_sigaction = signal_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD, &sa, 0);

    int res = fork();
    if (res == 0) {
        printf("Hello from child %d, exiting...\n", getpid());
    } else {
        printf("Hello from parent %d, waiting...\n", getpid());
        while (waitme)
            ;
        printf("Done waiting!!\n");
    }

    return FELIX86_BTEST_SUCCESS;
}
