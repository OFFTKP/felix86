#include <cstring>
#include <sys/socket.h>
#include "felix86/common/log.hpp"
#include "felix86/hle/socket32.hpp"

#ifndef SO_RCVTIMEO_OLD
#define SO_RCVTIMEO_OLD 20
#endif
#ifndef SO_SNDTIMEO_OLD
#define SO_SNDTIMEO_OLD 21
#endif
#ifndef SO_TIMESTAMP_OLD
#define SO_TIMESTAMP_OLD 29
#endif
#ifndef SO_TIMESTAMPNS_OLD
#define SO_TIMESTAMPNS_OLD 35
#endif
#ifndef SO_TIMESTAMPING_OLD
#define SO_TIMESTAMPING_OLD 37
#endif
#ifndef SO_MEMINFO
#define SO_MEMINFO 55
#endif
#ifndef SO_INCOMING_NAPI_ID
#define SO_INCOMING_NAPI_ID 56
#endif
#ifndef SO_PEERGROUPS
#define SO_PEERGROUPS 59
#endif
#ifndef SO_ZEROCOPY
#define SO_ZEROCOPY 60
#endif
#ifndef SO_TXTIME
#define SO_TXTIME 61
#endif
#ifndef SO_BINDTOIFINDEX
#define SO_BINDTOIFINDEX 62
#endif
#ifndef SO_TIMESTAMP_NEW
#define SO_TIMESTAMP_NEW 63
#endif
#ifndef SO_TIMESTAMPNS_NEW
#define SO_TIMESTAMPNS_NEW 64
#endif
#ifndef SO_TIMESTAMPING_NEW
#define SO_TIMESTAMPING_NEW 65
#endif
#ifndef SO_RCVTIMEO_NEW
#define SO_RCVTIMEO_NEW 66
#endif
#ifndef SO_SNDTIMEO_NEW
#define SO_SNDTIMEO_NEW 67
#endif
#ifndef SO_DETACH_REUSEPORT_BPF
#define SO_DETACH_REUSEPORT_BPF 68
#endif
#ifndef SO_PREFER_BUSY_POLL
#define SO_PREFER_BUSY_POLL 69
#endif
#ifndef SO_BUSY_POLL_BUDGET
#define SO_BUSY_POLL_BUDGET 70
#endif
#ifndef SO_NETNS_COOKIE
#define SO_NETNS_COOKIE 71
#endif
#ifndef SO_BUF_LOCK
#define SO_BUF_LOCK 72
#endif
#ifndef SO_RESERVE_MEM
#define SO_RESERVE_MEM 73
#endif
#ifndef SO_TXREHASH
#define SO_TXREHASH 74
#endif
#ifndef SO_RCVMARK
#define SO_RCVMARK 75
#endif
#ifndef SO_PASSPIDFD
#define SO_PASSPIDFD 76
#endif
#ifndef SO_PEERPIDFD
#define SO_PEERPIDFD 77
#endif

int recvmsg32(int fd, x86_msghdr* guest_msghdr, int flags) {
    struct msghdr host_msghdr;
    host_msghdr.msg_flags = guest_msghdr->msg_flags;
    host_msghdr.msg_name = (void*)(u64)guest_msghdr->msg_name;
    host_msghdr.msg_namelen = guest_msghdr->msg_namelen;

    x86_iovec* iovecs32 = (x86_iovec*)(u64)guest_msghdr->msg_iov;
    std::vector<iovec> iovecs(iovecs32, iovecs32 + guest_msghdr->msg_iovlen);
    host_msghdr.msg_iov = iovecs.data();
    host_msghdr.msg_iovlen = guest_msghdr->msg_iovlen;

    constexpr size_t cmsghdr_size_difference = sizeof(cmsghdr) - sizeof(x86_cmsghdr);
    host_msghdr.msg_control = alloca(guest_msghdr->msg_controllen * 2);
    host_msghdr.msg_controllen = guest_msghdr->msg_controllen * 2;

    int result = ::recvmsg(fd, &host_msghdr, flags);
    if (result != -1) {
        for (u32 i = 0; i < guest_msghdr->msg_iovlen; i++) {
            x86_iovec* guest_iovec = (x86_iovec*)(guest_msghdr->msg_iov + (i * sizeof(x86_iovec)));
            *guest_iovec = host_msghdr.msg_iov[i];
        }

        guest_msghdr->msg_namelen = host_msghdr.msg_namelen;
        guest_msghdr->msg_controllen = 0;
        guest_msghdr->msg_flags = host_msghdr.msg_flags;

        if (host_msghdr.msg_controllen != 0) {
            u64 guest_cmsghdr_pointer = guest_msghdr->msg_control;

            for (cmsghdr* host_cmsghdr = CMSG_FIRSTHDR(&host_msghdr); host_cmsghdr != nullptr;
                 host_cmsghdr = CMSG_NXTHDR(&host_msghdr, host_cmsghdr)) {
                x86_cmsghdr* guest_cmsghdr = (x86_cmsghdr*)guest_cmsghdr_pointer;
                guest_cmsghdr->cmsg_level = host_cmsghdr->cmsg_level;
                guest_cmsghdr->cmsg_type = host_cmsghdr->cmsg_type;

                if (host_cmsghdr->cmsg_len != 0) {
                    guest_cmsghdr->cmsg_len = host_cmsghdr->cmsg_len - cmsghdr_size_difference;
                    guest_msghdr->msg_controllen += guest_cmsghdr->cmsg_len;
                    memcpy(guest_cmsghdr->cmsg_data, CMSG_DATA(host_cmsghdr), host_cmsghdr->cmsg_len - sizeof(cmsghdr));
                    guest_cmsghdr_pointer += guest_cmsghdr->cmsg_len;
                    guest_cmsghdr_pointer = (guest_cmsghdr_pointer + 3) & ~0b11ull;
                }
            }
        }
    }
    return result;
}

