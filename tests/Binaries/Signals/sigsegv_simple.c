#include <signal.h>

int pass = 1;

void signal_handler(int sig, siginfo_t* info, void* ucontext) {
    pass = 0;
}

int main() {
    struct sigaction act;
    act.sa_sigaction = signal_handler;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &act, 0);

    volatile int* ptr = 0;
    *ptr = 42;

    return pass;
}