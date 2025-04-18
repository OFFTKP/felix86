#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include "common.h"

volatile int handled_count = 0;
volatile int broken = 0;

void signal_handler(int sig, siginfo_t* info, void* ctx) {
    if (handled_count < 5) {
        handled_count++;
        printf("Incrementing count, new count: %d\n", handled_count);
        int pid = getpid();
        kill(pid, sig);

        // Don't get to this point unless if all signals have been handled
        // SA_NODEFER allows it to execute the signal while inside the handler
        if (handled_count != 5) {
            printf("Broken, count: %d\n", handled_count);
            broken = 1;
        }
    }
}

int main() {
    struct sigaction act;
    act.sa_sigaction = signal_handler;
    act.sa_flags = SA_SIGINFO | SA_NODEFER; // NODEFER allows the signal to happen inside the handler
    sigemptyset(&act.sa_mask);
    sigaction(SIGURG, &act, 0);

    int pid = getpid();

    kill(pid, SIGURG);

    if (handled_count != 5) {
        return 1;
    } else if (broken) {
        return 2;
    } else {
        return FELIX86_BTEST_SUCCESS;
    }
}