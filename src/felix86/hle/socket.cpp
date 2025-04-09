#include <cstring>
#include <sys/socket.h>
#include "felix86/hle/socket.hpp"

int sendmsg32(int fd, const x86_msghdr* msg, int flags) {
    struct msghdr host_msghdr;
    host_msghdr.msg_flags = msg->msg_flags;
    host_msghdr.msg_name = (void*)(u64)msg->msg_name;
    host_msghdr.msg_namelen = msg->msg_namelen;

    x86_iovec* iovecs32 = (x86_iovec*)(u64)msg->msg_iov;
    std::vector<iovec> iovecs(iovecs32, iovecs32 + msg->msg_iovlen);
    host_msghdr.msg_iov = iovecs.data();
    host_msghdr.msg_iovlen = msg->msg_iovlen;

    constexpr size_t cmsghdr_size_difference = sizeof(cmsghdr) - sizeof(x86_cmsghdr);
    host_msghdr.msg_control = alloca(msg->msg_controllen * 2);
    host_msghdr.msg_controllen = 0;

    if (msg->msg_controllen) {
        u64 guest_cmsghdr_pointer = msg->msg_control;
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

                if (guest_cmsghdr_pointer > msg->msg_control + msg->msg_controllen) {
                    break;
                }
            }
        }
    }

    return ::sendmsg(fd, &host_msghdr, flags);
}
