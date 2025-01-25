#include <csignal>
#include <errno.h>
#include <fcntl.h>
#include <fmt/format.h>
#include <poll.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <termios.h>
#undef VMIN
#include <unistd.h>
#include "felix86/common/debug.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/x86.hpp"
#include "felix86/emulator.hpp"
#include "felix86/hle/filesystem.hpp"
#include "felix86/hle/stat.hpp"
#include "felix86/hle/syscall.hpp"
#include "felix86/hle/thread.hpp"

// We add felix86_${ARCH}_ in front of the linux related identifiers to avoid
// naming conflicts

#define felix86_x86_64_ARCH_SET_GS 0x1001
#define felix86_x86_64_ARCH_SET_FS 0x1002
#define felix86_x86_64_ARCH_GET_FS 0x1003
#define felix86_x86_64_ARCH_GET_GS 0x1004

#define HOST_SYSCALL(name, ...) (syscall(match_host(felix86_x86_64_##name), ##__VA_ARGS__))

enum {

#define X(name, id) felix86_x86_64_##name = id,
#include "felix86/hle/syscalls_x86_64.inc"
#undef X
};

enum {

#define X(name, id) felix86_riscv64_##name = id,
#include "felix86/hle/syscalls_riscv64.inc"
#undef X
};

consteval int match_host(int syscall) {
#define X(name)                                                                                                                                      \
    case felix86_x86_64_##name:                                                                                                                      \
        return felix86_riscv64_##name;
    switch (syscall) {
#include "felix86/hle/syscalls_common.inc"
#undef X
    default:
        ERROR("Host syscall not found: %d", syscall);
        return -1;
    }
#undef X
}

static_assert(match_host(felix86_x86_64_setxattr) == felix86_riscv64_setxattr);

const char* print_syscall_name(u64 syscall_number) {
    switch (syscall_number) {
#define X(name, id)                                                                                                                                  \
    case id:                                                                                                                                         \
        return #name;
#include "felix86/hle/syscalls_x86_64.inc"
#undef X
    default:
        return "Unknown";
    }
}

bool detecting_memory_region = false;
std::string name = {};
u64 min_address = ULONG_MAX;
u64 max_address = 0;

