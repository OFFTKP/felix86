#pragma once

#include <cstring>
#include <fcntl.h>
#include <linux/sem.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/statfs.h>
#include <sys/sysinfo.h>
#include <sys/uio.h>
#include "felix86/common/log.hpp"
#include "felix86/common/types.hpp"

using x86_fdset = u32;

struct x86_linux_dirent {
    u64 d_ino;
    u64 d_off;
    u16 d_reclen;
    u8 d_type;
    u8 _pad[5];
    char d_name[];
};

static_assert(std::is_trivial_v<x86_linux_dirent>);
static_assert(sizeof(x86_linux_dirent) == 24);

struct x86_sigset_argpack {
    u32 data; // pointer to u64
    u32 size;
};

struct x64_sigaction {
    u64 handler;
    u64 sa_flags;
    u64 restorer;
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

struct x86_sysinfo {
    u32 uptime;
    u32 loads[3];
    u32 totalram;
    u32 freeram;
    u32 sharedram;
    u32 bufferram;
    u32 totalswap;
    u32 freeswap;
    u16 procs;
    u32 totalhigh;
    u32 freehigh;
    u32 mem_unit;
    char _pad[8];

    x86_sysinfo() = delete;

    x86_sysinfo(const struct sysinfo& host_sysinfo) {
        uptime = std::min(host_sysinfo.uptime, (i64)INT32_MAX);
        procs = host_sysinfo.procs;
        mem_unit = host_sysinfo.mem_unit;
        for (int i = 0; i < 3; i++) {
            loads[i] = std::min(host_sysinfo.loads[i], (u64)UINT64_MAX);
        }

        // Shift all memory and increase the memory unit if they don't fit, idea borrowed from FEX
        u32 shift = 0;
        u32 temp_mem_unit = host_sysinfo.mem_unit;
        if ((host_sysinfo.totalram >> 32) != 0 || (host_sysinfo.totalswap >> 32) != 0) {
            while (temp_mem_unit < 4096) {
                temp_mem_unit <<= 1;
                ++shift;
            }
        }

        totalram = host_sysinfo.totalram >> shift;
        sharedram = host_sysinfo.sharedram >> shift;
        bufferram = host_sysinfo.bufferram >> shift;
        freeram = host_sysinfo.freeram >> shift;
        totalhigh = host_sysinfo.totalhigh >> shift;
        freehigh = host_sysinfo.freehigh >> shift;
        totalswap = host_sysinfo.totalswap >> shift;
        freeswap = host_sysinfo.freeswap >> shift;
        mem_unit = temp_mem_unit;
    }
};

static_assert(sizeof(x86_sysinfo) == 64);
static_assert(std::is_trivial_v<x86_sysinfo>);

struct __attribute__((packed)) x86_stat64 {
    u64 st_dev;
    u8 __pad0[4];
    u32 __st_ino;
    u32 st_mode;
    u32 st_nlink;
    u32 st_uid;
    u32 st_gid;
    u64 st_rdev;
    u8 __pad3[4];
    u64 st_size;
    u32 st_blksize;
    u64 st_blocks;
    u32 st_atime_;
    u32 st_atime_nsec;
    u32 st_mtime_;
    u32 st_mtime_nsec;
    u32 st_ctime_;
    u32 st_ctime_nsec;
    u64 st_ino;

    x86_stat64(const struct stat& host_stat) {
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
        st_atime_nsec = host_stat.st_atim.tv_nsec;
        st_mtime_ = host_stat.st_mtime;
        st_mtime_nsec = host_stat.st_mtim.tv_nsec;
        st_ctime_ = host_stat.st_ctime;
        st_ctime_nsec = host_stat.st_ctim.tv_nsec;
        __st_ino = host_stat.st_ino;
    }

    x86_stat64(const struct stat64& host_stat) {
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
        st_atime_nsec = host_stat.st_atim.tv_nsec;
        st_mtime_ = host_stat.st_mtime;
        st_mtime_nsec = host_stat.st_mtim.tv_nsec;
        st_ctime_ = host_stat.st_ctime;
        st_ctime_nsec = host_stat.st_ctim.tv_nsec;
        __st_ino = host_stat.st_ino;
    }
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

struct x64_ipc_perm {
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

    x64_ipc_perm() = delete;

