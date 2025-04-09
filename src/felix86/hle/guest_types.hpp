#pragma once

#include <fcntl.h>
#include <linux/sem.h>
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
    x86_rlimit(const rlimit& rlimit) {
        this->rlim_cur = rlimit.rlim_cur;
        this->rlim_max = rlimit.rlim_max;
    }

    operator rlimit() const {
        rlimit rlimit;
        rlimit.rlim_cur = (i32)this->rlim_cur;
        rlimit.rlim_max = (i32)this->rlim_max;
        return rlimit;
    }

    u32 rlim_cur;
    u32 rlim_max;
};

struct x86_iovec {
    x86_iovec(const iovec& iovec) {
        this->iov_base = (u64)iovec.iov_base;
        this->iov_len = iovec.iov_len;
    }

    operator iovec() const {
        iovec iovec;
        iovec.iov_base = (void*)(u64)this->iov_base;
        iovec.iov_len = this->iov_len;
        return iovec;
    }

    u32 iov_base;
    u32 iov_len;
};

struct x86_timespec {
    x86_timespec(const timespec& host_timespec) {
        this->tv_sec = host_timespec.tv_sec;
        this->tv_nsec = host_timespec.tv_nsec;
    }

    operator timespec() const {
        timespec timespec;
        timespec.tv_sec = this->tv_sec;
        timespec.tv_nsec = this->tv_nsec;
        return timespec;
    }

    u32 tv_sec;
    u32 tv_nsec;
};

struct __attribute__((packed)) x86_epoll_event {
    x86_epoll_event(const epoll_event& epoll_event) {
        this->events = epoll_event.events;
        this->data = epoll_event.data.u64;
    }

    operator epoll_event() const {
        epoll_event epoll_event;
        epoll_event.events = this->events;
        epoll_event.data.u64 = this->data;
        return epoll_event;
    }

    u32 events = 0;
    u64 data = 0;
};

struct __attribute__((packed)) x86_stat {
    x86_stat() = delete;

    x86_stat(const struct stat& stat) {
        st_dev = stat.st_dev;
        st_ino = stat.st_ino;
        st_nlink = stat.st_nlink;
        st_mode = stat.st_mode;
        st_uid = stat.st_uid;
        st_gid = stat.st_gid;
        st_rdev = stat.st_rdev;
        st_size = stat.st_size;
        st_blksize = stat.st_blksize;
        st_blocks = stat.st_blocks;
        st_atime_ = stat.st_atim.tv_sec;
        fex_st_atime_nsec = stat.st_atim.tv_nsec;
        st_mtime_ = stat.st_mtime;
        fex_st_mtime_nsec = stat.st_mtim.tv_nsec;
        st_ctime_ = stat.st_ctime;
        fex_st_ctime_nsec = stat.st_ctim.tv_nsec;
    }

