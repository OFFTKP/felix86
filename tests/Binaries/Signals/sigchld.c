#include <stdio.h>
#include <sys/signal.h>
#include <unistd.h>

volatile int waitme = 1;

void signal_handler(int sig, siginfo_t* info, void* ctx) {
    printf("hello it's me: %d\n", getpid());
    printf("sigchld!! pid: %d uid: %d status: %d\n", info->si_pid, info->si_uid, info->si_status);
    printf("BYE BYE SIGNAL HANDLER :3\n");
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
}
