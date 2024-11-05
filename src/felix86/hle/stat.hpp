#pragma once

#include <sys/stat.h>
#include "felix86/common/utility.hpp"

struct __attribute__((packed)) x64Stat {
    x64Stat() = delete;

    x64Stat(const struct stat& host_stat) {
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

private:
    u64 st_dev;
    u64 st_ino;
    u64 st_nlink;

    unsigned int st_mode;
    unsigned int st_uid;
    unsigned int st_gid;
    [[maybe_unused]] unsigned int __pad0;
    u64 st_rdev;
    int64_t st_size;
    int64_t st_blksize;
    int64_t st_blocks; /* Number 512-byte blocks allocated. */

    u64 st_atime_;
    u64 fex_st_atime_nsec;
    u64 st_mtime_;
    u64 fex_st_mtime_nsec;
    u64 st_ctime_;
    u64 fex_st_ctime_nsec;
    [[maybe_unused]] int64_t unused[3];
};

static_assert(std::is_trivial<x64Stat>::value);
static_assert(sizeof(x64Stat) == 144);
