#include <signal.h>
#include <unistd.h>
#include "common.h"

volatile int handled = 0;

void signal_handler(int sig, siginfo_t* info, void* ctx) {
    int pid = getpid();

    // If the emulator didn't block `sig`, this would recurse forever
    if (!handled) {
        kill(pid, sig);
    }

    handled++;
}

int main() {
    struct sigaction act;
    act.sa_sigaction = signal_handler;
    act.sa_flags = SA_SIGINFO;
    sigemptyset(&act.sa_mask);
    sigaction(SIGURG, &act, 0);

    int pid = getpid();

    kill(pid, SIGURG);

    if (handled != 2) {
        return 1;
    } else {
        return FELIX86_BTEST_SUCCESS;
    }
}