    x64_ipc_perm(const struct ipc64_perm& host_perm) {
        key = host_perm.key;
        uid = host_perm.uid;
        gid = host_perm.gid;
        cuid = host_perm.cuid;
        cgid = host_perm.cgid;
        mode = host_perm.mode;
        seq = host_perm.seq;
        padding = 0;
        padding2 = 0;
        padding3[0] = 0;
        padding3[1] = 0;
    }

    operator ipc64_perm() const {
        struct ipc64_perm host_perm{};
        host_perm.key = key;
        host_perm.uid = uid;
        host_perm.gid = gid;
        host_perm.cuid = cuid;
        host_perm.cgid = cgid;
        host_perm.mode = mode;
        host_perm.seq = seq;
        return host_perm;
    }
};

static_assert(std::is_trivial<x64_ipc_perm>::value);
static_assert(sizeof(x64_ipc_perm) == 48);

struct x64_semid64_ds {
    x64_ipc_perm sem_perm;
    u64 sem_otime;
    u64 unused1;
    u64 sem_ctime;
    u64 unused2;
    u64 sem_nsems;
    u64 unused3;
    u64 unused4;

    x64_semid64_ds() = delete;

    operator struct semid64_ds() const {
        struct semid64_ds semi{};
        semi.sem_perm = sem_perm;
        semi.sem_otime = sem_otime;
        semi.sem_ctime = sem_ctime;
        semi.sem_nsems = sem_nsems;
        return semi;
    }

    x64_semid64_ds(const struct semid64_ds& semi) : sem_perm(semi.sem_perm) {
        sem_otime = semi.sem_otime;
        sem_ctime = semi.sem_ctime;
        sem_nsems = semi.sem_nsems;
    }
};

static_assert(std::is_trivial<x64_semid64_ds>::value);
static_assert(sizeof(x64_semid64_ds) == 104);

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

struct x86_mmsghdr {
    x86_msghdr header;
    u32 msg_len;
};

static_assert(std::is_trivial<x86_mmsghdr>::value);
static_assert(sizeof(x86_mmsghdr) == 32);

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

struct x86_ipc_perm_32 {
    u32 key;
    u16 uid;
    u16 gid;
    u16 cuid;
    u16 cgid;
    u16 mode;
    u16 seq;

    x86_ipc_perm_32() = delete;

    operator struct ipc64_perm() const {
        struct ipc64_perm host_perm{};
        host_perm.key = key;
        host_perm.uid = uid;
        host_perm.gid = gid;
        host_perm.cuid = cuid;
        host_perm.cgid = cgid;
        host_perm.mode = mode;
        host_perm.seq = seq;
        return host_perm;
    }

    x86_ipc_perm_32(const ipc64_perm& host_perm) {
        key = host_perm.key;
        uid = host_perm.uid;
        gid = host_perm.gid;
        cuid = host_perm.cuid;
        cgid = host_perm.cgid;
        mode = host_perm.mode;
        seq = host_perm.seq;
    }
};

static_assert(std::is_trivially_copyable_v<x86_ipc_perm_32>);
static_assert(sizeof(x86_ipc_perm_32) == 16);

struct x86_ipc_perm_64 {
    u32 key;
    u32 uid;
    u32 gid;
    u32 cuid;
    u32 cgid;
    u16 mode;
    u16 _pad1;
    u16 seq;
    u16 _pad2;
    u32 _pad[2];

    x86_ipc_perm_64() = delete;

    operator struct ipc64_perm() const {
        struct ipc64_perm host_perm{};
        host_perm.key = key;
        host_perm.uid = uid;
        host_perm.gid = gid;
        host_perm.cuid = cuid;
        host_perm.cgid = cgid;
        host_perm.mode = mode;
        host_perm.seq = seq;
        return host_perm;
    }