    operator struct stat() const {
        struct stat stat;
        stat.st_dev = st_dev;
        stat.st_ino = st_ino;
        stat.st_nlink = st_nlink;
        stat.st_mode = st_mode;
        stat.st_uid = st_uid;
        stat.st_gid = st_gid;
        stat.st_rdev = st_rdev;
        stat.st_size = st_size;
        stat.st_blksize = st_blksize;
        stat.st_blocks = st_blocks;
        stat.st_atim.tv_sec = st_atime_;
        stat.st_atim.tv_nsec = fex_st_atime_nsec;
        stat.st_mtim.tv_sec = st_mtime_;
        stat.st_mtim.tv_nsec = fex_st_mtime_nsec;
        stat.st_ctim.tv_sec = st_ctime_;
        stat.st_ctim.tv_nsec = fex_st_ctime_nsec;
        return stat;
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

struct x86_ipc_perm {
    u32 key;
    u32 uid;
    u32 gid;
    u32 cuid;
    u32 cgid;
    u16 mode;
    u16 padding;
    u16 seq;
    u16 padding2;
    u64 padding3[2];

    x86_ipc_perm() = delete;

    x86_ipc_perm(const struct ipc64_perm& perm) {
        key = perm.key;
        uid = perm.uid;
        gid = perm.gid;
        cuid = perm.cuid;
        cgid = perm.cgid;
        mode = perm.mode;
        seq = perm.seq;
        padding = 0;
        padding2 = 0;
        padding3[0] = 0;
        padding3[1] = 0;
    }

    operator ipc64_perm() const {
        struct ipc64_perm perm{};
        perm.key = key;
        perm.uid = uid;
        perm.gid = gid;
        perm.cuid = cuid;
        perm.cgid = cgid;
        perm.mode = mode;
        perm.seq = seq;
        return perm;
    }
};

static_assert(std::is_trivial<x86_ipc_perm>::value);
static_assert(sizeof(x86_ipc_perm) == 48);

struct x86_semid64_ds {
    x86_ipc_perm sem_perm;
    u64 sem_otime;
    u64 unused1;
    u64 sem_ctime;
    u64 unused2;
    u64 sem_nsems;
    u64 unused3;
    u64 unused4;

    x86_semid64_ds() = delete;

    operator struct semid64_ds() const {
        struct semid64_ds semi{};
        semi.sem_perm = sem_perm;
        semi.sem_otime = sem_otime;
        semi.sem_ctime = sem_ctime;
        semi.sem_nsems = sem_nsems;
        return semi;
    }

    x86_semid64_ds(const struct semid64_ds& semi) : sem_perm(semi.sem_perm) {
        sem_otime = semi.sem_otime;
        sem_ctime = semi.sem_ctime;
        sem_nsems = semi.sem_nsems;
    }
};

static_assert(std::is_trivial<x86_semid64_ds>::value);
static_assert(sizeof(x86_semid64_ds) == 104);

struct __attribute__((packed)) x86_flock64 {
    i16 l_type;
    i16 l_whence;
    i64 l_start;
    i64 l_len;
    i32 l_pid;

    x86_flock64() = delete;

    x86_flock64(const struct flock& flock) {
        l_type = flock.l_type;
        l_whence = flock.l_whence;
        l_start = flock.l_start;
        l_len = flock.l_len;
        l_pid = flock.l_pid;
    }

    operator struct flock() const {
        struct flock flock{};
        flock.l_type = l_type;
        flock.l_whence = l_whence;
        flock.l_start = l_start;
        flock.l_len = l_len;
        flock.l_pid = l_pid;
        return flock;
    }
};

static_assert(std::is_trivial<x86_flock64>::value);
static_assert(sizeof(x86_flock64) == 24);

struct __attribute__((packed)) x86_flock {
    i16 l_type;
    i16 l_whence;
    i32 l_start;
    i32 l_len;
    i32 l_pid;

    x86_flock() = delete;

    x86_flock(const struct flock& flock) {
        l_type = flock.l_type;
        l_whence = flock.l_whence;
        l_start = flock.l_start;
        l_len = flock.l_len;
        l_pid = flock.l_pid;
    }

    operator struct flock() const {
        struct flock flock{};
        flock.l_type = l_type;
        flock.l_whence = l_whence;
        flock.l_start = l_start;
        flock.l_len = l_len;
        flock.l_pid = l_pid;
        return flock;
    }
};

static_assert(std::is_trivial<x86_flock>::value);
static_assert(sizeof(x86_flock) == 16);

struct x86_cmsghdr {
    u32 cmsg_len;
    u32 cmsg_level;
    u32 cmsg_type;
    u8 cmsg_data[0]; // variable sized
};

static_assert(std::is_trivial<x86_cmsghdr>::value);
static_assert(sizeof(x86_cmsghdr) == 12);

struct x86_msghdr {
    u32 msg_name;
    u32 msg_namelen;
    u32 msg_iov;
    u32 msg_iovlen;
    u32 msg_control;
    u32 msg_controllen;
    u32 msg_flags;
};

static_assert(std::is_trivial<x86_msghdr>::value);
static_assert(sizeof(x86_msghdr) == 28);
