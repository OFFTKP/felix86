#pragma once

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

    operator rlimit() {
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

    operator iovec() {
        iovec guest64;
        guest64.iov_base = (void*)(u64)this->iov_base;
        guest64.iov_len = this->iov_len;
        return guest64;
    }

    u32 iov_base;
    u32 iov_len;
};