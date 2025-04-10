#include <cstring>
#include <sys/socket.h>
#include "felix86/hle/socket.hpp"

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
    host_msghdr.msg_controllen = 0;

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
                host_msghdr.msg_controllen += host_cmsghdr->cmsg_len;
                memcpy(CMSG_DATA(host_cmsghdr), guest_cmsghdr->cmsg_data, guest_cmsghdr->cmsg_len - sizeof(x86_cmsghdr));
            }

            host_cmsghdr_pointer = (u64)CMSG_NXTHDR(&host_msghdr, host_cmsghdr);

            if (guest_cmsghdr->cmsg_len < sizeof(x86_cmsghdr)) {
                break;
            } else {
                guest_cmsghdr_pointer += guest_cmsghdr->cmsg_len;
                guest_cmsghdr_pointer = (guest_cmsghdr_pointer + 3) & ~0b11ull;

                if (guest_cmsghdr_pointer > guest_msghdr->msg_control + guest_msghdr->msg_controllen) {
                    break;
                }
            }
        }
    }

    return ::sendmsg(fd, &host_msghdr, flags);
}