    x86_ipc_perm_64(const ipc64_perm& host_perm) {
        key = host_perm.key;
        uid = host_perm.uid;
        gid = host_perm.gid;
        cuid = host_perm.cuid;
        cgid = host_perm.cgid;
        mode = host_perm.mode;
        seq = host_perm.seq;
    }
};

static_assert(std::is_trivially_copyable_v<x86_ipc_perm_64>);
static_assert(sizeof(x86_ipc_perm_64) == 36);

struct riscv64_shmid64_ds {
    struct ipc64_perm shm_perm; /* operation permission struct */
    size_t shm_segsz;           /* size of segment in bytes */
    u64 shm_atime;              /* time of last shmat() */
    u64 shm_dtime;              /* time of last shmdt() */
    u64 shm_ctime;              /* time of last change by shmctl() */
    u32 shm_cpid;               /* pid of creator */
    u32 shm_lpid;               /* pid of last shmop */
    u64 shm_nattch;             /* number of current attaches */
    u64 __glibc_reserved5;
    u64 __glibc_reserved6;
};

struct riscv64_semid64_ds {
    struct ipc64_perm sem_perm; /* permissions .. see ipc.h */
    u64 sem_otime;              /* last semop time */
    u64 sem_ctime;              /* last change time */
    u64 sem_nsems;              /* no. of semaphores in array */
    u64 __unused3 = 0;
    u64 __unused4 = 0;
};

struct riscv64_shminfo {
    u64 shmmax;
    u64 shmmin;
    u64 shmmni;
    u64 shmseg;
    u64 shmall;
    u64 __glibc_reserved1;
    u64 __glibc_reserved2;
    u64 __glibc_reserved3;
    u64 __glibc_reserved4;
};

struct riscv64_shm_info {
    u32 used_ids;
    u64 shm_tot; /* total allocated shm */
    u64 shm_rss; /* total resident shm */
    u64 shm_swp; /* total swapped shm */
    u64 swap_attempts;
    u64 swap_successes;
};

struct x86_shmid_ds_64 {
    x86_ipc_perm_64 shm_perm;
    u32 shm_segsz;
    u32 shm_atime;
    u32 shm_atime_high;
    u32 shm_dtime;
    u32 shm_dtime_high;
    u32 shm_ctime;
    u32 shm_ctime_high;
    u32 shm_cpid;
    u32 shm_lpid;
    u32 shm_nattch;
    u32 shm_unused4;
    u32 shm_unused5;

    x86_shmid_ds_64() = delete;

    operator struct riscv64_shmid64_ds() const {
        struct riscv64_shmid64_ds host_shmid{};
        host_shmid.shm_perm = shm_perm;
        host_shmid.shm_segsz = shm_segsz;
        host_shmid.shm_atime = shm_atime_high;
        host_shmid.shm_atime <<= 32;
        host_shmid.shm_atime |= shm_atime;
        host_shmid.shm_dtime = shm_dtime_high;
        host_shmid.shm_dtime <<= 32;
        host_shmid.shm_dtime |= shm_dtime;
        host_shmid.shm_ctime = shm_ctime_high;
        host_shmid.shm_ctime <<= 32;
        host_shmid.shm_ctime |= shm_ctime;
        host_shmid.shm_cpid = shm_cpid;
        host_shmid.shm_lpid = shm_lpid;
        host_shmid.shm_nattch = shm_nattch;
        return host_shmid;
    }

    x86_shmid_ds_64(const riscv64_shmid64_ds& host_shmid) : shm_perm{host_shmid.shm_perm} {
        shm_segsz = host_shmid.shm_segsz;
        shm_atime = host_shmid.shm_atime;
        shm_atime_high = host_shmid.shm_atime >> 32;
        shm_dtime = host_shmid.shm_dtime;
        shm_dtime_high = host_shmid.shm_dtime >> 32;
        shm_ctime = host_shmid.shm_ctime;
        shm_ctime_high = host_shmid.shm_ctime >> 32;
        shm_cpid = host_shmid.shm_cpid;
        shm_lpid = host_shmid.shm_lpid;
        shm_nattch = host_shmid.shm_nattch;
    }
};

static_assert(std::is_trivially_copyable_v<x86_shmid_ds_64>);
static_assert(sizeof(x86_shmid_ds_64) == 84);

struct x86_shmid_ds_32 {
    x86_ipc_perm_32 shm_perm;
    u32 shm_segsz;
    u32 shm_atime;
    u32 shm_dtime;
    u32 shm_ctime;
    u16 shm_cpid;
    u16 shm_lpid;
    u16 shm_nattch;
    u16 shm_unused;
    u32 shm_unused2;
    u32 shm_unused3;

    x86_shmid_ds_32() = delete;

    operator struct riscv64_shmid64_ds() const {
        struct riscv64_shmid64_ds host_shmid{};
        host_shmid.shm_perm = shm_perm;
        host_shmid.shm_segsz = shm_segsz;
        host_shmid.shm_atime = shm_atime;
        host_shmid.shm_dtime = shm_dtime;
        host_shmid.shm_ctime = shm_ctime;
        host_shmid.shm_cpid = shm_cpid;
        host_shmid.shm_lpid = shm_lpid;
        host_shmid.shm_nattch = shm_nattch;
        return host_shmid;
    }