void felix86_syscall(ThreadState* state) {
    u64 syscall_number = state->GetGpr(X86_REF_RAX);
    u64 rdi = state->GetGpr(X86_REF_RDI);
    u64 rsi = state->GetGpr(X86_REF_RSI);
    u64 rdx = state->GetGpr(X86_REF_RDX);
    u64 r10 = state->GetGpr(X86_REF_R10);
    u64 r8 = state->GetGpr(X86_REF_R8);
    u64 r9 = state->GetGpr(X86_REF_R9);
    ssize_t result = -1;

    Filesystem& fs = g_emulator->GetFilesystem();

    switch (syscall_number) {
    case felix86_x86_64_brk: {
        if (rdi == 0) {
            result = state->brk_current_address;
        } else {
            state->brk_current_address = rdi;
            result = state->brk_current_address;
        }
        STRACE("brk(%p) = %p", (void*)rdi, (void*)result);
        break;
    }
    case felix86_x86_64_arch_prctl: {
        switch (rdi) {
        case felix86_x86_64_ARCH_SET_GS: {
            state->gsbase = rsi;
            result = 0;
            break;
        }
        case felix86_x86_64_ARCH_SET_FS: {
            state->fsbase = rsi;
            result = 0;
            break;
        }
        case felix86_x86_64_ARCH_GET_FS: {
            result = state->fsbase;
            break;
        }
        case felix86_x86_64_ARCH_GET_GS: {
            result = state->gsbase;
            break;
        }
        default: {
            ERROR("Unimplemented arch_prctl %016lx", rdi);
            break;
        }
        }
        STRACE("arch_prctl(%016lx, %016lx) = %016lx", rdi, rsi, result);
        break;
    }
    case felix86_x86_64_set_tid_address: {
        state->clear_child_tid = rdi;
        result = rdi;
        STRACE("set_tid_address(%016lx) = %016lx", rdi, result);
        break;
    }
    case felix86_x86_64_get_robust_list: {
        if (rdi != 0) {
            ERROR("get_robust_list asking for a different thread not implemented");
        }
        ERROR("get_robust_list not implemented");
        break;
    }
    case felix86_x86_64_set_robust_list: {
        // state->robust_futex_list = rdi;
        // if (rsi != sizeof(u64) * 3) {
        //     WARN("Struct size is wrong during set_robust_list");
        //     result = -EINVAL;
        // }
        // STRACE("set_robust_list(%016lx, %016lx) = %016lx", rdi, rsi, result);
        result = -ENOSYS;
        break;
    }
    case felix86_x86_64_rseq: {
        // Couldn't find any solid documentation and FEX doesn't support it either
        result = -ENOSYS;
        STRACE("rseq(...) = %016lx", result);
        break;
    }
    case felix86_x86_64_time: {
        result = ::time((time_t*)rdi);
        STRACE("time(%p) = %016lx", (void*)rdi, result);
        break;
    }
    case felix86_x86_64_prlimit64: {
        result = HOST_SYSCALL(prlimit64, rdi, rsi, rdx, r10);
        STRACE("prlimit64(%016lx, %016lx, %016lx, %016lx) = %016lx", rdi, rsi, rdx, r10, result);
        break;
    }
    case felix86_x86_64_readlink: {
        result = fs.ReadLink((const char*)rdi, (char*)rsi, rdx);
        STRACE("readlink(%s, %s, %d) = %d", (const char*)rdi, (char*)rsi, (int)rdx, (int)result);
        break;
    }
    case felix86_x86_64_readlinkat: {
        result = fs.ReadLinkAt(rdi, (const char*)rsi, (char*)rdx, r10);
        STRACE("readlinkat(%d, %s, %s, %d) = %d", (int)rdi, (const char*)rsi, (char*)rdx, (int)r10, (int)result);
        break;
    }
    case felix86_x86_64_getrandom: {
        result = HOST_SYSCALL(getrandom, rdi, rsi, rdx);
        STRACE("getrandom(%p, %016lx, %d) = %016lx", (void*)rdi, rsi, (int)rdx, result);
        break;
    }
    case felix86_x86_64_mprotect: {
        result = HOST_SYSCALL(mprotect, rdi, rsi, rdx);
        STRACE("mprotect(%p, %016lx, %d) = %016lx", (void*)rdi, rsi, (int)rdx, result);
        break;
    }
    case felix86_x86_64_close: {
        // Don't close our stdout
        // TODO: better implementation where it closes an emulated stdout instead
        if (rdi != 1 && rdi != 2) {
            result = HOST_SYSCALL(close, rdi);
        } else {
            result = 0;
        }
        STRACE("close(%d) = %d", (int)rdi, (int)result);
        if (detecting_memory_region && MemoryMetadata::IsInInterpreterRegion(state->rip)) {
            detecting_memory_region = false;
            ASSERT(result != -1);
            MemoryMetadata::AddRegion(name, min_address, max_address);
        }
        break;
    }
    case felix86_x86_64_setpgid: {
        result = HOST_SYSCALL(setpgid, rdi, rsi);
        STRACE("setpgid(%d, %d) = %d", (int)rdi, (int)rsi, (int)result);
        break;
    }
    case felix86_x86_64_getrusage: {
        result = HOST_SYSCALL(getrusage, rdi, (struct rusage*)rsi);
        STRACE("getrusage(%d, %p) = %d", (int)rdi, (void*)rsi, (int)result);
        break;
    }
    case felix86_x86_64_getpgrp: {
        result = getpgrp();
        STRACE("getpgrp() = %d", (int)result);
        break;
    }
    case felix86_x86_64_getcwd: {
        result = fs.GetCwd((char*)rdi, rsi);
        STRACE("getcwd(%p, %d) = %d", (void*)rdi, (int)rsi, (int)result);
        break;
    }
    case felix86_x86_64_poll: {
        result = poll((struct pollfd*)rdi, rsi, rdx);
        STRACE("poll(%p, %d, %d) = %d", (void*)rdi, (int)rsi, (int)rdx, (int)result);
        break;
    }
    case felix86_x86_64_clock_gettime: {
        result = HOST_SYSCALL(clock_gettime, rdi, (struct timespec*)rsi);
        STRACE("clock_gettime(%d, %p) = %d", (int)rdi, (void*)rsi, (int)result);
        break;
    }
    case felix86_x86_64_gettimeofday: {
        result = HOST_SYSCALL(gettimeofday, (struct timeval*)rdi, (struct timezone*)rsi);
        STRACE("gettimeofday(%p, %p) = %d", (void*)rdi, (void*)rsi, (int)result);
        break;
    }
    case felix86_x86_64_dup: {
        result = HOST_SYSCALL(dup, rdi);
        STRACE("dup(%d) = %d", (int)rdi, (int)result);
        break;
    }
    case felix86_x86_64_dup2: {
        result = dup2(rdi, rsi);
        STRACE("dup2(%d, %d) = %d", (int)rdi, (int)rsi, (int)result);
        break;
    }
    case felix86_x86_64_dup3: {
        result = HOST_SYSCALL(dup3, rdi, rsi, rdx);
        STRACE("dup3(%d, %d, %d) = %d", (int)rdi, (int)rsi, (int)rdx, (int)result);
        break;
    }
    case felix86_x86_64_fstat: {
        x64Stat* guest_stat = (x64Stat*)rsi;
        struct stat host_stat;
        result = HOST_SYSCALL(fstat, rdi, &host_stat);
        STRACE("fstat(%d, %p) = %d", (int)rdi, (void*)rsi, (int)result);
        if (result != -1) {
            *guest_stat = host_stat;
            STRACE("st_dev : %ld", guest_stat->st_dev);
            STRACE("st_ino : %ld", guest_stat->st_ino);
            STRACE("st_mode : %d", guest_stat->st_mode);
            STRACE("st_nlink : %ld", guest_stat->st_nlink);
            STRACE("st_uid : %d", guest_stat->st_uid);
            STRACE("st_gid : %d", guest_stat->st_gid);
            STRACE("st_rdev : %ld", guest_stat->st_rdev);
            STRACE("st_size : %ld", guest_stat->st_size);
            STRACE("st_blksize : %ld", guest_stat->st_blksize);
            STRACE("st_blocks : %ld", guest_stat->st_blocks);
        } else {
            STRACE("fstat failed: %d", errno);
        }
        break;
    }
    case felix86_x86_64_statx: {
        result = fs.Statx(rdi, (const char*)rsi, rdx, r10, (struct statx*)r8);
        STRACE("statx(%d, %s, %d, %d, %d) = %d", (int)rdi, (const char*)rsi, (int)rdx, (int)r10, (int)r8, (int)result);
        break;
    }
    case felix86_x86_64_fadvise64: {
        result = HOST_SYSCALL(fadvise64, rdi, rsi, rdx, r10);
        STRACE("fadvise64(%d, %d, %d, %d) = %d", (int)rdi, (int)rsi, (int)rdx, (int)r10, (int)result);
        break;
    }
    case felix86_x86_64_fcntl: {
        result = HOST_SYSCALL(fcntl, rdi, rsi, rdx);
        STRACE("fcntl(%d, %d, %d) = %d", (int)rdi, (int)rsi, (int)rdx, (int)result);
        break;
    }
    case felix86_x86_64_pselect6: {
        result = HOST_SYSCALL(pselect6, rdi, rsi, rdx, r10, r8, r9);
        STRACE("pselect6(%d, %p, %p, %p, %p, %p) = %d", (int)rdi, (void*)rsi, (void*)rdx, (void*)r10, (void*)r8, (void*)r9, (int)result);
        break;
    }
    case felix86_x86_64_chdir: {
        result = fs.Chdir((const char*)rdi);
        STRACE("chdir(%s) = %d", (const char*)rdi, (int)result);
        break;
    }
    case felix86_x86_64_fchdir: {
        result = HOST_SYSCALL(fchdir, rdi);
        STRACE("fchdir(%d) = %d", (int)rdi, (int)result);
        break;
    }
    case felix86_x86_64_newfstatat: {
        std::optional<std::filesystem::path> path = fs.AtPath(rdi, (const char*)rsi);

        if (!path) {
            result = -EACCES;
            break;
        }

        x64Stat* guest_stat = (x64Stat*)rdx;
        struct stat host_stat;
        result = HOST_SYSCALL(newfstatat, rdi, path->c_str(), &host_stat, r10);
        STRACE("newfstatat(%d, %s, %p, %d) = %d", (int)rdi, path->c_str(), (void*)rdx, (int)r10, (int)result);
        if (result != -1) {
            *guest_stat = host_stat;
        }
        break;
    }
    case felix86_x86_64_sigaltstack: {
        result = HOST_SYSCALL(sigaltstack, rdi, rsi);
        STRACE("sigaltstack(%p, %p) = %d", (void*)rdi, (void*)rsi, (int)result);
        break;
    }
    case felix86_x86_64_sysinfo: {
        result = HOST_SYSCALL(sysinfo, rdi);
        STRACE("sysinfo(%p) = %d", (void*)rdi, (int)result);
        break;
    }
    case felix86_x86_64_ioctl: {
        result = HOST_SYSCALL(ioctl, rdi, rsi, rdx);
        STRACE("ioctl(%d, %016lx, %016lx) = %016lx", (int)rdi, rsi, rdx, result);

#if 1
        if (g_strace) {
            // TCSETSW
            termios* term = (termios*)rdx;
            switch (rsi) {
            case TCSETS:
            case TCSETSW:
            case TCSETSF: {
                auto oflag = term->c_oflag;
                // todo, output the flag properties
            }
            }
        }
#endif
        break;
    }
    case felix86_x86_64_write: {
        result = HOST_SYSCALL(write, rdi, rsi, rdx);

        if (g_strace) {
            STRACE("write(%d, %s, %d) = %d", (int)rdi, (char*)rsi, (int)rdx, (int)result);
        }
        break;
    }
    case felix86_x86_64_writev: {
        result = HOST_SYSCALL(writev, rdi, rsi, rdx);
        STRACE("writev(%d, %p, %d) = %d", (int)rdi, (void*)rsi, (int)rdx, (int)result);
        break;
    }
    case felix86_x86_64_exit_group: {
        VERBOSE("Emulator called exit_group(%d)", (int)rdi);
        STRACE("exit_group(%d)", (int)rdi);
        // TODO: can we make felix into a child process and exit that instead of exiting the entire thing instead?
        // result = HOST_SYSCALL(exit_group, rdi);
        exit(0);
        break;
    }
    case felix86_x86_64_access: {
        result = fs.FAccessAt(AT_FDCWD, (const char*)rdi, rsi, 0);
        STRACE("access(%s, %d) = %d", (const char*)rdi, (int)rsi, (int)result);
        break;
    }
    case felix86_x86_64_faccessat:
    case felix86_x86_64_faccessat2: {
        result = fs.FAccessAt(rdi, (const char*)rsi, rdx, r10);
        STRACE("faccessat2(%d, %s, %d, %d) = %d", (int)rdi, (const char*)rsi, (int)rdx, (int)r10, (int)result);
        break;
    }
    case felix86_x86_64_pipe2: {
        result = HOST_SYSCALL(pipe2, rdi, rsi);
        STRACE("pipe2(%p, %d) = %d", (void*)rdi, (int)rsi, (int)result);
        break;
    }
    case felix86_x86_64_memfd_create: {
        result = HOST_SYSCALL(memfd_create, (const char*)rdi, rsi);
        STRACE("memfd_create(%s, %d) = %d", (const char*)rdi, (int)rsi, (int)result);
        break;
    }
    case felix86_x86_64_ftruncate: {
        result = HOST_SYSCALL(ftruncate, rdi, rsi);
        STRACE("ftruncate(%d, %d) = %d", (int)rdi, (int)rsi, (int)result);
        break;
    }
    case felix86_x86_64_pipe: {
        result = pipe((int*)rdi);
        STRACE("pipe(%p) = %d", (void*)rdi, (int)result);
        break;
    }
    case felix86_x86_64_read: {
        result = HOST_SYSCALL(read, rdi, rsi, rdx);
        STRACE("read(%d, %p, %d) = %d", (int)rdi, (void*)rsi, (int)rdx, (int)result);
        break;
    }
    case felix86_x86_64_getdents64: {
        result = HOST_SYSCALL(getdents64, rdi, rsi, rdx);
        STRACE("getdents64(%d, %p, %d) = %d", (int)rdi, (void*)rsi, (int)rdx, (int)result);
        break;
    }
    case felix86_x86_64_mkdir: {
        auto path = fs.AtPath(AT_FDCWD, (const char*)rdi);

        if (!path) {
            result = -EACCES;
            break;
        }

        result = mkdir(path->c_str(), rsi);
        STRACE("mkdir(%s, %d) = %d", (char*)rdi, (int)rsi, (int)result);
        break;
    }
    case felix86_x86_64_openat: {
        result = fs.OpenAt(rdi, (const char*)rsi, rdx, r10);
        STRACE("openat(%d, %s, %d, %d) = %d", (int)rdi, (const char*)rsi, (int)rdx, (int)r10, (int)result);

        if (MemoryMetadata::IsInInterpreterRegion(state->rip)) {
            name = std::filesystem::path((const char*)rsi).filename().string();

            if (name.find(".so") != std::string::npos) {
                detecting_memory_region = true;
                min_address = ULONG_MAX;
                max_address = 0;
            } else {
                name = {};
            }
        }
        break;
    }
    case felix86_x86_64_pread64: {
        result = HOST_SYSCALL(pread64, rdi, rsi, rdx, r10);
        STRACE("pread64(%d, %p, %d, %d) = %d", (int)rdi, (void*)rsi, (int)rdx, (int)r10, (int)result);
        break;
    }
    case felix86_x86_64_mmap: {
        result = HOST_SYSCALL(mmap, rdi, rsi, rdx, r10, r8, r9);
        STRACE("mmap(%p, %016lx, %d, %d, %d, %d) = %016lx", (void*)rdi, rsi, (int)rdx, (int)r10, (int)r8, (int)r9, result);

        if (detecting_memory_region && MemoryMetadata::IsInInterpreterRegion(state->rip)) {
            if (result < min_address) {
                min_address = result;
            }
            if (result + rsi > max_address) {
                max_address = result + rsi;
            }
        }
        break;
    }
    case felix86_x86_64_munmap: {
        result = HOST_SYSCALL(munmap, rdi, rsi);
        STRACE("munmap(%p, %016lx) = %016lx", (void*)rdi, rsi, result);
        break;
    }
    case felix86_x86_64_getuid: {
        result = HOST_SYSCALL(getuid);
        STRACE("getuid() = %d", (int)result);
        break;
    }
    case felix86_x86_64_geteuid: {
        result = HOST_SYSCALL(geteuid);
        STRACE("geteuid() = %d", (int)result);
        break;
    }
    case felix86_x86_64_getegid: {
        result = HOST_SYSCALL(getegid);
        STRACE("getegid() = %d", (int)result);
        break;
    }
    case felix86_x86_64_getgid: {
        result = HOST_SYSCALL(getgid);
        STRACE("getgid() = %d", (int)result);
        break;
    }
    case felix86_x86_64_setfsgid: {
        result = HOST_SYSCALL(setfsgid, rdi);
        STRACE("setfsgid(%d) = %d", (int)rdi, (int)result);
        break;
    }
    case felix86_x86_64_setfsuid: {
        result = HOST_SYSCALL(setfsuid, rdi);
        STRACE("setfsuid(%d) = %d", (int)rdi, (int)result);
        break;
    }
    case felix86_x86_64_getppid: {
        result = HOST_SYSCALL(getppid);
        STRACE("getppid() = %d", (int)result);
        break;
    }
    case felix86_x86_64_getpid: {
        result = HOST_SYSCALL(getpid);
        STRACE("getpid() = %d", (int)result);
        break;
    }
    case felix86_x86_64_gettid: {
        result = HOST_SYSCALL(gettid);
        STRACE("gettid() = %d", (int)result);
        break;
    }
    case felix86_x86_64_socket: {
        result = HOST_SYSCALL(socket, rdi, rsi, rdx);
        STRACE("socket(%d, %d, %d) = %d", (int)rdi, (int)rsi, (int)rdx, (int)result);
        break;
    }
    case felix86_x86_64_connect: {
        result = HOST_SYSCALL(connect, rdi, rsi, rdx);
        STRACE("connect(%d, %p, %d) = %d", (int)rdi, (void*)rsi, (int)rdx, (int)result);
        break;
    }
    case felix86_x86_64_sendto: {
        result = HOST_SYSCALL(sendto, rdi, rsi, rdx, r10, r8, r9);
        STRACE("sendto(%d, %p, %d, %d, %p, %d) = %d", (int)rdi, (void*)rsi, (int)rdx, (int)r10, (void*)r8, (int)r9, (int)result);
        break;
    }
    case felix86_x86_64_recvfrom: {
        result = HOST_SYSCALL(recvfrom, rdi, rsi, rdx, r10, r8, r9);
        STRACE("recvfrom(%d, %p, %d, %d, %p, %p) = %d", (int)rdi, (void*)rsi, (int)rdx, (int)r10, (void*)r8, (void*)r9, (int)result);
        break;
    }
    case felix86_x86_64_lseek: {
        result = HOST_SYSCALL(lseek, rdi, rsi, rdx);
        STRACE("lseek(%d, %d, %d) = %d", (int)rdi, (int)rsi, (int)rdx, (int)result);
        break;
    }
    case felix86_x86_64_uname: {
        struct utsname host_uname;
        struct utsname* guest_uname = (struct utsname*)rdi;
        if (uname(&host_uname) == 0) {
            memcpy(guest_uname->nodename, host_uname.nodename, sizeof(host_uname.nodename));
            memcpy(guest_uname->domainname, host_uname.domainname, sizeof(host_uname.domainname));
        } else {
            strcpy(guest_uname->nodename, "felix86");
            WARN("Failed to determine host node name");
        }
        strcpy(guest_uname->sysname, "Linux");
        strcpy(guest_uname->release, "5.0.0");
        std::string version = "#1 SMP " __DATE__ " " __TIME__;
        strcpy(guest_uname->version, version.c_str());
        strcpy(guest_uname->machine, "x86_64");
        result = 0;
        break;
    }
    case felix86_x86_64_statfs: {
        std::optional<std::filesystem::path> path = fs.AtPath(AT_FDCWD, (const char*)rdi);

        if (!path) {
            result = -EACCES;
            break;
        }

        result = HOST_SYSCALL(statfs, path->c_str(), (struct statfs*)rsi);
        STRACE("statfs(%s, %p) = %d", path->c_str(), (void*)rsi, (int)result);
        break;
    }
    case felix86_x86_64_getsockname: {
        result = HOST_SYSCALL(getsockname, rdi, (struct sockaddr*)rsi, (socklen_t*)rdx);
        STRACE("getsockname(%d, %p, %p) = %d", (int)rdi, (void*)rsi, (void*)rdx, (int)result);
        break;
    }
    case felix86_x86_64_recvmsg: {
        result = HOST_SYSCALL(recvmsg, rdi, (struct msghdr*)rsi, rdx);
        STRACE("recvmsg(%d, %p, %d) = %d", (int)rdi, (void*)rsi, (int)rdx, (int)result);
        break;
    }
    case felix86_x86_64_sendmsg: {
        result = HOST_SYSCALL(sendmsg, rdi, (struct msghdr*)rsi, rdx);
        STRACE("sendmsg(%d, %p, %d) = %d", (int)rdi, (void*)rsi, (int)rdx, (int)result);
        break;
    }
    case felix86_x86_64_flock: {
        result = HOST_SYSCALL(flock, rdi, rsi);
        STRACE("flock(%d, %d) = %d", (int)rdi, (int)rsi, (int)result);
        break;
    }
    case felix86_x86_64_rt_sigaction: {
        struct sigaction* act = (struct sigaction*)rsi;
        if (act) {
            bool sigaction = act->sa_flags & SA_SIGINFO;
            void* handler = sigaction ? (void*)act->sa_sigaction : (void*)act->sa_handler;
            Signals::registerSignalHandler(rdi, handler, act->sa_mask, act->sa_flags);
        }

        struct sigaction* old_act = (struct sigaction*)rdx;
        if (old_act) {
            RegisteredSignal old = Signals::getSignalHandler(rdi);
            bool was_sigaction = old.flags & SA_SIGINFO;
            if (was_sigaction) {
                old_act->sa_sigaction = (decltype(old_act->sa_sigaction))old.handler;
            } else {
                old_act->sa_handler = (decltype(old_act->sa_handler))old.handler;
            }
            old_act->sa_flags = old.flags;
            old_act->sa_mask = old.mask;
        }

        result = 0;
        WARN("rt_sigaction(%d, %p, %p) = %d", (int)rdi, (void*)rsi, (void*)r10, (int)result);
        STRACE("rt_sigaction(%d, %p, %p) = %d", (int)rdi, (void*)rsi, (void*)r10, (int)result);
        break;
    }
    case felix86_x86_64_prctl: {
#ifndef PR_GET_AUXV
#define PR_GET_AUXV 0x41555856
#endif
        int option = rdi;
        switch (option) {
        case PR_GET_AUXV: {
            if (r10 || r8) {
                result = -EINVAL;
            } else {
                void* addr = (void*)rsi;
                size_t size = rdx;
                auto [auxv_addr, auxv_size] = g_emulator->GetAuxv();
                size_t actual_size = std::min(size, auxv_size);
                memcpy(addr, auxv_addr, actual_size);
                result = actual_size;
            }
            break;
        }
        case PR_SET_SECCOMP:
        case PR_GET_SECCOMP: {
            WARN("prctl(SECCOMP) not implemented");
            result = -EINVAL;
            break;
        }
        default: {
            result = HOST_SYSCALL(prctl, rdi, rsi, rdx, r10, r8);
            break;
        }
        }
        STRACE("prctl(%d, %016lx, %016lx, %016lx, %016lx) = %016lx", (int)rdi, rsi, rdx, r10, r8, result);
        break;
    }
    case felix86_x86_64_futex: {
        result = HOST_SYSCALL(futex, rdi, rsi, rdx, r10, r8, r9);
        STRACE("futex(%p, %d, %d, %p, %p, %d) = %d", (void*)rdi, (int)rsi, (int)rdx, (void*)r10, (void*)r8, (int)r9, (int)result);
        break;
    }
    case felix86_x86_64_sched_getaffinity: {
        result = HOST_SYSCALL(sched_getaffinity, rdi, rsi, rdx);
        STRACE("sched_getaffinity(%d, %d, %p) = %d", (int)rdi, (int)rsi, (void*)rdx, (int)result);
        break;
    }
    case felix86_x86_64_mincore: {
        result = HOST_SYSCALL(mincore, rdi, rsi, rdx);
        STRACE("mincore(%p, %d, %p) = %d", (void*)rdi, (int)rsi, (void*)rdx, (int)result);
        break;
    }
    case felix86_x86_64_clone3: {
        clone_args args;
        memset(&args, 0, sizeof(clone_args));
        size_t size = std::min(rsi, sizeof(clone_args));
        memcpy(&args, (void*)rdi, size);

        if (args.flags & CLONE_CLEAR_SIGHAND) { // we don't support this
            result = -EINVAL;
            break;
        }

        long result = Threads::Clone3(state, &args);

        if (result != 0) { // Parent
            ERROR("parent");
        }

        ERROR("child");
        break;
    }
    case felix86_x86_64_clone: {
        clone_args args;
        memset(&args, 0, sizeof(clone_args));
        args.flags = rdi;
        args.stack = rsi;
        args.parent_tid = rdx;
        args.child_tid = r10;
        args.tls = r8;
        result = Threads::Clone(state, &args);
        break;
    }
    case felix86_x86_64_wait4: {
        result = HOST_SYSCALL(wait4, rdi, rsi, rdx, r10);
        STRACE("wait4(%d, %p, %d, %p) = %d", (int)rdi, (void*)rsi, (int)rdx, (void*)r10, (int)result);
        break;
    }
    case felix86_x86_64_unlink: {
        std::optional<std::filesystem::path> path = fs.AtPath(AT_FDCWD, (const char*)rdi);

        if (!path) {
            result = fs.Error();
            break;
        }

        unlink(path->c_str());
        result = 0;
        break;
    }
    case felix86_x86_64_getpeername: {
        result = HOST_SYSCALL(getpeername, rdi, (struct sockaddr*)rsi, (socklen_t*)rdx);
        STRACE("getpeername(%d, %p, %p) = %d", (int)rdi, (void*)rsi, (void*)rdx, (int)result);
        break;
    }
    case felix86_x86_64_rt_sigprocmask: {
        int how = rdi;
        sigset_t* set = (sigset_t*)rsi;
        sigset_t* oldset = (sigset_t*)rdx;

        if (set) {
            for (int i = 1; i <= 64; i++) {
                int res = sigismember(set, i);
                if (res == 1) {
                    if (how == SIG_BLOCK) {
                        state->SetSignalMask(i, true);
                    } else if (how == SIG_UNBLOCK) {
                        state->SetSignalMask(i, false);
                    } else if (how == SIG_SETMASK) {
                        state->SetSignalMask(i, true);
                    }
                } else if (res == 0) {
                    if (how == SIG_SETMASK) {
                        state->SetSignalMask(i, false);
                    }
                }
            }
        }

        if (oldset) {
            sigemptyset(oldset);
            for (int i = 1; i <= 64; i++) {
                if (state->GetSignalMask(i)) {
                    sigaddset(oldset, i);
                }
            }
        }

        if (how != SIG_BLOCK && how != SIG_UNBLOCK && how != SIG_SETMASK) {
            result = -EINVAL;
        } else {
            result = 0;
        }
        break;
    }
    default: {
        ERROR("Unimplemented syscall %s (%016lx)", print_syscall_name(syscall_number), syscall_number);
        break;
    }
    }

    if (result == -1) {
        result = -errno;
    }

    state->SetGpr(X86_REF_RAX, result);
}