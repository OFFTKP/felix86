#pragma once

#include <cstring>
#include <fcntl.h>
#include <linux/sem.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <sys/signal.h>
#include <sys/statfs.h>
#include <sys/uio.h>
#include "felix86/common/log.hpp"
#include "felix86/common/utility.hpp"

struct x64_sigaction {
    void (*handler)(int, siginfo_t*, void*);
    u64 sa_flags;
    void (*restorer)(void);
    u64 sa_mask;
};

struct x86_sigaction {
    u32 handler;
    u32 sa_flags;
    u32 restorer;
    u64 sa_mask;
};

// Funny reordering that had to happen, with sa_mask not being the last member of the struct it didn't allow
// for it to grow bigger in the future if sigset_t grew, which it did with the introduction of realtime signals
// so this struct was reordered in the rt_sigaction version
struct x86_old_sigaction {
    u32 handler;
    u32 sa_flags;
    u32 sa_mask;
    u32 restorer;
};

struct x86_stack_t {
    u32 ss_sp;
    u32 ss_flags;
    u32 ss_size;
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
        rlim_cur = rlimit.rlim_cur;
        rlim_max = rlimit.rlim_max;
    }

    operator rlimit() const {
        rlimit rlimit;
        rlimit.rlim_cur = (i32)rlim_cur;
        rlimit.rlim_max = (i32)rlim_max;
        return rlimit;
    }

    u32 rlim_cur;
    u32 rlim_max;
};

struct x86_iovec {
    x86_iovec(const iovec& iovec) {
        iov_base = (u64)iovec.iov_base;
        iov_len = iovec.iov_len;
    }

    operator iovec() const {
        iovec iovec;
        iovec.iov_base = (void*)(u64)iov_base;
        iovec.iov_len = iov_len;
        return iovec;
    }

    u32 iov_base;
    u32 iov_len;
};

struct x86_timespec {
    x86_timespec(const timespec& host_timespec) {
        tv_sec = host_timespec.tv_sec;
        tv_nsec = host_timespec.tv_nsec;
    }

    operator timespec() const {
        timespec timespec;
        timespec.tv_sec = tv_sec;
        timespec.tv_nsec = tv_nsec;
        return timespec;
    }

    u32 tv_sec;
    u32 tv_nsec;
};

struct x86_timeval {
    x86_timeval(const timeval& host_timeval) {
        tv_sec = host_timeval.tv_sec;
        tv_usec = host_timeval.tv_usec;
    }

    operator timeval() const {
        timeval timeval;
        timeval.tv_sec = tv_sec;
        timeval.tv_usec = tv_usec;
        return timeval;
    }

    u32 tv_sec;
    u32 tv_usec;
};

struct __attribute__((packed)) x86_epoll_event {
    x86_epoll_event(const epoll_event& epoll_event) {
        events = epoll_event.events;
        data = epoll_event.data.u64;
    }