    x86_shmid_ds_32(const riscv64_shmid64_ds& host_shmid) : shm_perm{host_shmid.shm_perm} {
        shm_segsz = host_shmid.shm_segsz;
        shm_atime = host_shmid.shm_atime;
        shm_dtime = host_shmid.shm_dtime;
        shm_ctime = host_shmid.shm_ctime;
        shm_cpid = host_shmid.shm_cpid;
        shm_lpid = host_shmid.shm_lpid;
        shm_nattch = host_shmid.shm_nattch;
    }
};

static_assert(std::is_trivially_copyable_v<x86_shmid_ds_32>);
static_assert(sizeof(x86_shmid_ds_32) == 48);

struct x86_semid_ds_32 {
    x86_ipc_perm_32 sem_perm;
    u32 sem_otime;
    u32 sem_ctime;
    u32 sem_base;
    u32 sem_pending;
    u32 sem_pending_last;
    u32 undo;
    u16 sem_nsems;
    u16 _pad;

    x86_semid_ds_32() = delete;

    operator riscv64_semid64_ds() const {
        riscv64_semid64_ds host_semid{};
        host_semid.sem_perm = sem_perm;
        host_semid.sem_otime = sem_otime;
        host_semid.sem_ctime = sem_ctime;
        host_semid.sem_nsems = sem_nsems;
        return host_semid;
    }

    x86_semid_ds_32(const riscv64_semid64_ds& host_semid) : sem_perm{host_semid.sem_perm} {
        sem_otime = host_semid.sem_otime;
        sem_ctime = host_semid.sem_ctime;
        sem_nsems = host_semid.sem_nsems;
        sem_base = 0;
        sem_pending = 0;
        sem_pending_last = 0;
        undo = 0;
        _pad = 0;
    }
};

static_assert(std::is_trivial_v<x86_semid_ds_32>);
static_assert(sizeof(x86_semid_ds_32) == 44);

struct x86_semid_ds_64 {
    x86_ipc_perm_64 sem_perm;

    // It'd be great to not have to use *_high but if you make these u64 the compiler adds padding
    u32 sem_otime;
    u32 sem_otime_high;
    u32 sem_ctime;
    u32 sem_ctime_high;
    u32 sem_nsems;
    u32 _pad[2];

    x86_semid_ds_64() = delete;

    operator riscv64_semid64_ds() const {
        riscv64_semid64_ds host_semid;
        host_semid.sem_perm = sem_perm;
        host_semid.sem_otime = (u64)sem_otime | ((u64)sem_otime_high << 32);
        host_semid.sem_ctime = (u64)sem_ctime | ((u64)sem_ctime_high << 32);
        host_semid.sem_nsems = sem_nsems;
        return host_semid;
    }

    x86_semid_ds_64(const riscv64_semid64_ds& host_semid) : sem_perm(host_semid.sem_perm) {
        sem_otime = host_semid.sem_otime;
        sem_otime_high = host_semid.sem_otime >> 32;
        sem_ctime = host_semid.sem_ctime;
        sem_ctime_high = host_semid.sem_ctime >> 32;
        sem_nsems = host_semid.sem_nsems;
    }
};

static_assert(std::is_trivial_v<x86_semid_ds_64>);
static_assert(sizeof(x86_semid_ds_64) == 64);

struct x86_shminfo_32 {
    u32 shmmax;
    u32 shmmin;
    u32 shmmni;
    u32 shmseg;
    u32 shmall;

    x86_shminfo_32() = delete;

    operator struct riscv64_shminfo() const {
        struct riscv64_shminfo host_shminfo{};
        host_shminfo.shmmax = shmmax;
        host_shminfo.shmmin = shmmin;
        host_shminfo.shmmni = shmmni;
        host_shminfo.shmseg = shmseg;
        host_shminfo.shmall = shmall;
        return host_shminfo;
    }

    x86_shminfo_32(const riscv64_shminfo& host_shminfo) {
        shmmax = host_shminfo.shmmax;
        shmmin = host_shminfo.shmmin;
        shmmni = host_shminfo.shmmni;
        shmseg = host_shminfo.shmseg;
        shmall = host_shminfo.shmall;
    }
};

static_assert(std::is_trivially_copyable_v<x86_shminfo_32>);
static_assert(sizeof(x86_shminfo_32) == 20);

struct x86_shm_info_32 {
    u32 used_ids;
    u32 shm_tot;
    u32 shm_rss;
    u32 shm_swp;
    u32 swap_attempts;
    u32 swap_successes;