int sendmsg32(int fd, const x86_msghdr* guest_msghdr, int flags) {
    struct msghdr host_msghdr;
    host_msghdr.msg_flags = guest_msghdr->msg_flags;
    host_msghdr.msg_name = (void*)(u64)guest_msghdr->msg_name;
    host_msghdr.msg_namelen = guest_msghdr->msg_namelen;

    x86_iovec* iovecs32 = (x86_iovec*)(u64)guest_msghdr->msg_iov;
    std::vector<iovec> iovecs(iovecs32, iovecs32 + guest_msghdr->msg_iovlen);
    host_msghdr.msg_iov = iovecs.data();
    host_msghdr.msg_iovlen = guest_msghdr->msg_iovlen;

    constexpr size_t cmsghdr_size_difference = sizeof(cmsghdr) - sizeof(x86_cmsghdr);
    host_msghdr.msg_control = alloca(guest_msghdr->msg_controllen * 2);
    host_msghdr.msg_controllen = guest_msghdr->msg_controllen;

    if (guest_msghdr->msg_controllen != 0) {
        u64 guest_cmsghdr_pointer = guest_msghdr->msg_control;
        u64 host_cmsghdr_pointer = (u64)host_msghdr.msg_control;

        while (true) {
            x86_cmsghdr* guest_cmsghdr = (x86_cmsghdr*)guest_cmsghdr_pointer;
            cmsghdr* host_cmsghdr = (cmsghdr*)host_cmsghdr_pointer;

            host_cmsghdr->cmsg_level = guest_cmsghdr->cmsg_level;
            host_cmsghdr->cmsg_type = guest_cmsghdr->cmsg_type;

            if (guest_cmsghdr->cmsg_len) {
                host_cmsghdr->cmsg_len = guest_cmsghdr->cmsg_len + cmsghdr_size_difference;
                host_msghdr.msg_controllen += cmsghdr_size_difference;
                memcpy(CMSG_DATA(host_cmsghdr), guest_cmsghdr->cmsg_data, guest_cmsghdr->cmsg_len - sizeof(x86_cmsghdr));
            }

            host_cmsghdr_pointer = (u64)CMSG_NXTHDR(&host_msghdr, host_cmsghdr);

            if (guest_cmsghdr->cmsg_len < sizeof(x86_cmsghdr)) {
                break;
            } else {
                guest_cmsghdr_pointer += guest_cmsghdr->cmsg_len;
                guest_cmsghdr_pointer = (guest_cmsghdr_pointer + 3) & ~0b11ull;

                if (guest_cmsghdr_pointer >= guest_msghdr->msg_control + guest_msghdr->msg_controllen) {
                    break;
                }
            }
        }
    }

    return ::sendmsg(fd, &host_msghdr, flags);
}

