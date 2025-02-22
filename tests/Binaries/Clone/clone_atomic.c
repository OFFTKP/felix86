#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <linux/sched.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "common.h"

int clone_handler(void* memory) {
    long* ptr = (long*)memory;
    while (1) {
        long val = __atomic_add_fetch(ptr, 1, __ATOMIC_RELAXED);
        if (val >= 10000000) {
            break;
        }
    }

    return 1;
}

int main() {
    void* memory = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    void* stack = malloc(1024);

    int res = clone(clone_handler, stack, CLONE_VM, memory, 0, 0, 0);
    if (res == -1) {
        printf("clone failed\n");
        return 1;
    }

    long* ptr = (long*)memory;
    while (1) {
        long val = __atomic_add_fetch(ptr, 1, __ATOMIC_RELAXED);
        if (val >= 10000000) {
            break;
        }
    }

    long val = *ptr;
    if (val == 10000000 || val == 10000001) {
        return FELIX86_BTEST_SUCCESS;
    } else {
        printf("Bad value: %ld\n", val);
        return 1;
    }
}