    operator epoll_event() const {
        epoll_event epoll_event;
        epoll_event.events = events;
        epoll_event.data.u64 = data;
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

struct __attribute__((packed)) x86_statfs64 {
    u32 f_type;
    u32 f_bsize;
    u64 f_blocks;
    u64 f_bfree;
    u64 f_bavail;
    u64 f_files;
    u64 f_ffree;
    u64 f_fsid;
    u32 f_namelen;
    u32 f_frsize;
    u32 f_flags;
    u32 pad[4];

    x86_statfs64() = delete;

    x86_statfs64(const struct statfs& statfs) {
        f_type = statfs.f_type;
        f_bsize = statfs.f_bsize;
        f_blocks = statfs.f_blocks;
        f_bfree = statfs.f_bfree;
        f_bavail = statfs.f_bavail;
        f_files = statfs.f_files;
        f_ffree = statfs.f_ffree;
        memcpy(&f_fsid, &statfs.f_fsid, sizeof(u64));
        f_namelen = statfs.f_namelen;
        f_frsize = statfs.f_frsize;
        f_flags = statfs.f_flags;
    }
};

static_assert(std::is_trivial<x86_statfs64>::value);
static_assert(sizeof(x86_statfs64) == 84);

struct x86_rusage {
    x86_timeval ru_utime;
    x86_timeval ru_stime;
    u32 ru_maxrss;
    u32 ru_ixrss;
    u32 ru_idrss;
    u32 ru_isrss;
    u32 ru_minflt;
    u32 ru_majflt;
    u32 ru_nswap;
    u32 ru_inblock;
    u32 ru_oublock;
    u32 ru_msgsnd;
    u32 ru_msgrcv;
    u32 ru_nsignals;
    u32 ru_nvcsw;
    u32 ru_nivcsw;

    x86_rusage() = delete;

    x86_rusage(const struct rusage& usage) : ru_utime{usage.ru_utime}, ru_stime{usage.ru_stime} {
        ru_maxrss = usage.ru_maxrss;
        ru_ixrss = usage.ru_ixrss;
        ru_idrss = usage.ru_idrss;
        ru_isrss = usage.ru_isrss;
        ru_minflt = usage.ru_minflt;
        ru_majflt = usage.ru_majflt;
        ru_nswap = usage.ru_nswap;
        ru_inblock = usage.ru_inblock;
        ru_oublock = usage.ru_oublock;
        ru_msgsnd = usage.ru_msgsnd;
        ru_msgrcv = usage.ru_msgrcv;
        ru_nsignals = usage.ru_nsignals;
        ru_nvcsw = usage.ru_nvcsw;
        ru_nivcsw = usage.ru_nivcsw;
    }

    operator struct rusage() const {
        struct rusage usage{};
        usage.ru_utime = ru_utime;
        usage.ru_stime = ru_stime;
        usage.ru_maxrss = ru_maxrss;
        usage.ru_ixrss = ru_ixrss;
        usage.ru_idrss = ru_idrss;
        usage.ru_isrss = ru_isrss;
        usage.ru_minflt = ru_minflt;
        usage.ru_majflt = ru_majflt;
        usage.ru_nswap = ru_nswap;
        usage.ru_inblock = ru_inblock;
        usage.ru_oublock = ru_oublock;
        usage.ru_msgsnd = ru_msgsnd;
        usage.ru_msgrcv = ru_msgrcv;
        usage.ru_nsignals = ru_nsignals;
        usage.ru_nvcsw = ru_nvcsw;
        usage.ru_nivcsw = ru_nivcsw;
        return usage;
    }
};

static_assert(std::is_trivially_copyable<x86_rusage>::value);
static_assert(sizeof(x86_rusage) == 72);

struct drm_version {
    int version_major;
    int version_minor;
    int version_patchlevel;
    size_t name_len;
    char* name;
    size_t date_len;
    char* date;
    size_t desc_len;
    char* desc;
};

struct x86_drm_version {
    u32 version_major;
    u32 version_minor;
    u32 version_patchlevel;
    u32 name_len;
    u32 name;
    u32 date_len;
    u32 date;
    u32 desc_len;
    u32 desc;

    x86_drm_version() = delete;

    operator drm_version() const {
        drm_version host_drm_version;
        host_drm_version.version_major = version_major;
        host_drm_version.version_minor = version_minor;
        host_drm_version.version_patchlevel = version_patchlevel;
        host_drm_version.name_len = name_len;
        host_drm_version.name = (char*)(u64)name;
        host_drm_version.date_len = date_len;
        host_drm_version.date = (char*)(u64)date;
        host_drm_version.desc_len = desc_len;
        host_drm_version.desc = (char*)(u64)desc;
        return host_drm_version;
    }

    x86_drm_version(const drm_version& host_drm_version) {
        version_major = host_drm_version.version_major;
        version_minor = host_drm_version.version_minor;
        version_patchlevel = host_drm_version.version_patchlevel;
        name_len = host_drm_version.name_len;
        name = (u64)host_drm_version.name;
        date_len = host_drm_version.date_len;
        date = (u64)host_drm_version.date;
        desc_len = host_drm_version.desc_len;
        desc = (u64)host_drm_version.desc;

        ASSERT(name < 0xFFFF'FFFF);
        ASSERT(date < 0xFFFF'FFFF);
        ASSERT(desc < 0xFFFF'FFFF);
    }
};