#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include "common.h"

volatile int you_can_leave = 0;

void signal_handler(int sig, siginfo_t* info, void* ucontext) {
    printf("Alarm hit\n");
    you_can_leave = 1;
}

int main() {
    sigset_t mask_before, mask_after;
    sigprocmask(SIG_SETMASK, NULL, &mask_before);

    struct sigaction act;
    act.sa_sigaction = signal_handler;
    act.sa_flags = SA_SIGINFO;
    sigemptyset(&act.sa_mask);
    sigaction(SIGALRM, &act, 0);

    alarm(1);

    printf("Mask before: %lx\n", mask_before.__val[0]);

    while (!you_can_leave) {
    }

    sigprocmask(SIG_SETMASK, NULL, &mask_before);

    if (mask_before.__val[0] != mask_after.__val[0]) {
        printf("Mask mismatch: %lx vs %lx\n", mask_before.__val[0], mask_after.__val[0]);
        return 1;
    }

    return FELIX86_BTEST_SUCCESS;
}