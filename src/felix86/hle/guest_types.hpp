#pragma once

#include <fcntl.h>
#include <sys/epoll.h>
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

struct __attribute__((packed)) x86_epoll_event {
    x86_epoll_event(const epoll_event& guest64) {
        this->events = guest64.events;
        this->data = guest64.data.u64;
    }

    operator epoll_event() const {
        epoll_event guest64;
        guest64.events = this->events;
        guest64.data.u64 = this->data;
        return guest64;
    }

    u32 events = 0;
    u64 data = 0;
};

struct __attribute__((packed)) x86_stat {
    x86_stat() = delete;

    x86_stat(const struct stat& host_stat) {
        st_dev = host_stat.st_dev;
        st_ino = host_stat.st_ino;
        st_nlink = host_stat.st_nlink;
        st_mode = host_stat.st_mode;
        st_uid = host_stat.st_uid;
        st_gid = host_stat.st_gid;
        st_rdev = host_stat.st_rdev;
        st_size = host_stat.st_size;
        st_blksize = host_stat.st_blksize;
        st_blocks = host_stat.st_blocks;
        st_atime_ = host_stat.st_atim.tv_sec;
        fex_st_atime_nsec = host_stat.st_atim.tv_nsec;
        st_mtime_ = host_stat.st_mtime;
        fex_st_mtime_nsec = host_stat.st_mtim.tv_nsec;
        st_ctime_ = host_stat.st_ctime;
        fex_st_ctime_nsec = host_stat.st_ctim.tv_nsec;
    }

    operator struct stat() const {
        struct stat host_stat;
        host_stat.st_dev = st_dev;
        host_stat.st_ino = st_ino;
        host_stat.st_nlink = st_nlink;
        host_stat.st_mode = st_mode;
        host_stat.st_uid = st_uid;
        host_stat.st_gid = st_gid;
        host_stat.st_rdev = st_rdev;
        host_stat.st_size = st_size;
        host_stat.st_blksize = st_blksize;
        host_stat.st_blocks = st_blocks;
        host_stat.st_atim.tv_sec = st_atime_;
        host_stat.st_atim.tv_nsec = fex_st_atime_nsec;
        host_stat.st_mtim.tv_sec = st_mtime_;
        host_stat.st_mtim.tv_nsec = fex_st_mtime_nsec;
        host_stat.st_ctim.tv_sec = st_ctime_;
        host_stat.st_ctim.tv_nsec = fex_st_ctime_nsec;
        return host_stat;
    }

    u64 st_dev;
    u64 st_ino;
    u64 st_nlink;

    unsigned int st_mode;
    unsigned int st_uid;
    unsigned int st_gid;
    [[maybe_unused]] unsigned int __pad0;
    u64 st_rdev;
    i64 st_size;
    i64 st_blksize;
    i64 st_blocks;

    u64 st_atime_;
    u64 fex_st_atime_nsec;
    u64 st_mtime_;
    u64 fex_st_mtime_nsec;
    u64 st_ctime_;
    u64 fex_st_ctime_nsec;
    [[maybe_unused]] i64 unused[3];
};

static_assert(std::is_trivial<x86_stat>::value);
static_assert(sizeof(x86_stat) == 144);
