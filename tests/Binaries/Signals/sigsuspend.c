#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "common.h"

int bad = 0;
int good = 0;
int continued = 0;

const char* sigdescr_np(int sig);

void safe_print(const char* str) {
    int len = strlen(str);
    syscall(SYS_write, 1, str, len);
}

void signal_handler(int sig, siginfo_t* info, void* ctx) {
    const char* str = sigdescr_np(sig);
    safe_print("Got signal!!!!\n");
    safe_print(str);
    safe_print("\n");

    if (sig != SIGUSR1) {
        if (continued)
            bad = 1;
    } else {
        good = 1;
    }
}

void* mythread(void* stuff) {
    pid_t pid = (pid_t)(uint64_t)stuff;

    usleep(300 * 1000); // 200ms, wait for thread to start
    kill(pid, SIGUSR2); // this should be ignored
    usleep(300 * 1000); // 50ms
    kill(pid, SIGUSR1); // this should be ignored

    return (void*)0xC;
}

int main() {
    struct sigaction sa;
    sa.sa_sigaction = signal_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    // Set the handler for both USR1 and USR2. However, we are gonna block USR2 in suspend_set
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);

    sigset_t suspend_set;
    sigemptyset(&suspend_set);
    sigaddset(&suspend_set, SIGUSR2);

    sigset_t full_set;
    sigfillset(&full_set);

    pthread_sigmask(SIG_BLOCK, &full_set, NULL);

    pthread_t thread;
    pthread_create(&thread, 0, mythread, (void*)(uint64_t)getpid());

    sigsuspend(&suspend_set);

    pthread_sigmask(SIG_UNBLOCK, &full_set, NULL);

    continued = 1;
    safe_print("Continued!!!\n");

    uint64_t ret = 0xBAD;

    pthread_join(thread, (void**)&ret);

    printf("Ret value: %ld\n", ret);

    if (good && !bad && ret == 0xC) {
        return FELIX86_BTEST_SUCCESS;
    } else {
        return 1;
    }
}