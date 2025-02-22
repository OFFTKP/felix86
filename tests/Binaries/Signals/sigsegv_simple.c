#include <signal.h>

volatile int* address = 0;
int valid_memory = 0;

void signal_handler(int sig, siginfo_t* info, void* ucontext) {
    address = &valid_memory;
}

int main() {
    struct sigaction act;
    act.sa_sigaction = signal_handler;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &act, 0);

    *address = 42;

    if (valid_memory == 42) {
        return 0x42;
    }

    return 1;
}