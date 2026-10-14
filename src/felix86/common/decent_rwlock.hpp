#pragma once

#include <cerrno>
#include <climits>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "felix86/common/types.hpp"

// pthread_rwlock_t relies on the cached TID in glibc, which may not be set properly if we call pthread_create with custom flags
// under our custom glibc. We don't want to pass CLONE_PARENT_SETTID to the pthread flags either as that could lead to trouble.
// So we roll our own which is good enough and works.
struct decent_rwlock_t {
    void read_lock() {
        u32 current = __atomic_load_n(&state, __ATOMIC_SEQ_CST);
        for (;;) {
            if (current == writer) {
                wait(writer);
                current = __atomic_load_n(&state, __ATOMIC_SEQ_CST);
            } else if (__atomic_compare_exchange_n(&state, &current, current + 1, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
                return;
            }
        }
    }

    void read_unlock() {
        if (__atomic_sub_fetch(&state, 1, __ATOMIC_SEQ_CST) == 0) {
            wake();
        }
    }

    void write_lock() {
        u32 current = 0;
        while (!__atomic_compare_exchange_n(&state, &current, writer, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
            wait(current);
            current = 0;
        }
    }

    void write_unlock() {
        __atomic_store_n(&state, 0, __ATOMIC_SEQ_CST);
        wake();
    }

private:
    static constexpr u32 writer = UINT32_MAX;

    void wait(u32 expected) {
        __atomic_add_fetch(&waiters, 1, __ATOMIC_SEQ_CST);
        int saved_errno = errno; // don't mess up errno as we use read_lock during signals in handle_smc
        syscall(SYS_futex, &state, FUTEX_WAIT_PRIVATE, expected, nullptr, nullptr, 0);
        errno = saved_errno;
        __atomic_sub_fetch(&waiters, 1, __ATOMIC_SEQ_CST);
    }

    void wake() {
        if (__atomic_load_n(&waiters, __ATOMIC_SEQ_CST) != 0) {
            int saved_errno = errno;
            syscall(SYS_futex, &state, FUTEX_WAKE_PRIVATE, INT_MAX, nullptr, nullptr, 0);
            errno = saved_errno;
        }
    }

    u32 state = 0;
    u32 waiters = 0;
};