    x86_shm_info_32() = delete;

    x86_shm_info_32(struct riscv64_shm_info host_shm_info) {
        used_ids = host_shm_info.used_ids;
        shm_tot = host_shm_info.shm_tot;
        shm_rss = host_shm_info.shm_rss;
        shm_swp = host_shm_info.shm_swp;
        swap_attempts = host_shm_info.swap_attempts;
        swap_successes = host_shm_info.swap_successes;
    }
};

static_assert(std::is_trivially_copyable_v<x86_shm_info_32>);
static_assert(sizeof(x86_shm_info_32) == 24);

struct x86_shminfo_64 {
    u32 shmmax;
    u32 shmmin;
    u32 shmmni;
    u32 shmseg;
    u32 shmall;
    u32 unused1;
    u32 unused2;
    u32 unused3;
    u32 unused4;

    x86_shminfo_64() = delete;

    operator riscv64_shminfo() const {
        riscv64_shminfo host_shminfo{};
        host_shminfo.shmmax = shmmax;
        host_shminfo.shmmin = shmmin;
        host_shminfo.shmmni = shmmni;
        host_shminfo.shmseg = shmseg;
        host_shminfo.shmall = shmall;
        return host_shminfo;
    }

    x86_shminfo_64(const riscv64_shminfo& host_shminfo) {
        shmmax = host_shminfo.shmmax;
        shmmin = host_shminfo.shmmin;
        shmmni = host_shminfo.shmmni;
        shmseg = host_shminfo.shmseg;
        shmall = host_shminfo.shmall;
    }
};

static_assert(std::is_trivially_copyable_v<x86_shminfo_64>);
static_assert(sizeof(x86_shminfo_64) == 36);

union x86_semun {
    i32 _i32;
    u32 _u32;
};

union __attribute__((packed)) x86_sifields {
    u32 pad[29];

    /* kill() */
    struct {
        u32 _pid; /* sender's pid */
        u32 _uid; /* sender's uid */
    } _kill;

    /* POSIX.1b timers */
    struct {
        u32 _tid;         /* timer id */
        u32 _overrun;     /* overrun count */
        u32 _sigval;      /* same as below */
        u32 _sys_private; /* Not used by the kernel. Historic leftover. Always 0. */
    } _timer;

    /* POSIX.1b signals */
    struct {
        u32 _pid; /* sender's pid */
        u32 _uid; /* sender's uid */
        u32 _sigval;
    } _rt;

    /* SIGCHLD */
    struct {
        u32 _pid;    /* which child */
        u32 _uid;    /* sender's uid */
        u32 _status; /* exit code */
        u32 _utime;
        u32 _stime;
    } _sigchld;

    /* SIGILL, SIGFPE, SIGSEGV, SIGBUS, SIGTRAP, SIGEMT */
    struct {
        u32 _addr; /* faulting insn/memory ref. */
    } _sigfault;

    /* SIGPOLL */
    struct {
        u32 _band; /* POLL_IN, POLL_OUT, POLL_MSG */
        u32 _fd;
    } _sigpoll;

    /* SIGSYS */
    struct {
        u32 _call_addr; /* calling user insn */
        u32 _syscall;   /* triggering system call number */
        u32 _arch;      /* AUDIT_ARCH_* of syscall */
    } _sigsys;
};

struct x86_siginfo_t {
    u32 si_signo;
    u32 si_errno;
    u32 si_code;
    x86_sifields fields;

    operator siginfo_t() const {
        siginfo_t host_siginfo;
        host_siginfo.si_code = si_code;
        host_siginfo.si_errno = si_errno;
        host_siginfo.si_signo = si_signo;
        host_siginfo.__pad0 = 0;
        memcpy(&host_siginfo._sifields, &fields, sizeof(host_siginfo._sifields));
        return host_siginfo;
    }

    x86_siginfo_t(const siginfo_t& host_siginfo) {
        si_signo = host_siginfo.si_signo;
        si_errno = host_siginfo.si_errno;
        si_code = host_siginfo.si_code;

        memcpy(&fields, &host_siginfo._sifields, sizeof(host_siginfo._sifields));
    }
};

static_assert(sizeof(x86_siginfo_t) == 128);

#undef __ADDR_BND_PKEY_PAD
#undef __SIGINFO