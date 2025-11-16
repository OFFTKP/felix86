#pragma once

#include "felix86/hle/guest_types.hpp"

int recvmsg32(int fd, x86_msghdr* msg, int flags);

int sendmsg32(int fd, const x86_msghdr* msg, int flags);

int recvmmsg32(int fd, x86_mmsghdr* msg, u32 n, int flags, x86_timespec* timeout);

int sendmmsg32(int fd, x86_mmsghdr* msg, u32 n, int flags);

int getsockopt32(int fd, int level, int optname, char* optval, u32* optlen);

int setsockopt32(int fd, int level, int optname, char* optval, int optlen);
