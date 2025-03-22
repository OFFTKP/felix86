#pragma once

#include <fcntl.h>
#include <sys/resource.h>
#include <sys/signal.h>
#include <sys/uio.h>
#include "felix86/common/utility.hpp"

struct x64_sigaction {
    void (*handler)(int, siginfo_t*, void*);
    u64 sa_flags;
    void (*restorer)(void);
    sigset_t sa_mask;
};

struct x86_user_desc {
    u32 entry_number = 0;
    u32 base_addr = 0;
    u32 limit = 0;
    u32 seg_32bit : 1 = 0;
    u32 contents : 2 = 0;
    u32 read_exec_only : 1 = 0;
    u32 limit_in_pages : 1 = 0;
    u32 seg_not_present : 1 = 0;
    u32 usable : 1 = 0;
};

struct x86_rlimit {
    x86_rlimit(const rlimit& guest64) {
        this->rlim_cur = guest64.rlim_cur;
        this->rlim_max = guest64.rlim_max;
    }

    operator rlimit() const {
        rlimit guest64;
        guest64.rlim_cur = (i32)this->rlim_cur;
        guest64.rlim_max = (i32)this->rlim_max;
        return guest64;
    }

    u32 rlim_cur;
    u32 rlim_max;
};

struct x86_iovec {
    x86_iovec(const iovec& guest64) {
        this->iov_base = (u64)guest64.iov_base;
        this->iov_len = guest64.iov_len;
    }

    operator iovec() const {
        iovec guest64;
        guest64.iov_base = (void*)(u64)this->iov_base;
        guest64.iov_len = this->iov_len;
        return guest64;
    }

    u32 iov_base;
    u32 iov_len;
};

struct x86_timespec {
    x86_timespec(const timespec& guest64) {
        this->tv_sec = guest64.tv_sec;
        this->tv_nsec = guest64.tv_nsec;
    }

    operator timespec() const {
        timespec guest64;
        guest64.tv_sec = this->tv_sec;
        guest64.tv_nsec = this->tv_nsec;
        return guest64;
    }

    u32 tv_sec;
    u32 tv_nsec;
};

constexpr int x86_O_DIRECT = 040000;
constexpr int x86_O_LARGEFILE = 0100000;
constexpr int x86_O_DIRECTORY = 0200000;
constexpr int x86_O_NOFOLLOW = 0400000;

inline int x86_to_riscv_flags(int flags) {
#define MAP(name)                                                                                                                                    \
    if (flags & x86_##name) {                                                                                                                        \
        flags &= ~x86_##name;                                                                                                                        \
        flags |= name;                                                                                                                               \
    }
    MAP(O_DIRECT);
    MAP(O_LARGEFILE);
    MAP(O_DIRECTORY);
    MAP(O_NOFOLLOW);
#undef MAP
    return flags;
}

inline int riscv_to_x86_flags(int flags) {
#define MAP(name)                                                                                                                                    \
    if (flags & name) {                                                                                                                              \
        flags &= ~name;                                                                                                                              \
        flags |= x86_##name;                                                                                                                         \
    }
    MAP(O_DIRECT);
    MAP(O_LARGEFILE);
    MAP(O_DIRECTORY);
    MAP(O_NOFOLLOW);
#undef MAP
    return flags;
}