// Thanks FEX-Emu for these
int getsockopt32(int fd, int level, int optname, char* optval, u32* optlen) {
    if (level != SOL_SOCKET) {
        return ::getsockopt(fd, level, optname, optval, optlen);
    } else {
        switch (optname) {
        case SO_DEBUG:
        case SO_REUSEADDR:
        case SO_TYPE:
        case SO_ERROR:
        case SO_DONTROUTE:
        case SO_BROADCAST:
        case SO_SNDBUF:
        case SO_RCVBUF:
        case SO_SNDBUFFORCE:
        case SO_RCVBUFFORCE:
        case SO_KEEPALIVE:
        case SO_OOBINLINE:
        case SO_NO_CHECK:
        case SO_PRIORITY:
        case SO_LINGER:
        case SO_BSDCOMPAT:
        case SO_REUSEPORT:
        case SO_PASSCRED:
        case SO_PEERCRED:
        case SO_RCVLOWAT:
        case SO_SNDLOWAT:
        case SO_SECURITY_AUTHENTICATION:
        case SO_SECURITY_ENCRYPTION_TRANSPORT:
        case SO_SECURITY_ENCRYPTION_NETWORK:
        case SO_ATTACH_FILTER:
        case SO_DETACH_FILTER:
        case SO_PEERNAME:
        case SO_TIMESTAMP_OLD:
        case SO_ACCEPTCONN:
        case SO_PEERSEC:
        case SO_PASSSEC:
        case SO_TIMESTAMPNS_OLD:
        case SO_MARK:
        case SO_TIMESTAMPING_OLD:
        case SO_PROTOCOL:
        case SO_DOMAIN:
        case SO_RXQ_OVFL:
        case SO_WIFI_STATUS:
        case SO_PEEK_OFF:
        case SO_NOFCS:
        case SO_LOCK_FILTER:
        case SO_SELECT_ERR_QUEUE:
        case SO_BUSY_POLL:
        case SO_MAX_PACING_RATE:
        case SO_BPF_EXTENSIONS:
        case SO_INCOMING_CPU:
        case SO_ATTACH_BPF:
        case SO_ATTACH_REUSEPORT_CBPF:
        case SO_ATTACH_REUSEPORT_EBPF:
        case SO_CNX_ADVICE:
        case SO_MEMINFO:
        case SO_INCOMING_NAPI_ID:
        case SO_COOKIE:
        case SO_PEERGROUPS:
        case SO_ZEROCOPY:
        case SO_TXTIME:
        case SO_BINDTOIFINDEX:
        case SO_TIMESTAMP_NEW:
        case SO_TIMESTAMPNS_NEW:
        case SO_TIMESTAMPING_NEW:
        case SO_RCVTIMEO_NEW:
        case SO_SNDTIMEO_NEW:
        case SO_DETACH_REUSEPORT_BPF:
        case SO_PREFER_BUSY_POLL:
        case SO_BUSY_POLL_BUDGET:
        case SO_NETNS_COOKIE:
        case SO_BUF_LOCK:
        case SO_RESERVE_MEM: {
            return ::getsockopt(fd, level, optname, optval, optlen);
        }
        default: {
            ERROR("Unhandled getsockopt optname: %d", optname);
            return -ENOSYS;
        }
        }
    }
}

int setsockopt32(int fd, int level, int optname, char* optval, int optlen) {
    if (level != SOL_SOCKET) {
        return ::setsockopt(fd, level, optname, optval, optlen);
    } else {
        switch (optname) {
        case SO_DEBUG:
        case SO_REUSEADDR:
        case SO_TYPE:
        case SO_ERROR:
        case SO_DONTROUTE:
        case SO_BROADCAST:
        case SO_SNDBUF:
        case SO_RCVBUF:
        case SO_SNDBUFFORCE:
        case SO_RCVBUFFORCE:
        case SO_KEEPALIVE:
        case SO_OOBINLINE:
        case SO_NO_CHECK:
        case SO_PRIORITY:
        case SO_LINGER:
        case SO_BSDCOMPAT:
        case SO_REUSEPORT:
        case SO_PASSCRED:
        case SO_PEERCRED:
        case SO_RCVLOWAT:
        case SO_SNDLOWAT:
        case SO_SECURITY_AUTHENTICATION:
        case SO_SECURITY_ENCRYPTION_TRANSPORT:
        case SO_SECURITY_ENCRYPTION_NETWORK:
        case SO_DETACH_FILTER:
        case SO_PEERNAME:
        case SO_TIMESTAMP_OLD:
        case SO_ACCEPTCONN:
        case SO_PEERSEC:
        case SO_PASSSEC:
        case SO_TIMESTAMPNS_OLD:
        case SO_MARK:
        case SO_TIMESTAMPING_OLD:
        case SO_PROTOCOL:
        case SO_DOMAIN:
        case SO_RXQ_OVFL:
        case SO_WIFI_STATUS:
        case SO_PEEK_OFF:
        case SO_NOFCS:
        case SO_LOCK_FILTER:
        case SO_SELECT_ERR_QUEUE:
        case SO_BUSY_POLL:
        case SO_MAX_PACING_RATE:
        case SO_BPF_EXTENSIONS:
        case SO_INCOMING_CPU:
        case SO_ATTACH_BPF:
        case SO_ATTACH_REUSEPORT_EBPF:
        case SO_CNX_ADVICE:
        case SO_MEMINFO:
        case SO_INCOMING_NAPI_ID:
        case SO_COOKIE:
        case SO_PEERGROUPS:
        case SO_ZEROCOPY:
        case SO_TXTIME:
        case SO_BINDTOIFINDEX:
        case SO_TIMESTAMP_NEW:
        case SO_TIMESTAMPNS_NEW:
        case SO_TIMESTAMPING_NEW:
        case SO_RCVTIMEO_NEW:
        case SO_SNDTIMEO_NEW:
        case SO_DETACH_REUSEPORT_BPF:
        case SO_PREFER_BUSY_POLL:
        case SO_BUSY_POLL_BUDGET:
        case SO_NETNS_COOKIE:
        case SO_BUF_LOCK:
        case SO_RESERVE_MEM:
        case SO_TXREHASH:
        case SO_RCVMARK:
        case SO_PASSPIDFD:
        case SO_PEERPIDFD: {
            return ::setsockopt(fd, level, optname, optval, optlen);
        }
        default: {
            ERROR("Unhandled setsockopt optname: %d", optname);
            return -ENOSYS;
        }
        }
    }
}