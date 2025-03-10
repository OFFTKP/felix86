#include <csignal>
#include <errno.h>
#include <fcntl.h>
#include <fmt/format.h>
#include <linux/futex.h>
#include <poll.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <termios.h>
#undef VMIN
#include <unistd.h>
#include "felix86/common/log.hpp"
#include "felix86/common/state.hpp"
#include "felix86/emulator.hpp"
#include "felix86/hle/filesystem.hpp"
#include "felix86/hle/stat.hpp"
#include "felix86/hle/syscall.hpp"
#include "felix86/hle/thread.hpp"

// We add felix86_${ARCH}_ in front of the linux related identifiers to avoid
// naming conflicts

struct x86_sigaction {
    void (*handler)(int, siginfo_t*, void*);
    u64 sa_flags;
    void (*restorer)(void);
    sigset_t sa_mask;
};

#define felix86_x86_64_ARCH_SET_GS 0x1001
#define felix86_x86_64_ARCH_SET_FS 0x1002
#define felix86_x86_64_ARCH_GET_FS 0x1003
#define felix86_x86_64_ARCH_GET_GS 0x1004

#define HOST_SYSCALL(name, ...) (syscall(match_host(felix86_x86_64_##name), ##__VA_ARGS__))

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
std::filesystem::path region_path = {};
u64 min_address = ULONG_MAX;
u64 max_address = 0;

bool try_strace_ioctl(int rdi, u64 rsi, u64 rdx, u64 result) {
    if (!g_strace) {
        return false;
    }

    switch (rsi) {
    case TCGETS:
    case TCSETS:
    case TCSETSW: {
        termios term = *(termios*)rdx;
        std::string name;
        std::string c_iflag, c_oflag, c_cflag, c_lflag;
#define ADD(name, flag)                                                                                                                              \
    if (term.c_##name & flag) {                                                                                                                      \
        c_##name += #flag "|";                                                                                                                       \
        term.c_##name &= ~flag;                                                                                                                      \
    }
        ADD(iflag, IGNBRK);
        ADD(iflag, BRKINT);
        ADD(iflag, IGNPAR);
        ADD(iflag, PARMRK);
        ADD(iflag, INPCK);
        ADD(iflag, ISTRIP);
        ADD(iflag, INLCR);
        ADD(iflag, IGNCR);
        ADD(iflag, ICRNL);
        ADD(iflag, IUCLC);
        ADD(iflag, IXON);
        ADD(iflag, IXANY);
        ADD(iflag, IXOFF);
        ADD(iflag, IMAXBEL);
        ADD(iflag, IUTF8);

        if (!c_iflag.empty()) {
            c_iflag.pop_back();
        }

        if (term.c_iflag != 0) {
            c_iflag += fmt::format("0x{:x}", term.c_iflag);
        }

        ADD(oflag, OPOST);
        ADD(oflag, OLCUC);
        ADD(oflag, ONLCR);
        ADD(oflag, OCRNL);
        ADD(oflag, ONOCR);
        ADD(oflag, ONLRET);
        ADD(oflag, OFILL);
        ADD(oflag, OFDEL);

        if (!c_oflag.empty()) {
            c_oflag.pop_back();
        }

        if (term.c_oflag != 0) {
            c_oflag += fmt::format("0x{:x}|", term.c_oflag);
        }

        ADD(cflag, CSIZE);
        ADD(cflag, CSTOPB);
        ADD(cflag, CREAD);
        ADD(cflag, PARENB);
        ADD(cflag, PARODD);
        ADD(cflag, HUPCL);
        ADD(cflag, CLOCAL);

        if (!c_cflag.empty()) {
            c_cflag.pop_back();
        }

        if (term.c_cflag != 0) {
            c_cflag += fmt::format("0x{:x}|", term.c_cflag);
        }

        ADD(lflag, ISIG);
        ADD(lflag, ICANON);
        ADD(lflag, ECHO);
        ADD(lflag, ECHOE);
        ADD(lflag, ECHOK);
        ADD(lflag, ECHONL);
        ADD(lflag, NOFLSH);
        ADD(lflag, TOSTOP);

        if (!c_lflag.empty()) {
            c_lflag.pop_back();
        }

        if (term.c_lflag != 0) {
            c_lflag += fmt::format("0x{:x}", term.c_lflag);
        }
#undef ADD

#define CHECK_NAME(id)                                                                                                                               \
    if (rsi == id)                                                                                                                                   \
        name = #id;
        CHECK_NAME(TCGETS);
        CHECK_NAME(TCSETS);
        CHECK_NAME(TCSETSW);
#undef CHECK_NAME

        STRACE("ioctl(%d, %s, {c_iflag=%s, c_oflag=%s, c_cflag=%s, c_lflag=%s}) = %d", rdi, name.c_str(), c_iflag.c_str(), c_oflag.c_str(),
               c_cflag.c_str(), c_lflag.c_str(), (int)result);
        return true;
    }
    }

    return false;
}

bool is_proc_self_exe(u64 val) {
    const char* path = (const char*)val;
    std::string spath = path;
    std::string pidpath = "/proc/" + std::to_string(getpid()) + "/exe";
    if (spath == "/proc/self/exe" || spath == "/proc/thread-self/exe" || spath == pidpath) {
        return true;
    }
    return false;
}

bool is_proc_self_exe(const std::string& path) {
    return is_proc_self_exe((u64)path.c_str());
}

void felix86_syscall(ThreadState* state) {
    u64 syscall_number = state->GetGpr(X86_REF_RAX);
    u64 rdi = state->GetGpr(X86_REF_RDI);
    u64 rsi = state->GetGpr(X86_REF_RSI);
    u64 rdx = state->GetGpr(X86_REF_RDX);
    u64 r10 = state->GetGpr(X86_REF_R10);
    u64 r8 = state->GetGpr(X86_REF_R8);
    u64 r9 = state->GetGpr(X86_REF_R9);

    struct Result {
        Result& operator=(ssize_t inner) {
            if (inner == -1) {
                this->inner = -errno;
            } else {
                this->inner = inner;
            }
            return *this;
        }

        operator ssize_t() const {
            return inner;
        }

        operator void*() const {
            return (void*)inner;
        }

    private:
        ssize_t inner = -1;
    };

    Result result;

    Filesystem& fs = *g_fs;

    switch (syscall_number) {
    case felix86_x86_64_brk: {
        if (rdi == 0) {
            result = g_current_brk - g_address_space_base;
        } else {
            g_current_brk = rdi + g_address_space_base;
            result = rdi;
        }

        if (g_current_brk > g_initial_brk + g_current_brk_size) {
            u64 new_size = (g_current_brk - g_initial_brk) * 2;
            void* new_map = mremap((void*)g_initial_brk, brk_size, new_size, 0);
            if ((u64)new_map != g_initial_brk) {
                ERROR("Failed to remap brk with new size: %lx", new_size);
            }
            WARN("Resized BRK to %lx", new_size);
            g_current_brk_size = new_size;
        }

        if (g_address_space_base) {
            ASSERT(g_current_brk >= g_address_space_base && g_current_brk <= g_address_space_base + UINT32_MAX);
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
            result = -EINVAL;
            break;
        }
        }
        STRACE("arch_prctl(%016lx, %016lx) = %016lx", rdi, rsi, (u64)result);
        break;
    }
    case felix86_x86_64_set_tid_address: {
        state->clear_tid_address = (pid_t*)rdi;
        result = gettid();
        STRACE("set_tid_address(%016lx) = %016lx", rdi, (u64)result);
        break;
    }
    case felix86_x86_64_set_robust_list: {
        result = -ENOSYS;
        break;
    }
    case felix86_x86_64_rseq: {
        // Couldn't find any solid documentation and FEX doesn't support it either
        result = -ENOSYS;
        STRACE("rseq(...) = %016lx", (u64)result);
        break;
    }
    case felix86_x86_64_time: {
        result = ::time((time_t*)rdi);
        STRACE("time(%p) = %016lx", (void*)rdi, (u64)result);
        break;
    }
    case felix86_x86_64_prlimit64: {
        result = HOST_SYSCALL(prlimit64, rdi, rsi, rdx, r10);
        STRACE("prlimit64(%016lx, %016lx, %016lx, %016lx) = %016lx", rdi, rsi, rdx, r10, (u64)result);
        break;
    }
    case felix86_x86_64_readlink: {
        if (is_proc_self_exe(rdi)) {
            std::string path = fs.GetExecutablePath().string();
            size_t size = std::min(path.size(), (size_t)rdx);
            memcpy((void*)rsi, path.c_str(), size);
            result = size;
        } else {
            result = HOST_SYSCALL(readlinkat, AT_FDCWD, rdi, rsi, rdx);
        }
        STRACE("readlink(%s, %s, %d) = %d", (const char*)rdi, (char*)rsi, (int)rdx, (int)result);
        break;
    }
    case felix86_x86_64_readlinkat: {
        if (is_proc_self_exe(rsi)) {
            std::string path = fs.GetExecutablePath().string();
            size_t size = std::min(path.size(), (size_t)r10);
            memcpy((void*)rdx, path.c_str(), size);
            result = size;
        } else {
            result = HOST_SYSCALL(readlinkat, rdi, rsi, rdx, r10);
        }
        STRACE("readlinkat(%d, %s, %s, %d) = %d", (int)rdi, (const char*)rsi, (char*)rdx, (int)r10, (int)result);
        break;
    }
    case felix86_x86_64_getrandom: {
        result = HOST_SYSCALL(getrandom, rdi, rsi, rdx);
        STRACE("getrandom(%p, %016lx, %d) = %016lx", (void*)rdi, rsi, (int)rdx, (u64)result);
        break;
    }
    case felix86_x86_64_mprotect: {
        result = HOST_SYSCALL(mprotect, rdi, rsi, rdx);
        STRACE("mprotect(%p, %016lx, %d) = %016lx", (void*)rdi, rsi, (int)rdx, (u64)result);
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
        // if (added_region && !(path_copy.empty() || name_copy.empty())) {
        //     Elf::LoadSymbols(name_copy, path_copy, (void*)min_address_copy);
        // }
        break;
    }
    case felix86_x86_64_shutdown: {
        result = HOST_SYSCALL(shutdown, rdi, rsi);
        STRACE("shutdown(%d, %d) = %d", (int)rdi, (int)rsi, (int)result);
        break;
    }
    case felix86_x86_64_shmget: {
        result = HOST_SYSCALL(shmget, rdi, rsi, rdx);
        STRACE("shmget(%d, %d, %d) = %d", (int)rdi, (int)rsi, (int)rdx, (int)result);
        break;
    }
    case felix86_x86_64_shmat: {
        result = HOST_SYSCALL(shmat, rdi, (void*)rsi, rdx);
        STRACE("shmat(%d, %p, %d) = %d", (int)rdi, (void*)rsi, (int)rdx, (int)result);
        break;
    }
    case felix86_x86_64_shmctl: {
        result = HOST_SYSCALL(shmctl, rdi, rsi, rdx);
        STRACE("shmctl(%d, %d, %p) = %d", (int)rdi, (int)rsi, (void*)rdx, (int)result);
        break;
    }
    case felix86_x86_64_shmdt: {
        result = HOST_SYSCALL(shmdt, (void*)rdi);
        STRACE("shmdt(%p) = %d", (void*)rdi, (int)result);
        break;
    }
    case felix86_x86_64_bind: {
        result = HOST_SYSCALL(bind, rdi, (struct sockaddr*)rsi, rdx);
        STRACE("bind(%d, %p, %d) = %d", (int)rdi, (void*)rsi, (int)rdx, (int)result);
        break;
    }
    case felix86_x86_64_setpgid: {
        result = HOST_SYSCALL(setpgid, rdi, rsi);
        STRACE("setpgid(%d, %d) = %d", (int)rdi, (int)rsi, (int)result);
        break;
    }
    case felix86_x86_64_setpriority: {
        result = HOST_SYSCALL(setpriority, rdi, rsi, rdx);
        STRACE("setpriority(%d, %d, %d) = %d", (int)rdi, (int)rsi, (int)rdx, (int)result);
        break;
    }
    case felix86_x86_64_getpriority: {
        result = HOST_SYSCALL(getpriority, rdi, rsi);
        STRACE("getpriority(%d, %d) = %d", (int)rdi, (int)rsi, (int)result);
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
        result = HOST_SYSCALL(getcwd, rdi, rsi);
        STRACE("getcwd(%p, %d) = %d", (void*)rdi, (int)rsi, (int)result);
        break;
    }
    case felix86_x86_64_rename: {
        std::string oldpath = (char*)rdi;
        if (is_proc_self_exe(oldpath)) {
            oldpath = fs.GetExecutablePath();
        }

        std::string newpath = (char*)rsi;
        if (is_proc_self_exe(newpath)) {
            newpath = fs.GetExecutablePath();
        }

        result = rename(oldpath.c_str(), newpath.c_str());
        STRACE("rename(%s, %s) = %d", oldpath.c_str(), (char*)rsi, (int)result);
        break;
    }
    case felix86_x86_64_epoll_create1: {
        result = HOST_SYSCALL(epoll_create1, rdi);
        STRACE("epoll_create1(%d) = %d", (int)rdi, (int)result);
        break;
    }
    case felix86_x86_64_epoll_ctl: {
        result = HOST_SYSCALL(epoll_ctl, rdi, rsi, rdx, r10);
        STRACE("epoll_ctl(%d, %d, %d, %p) = %d", (int)rdi, (int)rsi, (int)rdx, (void*)r10, (int)result);
        break;
    }
    case felix86_x86_64_epoll_pwait: {
        result = HOST_SYSCALL(epoll_pwait, rdi, rsi, rdx, r10, r8, r9);
        STRACE("epoll_pwait(%d, %p, %d, %d, %p, %d) = %d", (int)rdi, (void*)rsi, (int)rdx, (int)r10, (void*)r8, (int)r9, (int)result);
        break;
    }
    case felix86_x86_64_epoll_pwait2: {
        result = HOST_SYSCALL(epoll_pwait2, rdi, rsi, rdx, r10, r8, r9);
        STRACE("epoll_pwait2(%d, %p, %d, %d, %p, %d) = %d", (int)rdi, (void*)rsi, (int)rdx, (int)r10, (void*)r8, (int)r9, (int)result);
        break;
    }
    case felix86_x86_64_chmod: {
        std::string path = (char*)rdi;
        if (is_proc_self_exe(path)) {
            path = fs.GetExecutablePath();
        }

        result = chmod(path.c_str(), rsi);
        STRACE("chmod(%s, %d) = %d", path.c_str(), (int)rsi, (int)result);
        break;
    }
    case felix86_x86_64_mount: {
        result = HOST_SYSCALL(mount, rdi, rsi, rdx, r10, r8);
        STRACE("mount(%p, %p, %p, %lx, %p) = %d", (void*)rdi, (void*)rsi, (void*)rdx, r10, (void*)r8, (int)result);
        break;
    }
    case felix86_x86_64_socketpair: {
        result = HOST_SYSCALL(socketpair, rdi, rsi, rdx, r10);
        STRACE("socketpair(%d, %d, %d, %p) = %d", (int)rdi, (int)rsi, (int)rdx, (void*)r10, (int)result);
        break;
    }
    case felix86_x86_64_setgid: {
        result = HOST_SYSCALL(setgid, rdi);
        STRACE("setgid(%d) = %d", (int)rdi, (int)result);
        break;
    }
    case felix86_x86_64_setsid: {
        result = HOST_SYSCALL(setsid, rdi);
        STRACE("setsid(%d) = %d", (int)rdi, (int)result);
        break;
    }
    case felix86_x86_64_setreuid: {
        result = HOST_SYSCALL(setreuid, rdi, rsi);
        STRACE("setreuid(%d, %d) = %d", (int)rdi, (int)rsi, (int)result);
        break;
    }
    case felix86_x86_64_setregid: {
        result = HOST_SYSCALL(setregid, rdi, rsi);
        STRACE("setregid(%d, %d) = %d", (int)rdi, (int)rsi, (int)result);
        break;
    }
    case felix86_x86_64_setgroups: {
        result = HOST_SYSCALL(setgroups, rdi, rsi);
        STRACE("setgroups(%d, %p) = %d", (int)rdi, (void*)rsi, (int)result);
        break;
    }
    case felix86_x86_64_getgroups: {
        result = HOST_SYSCALL(getgroups, rdi, rsi);
        STRACE("getgroups(%d, %p) = %d", (int)rdi, (void*)rsi, (int)result);
        break;
    }
    case felix86_x86_64_setuid: {
        result = HOST_SYSCALL(setuid, rdi);
        STRACE("setuid(%d) = %d", (int)rdi, (int)result);
        break;
    }
    case felix86_x86_64_umount2: {
        result = HOST_SYSCALL(umount2, rdi, rsi);
        STRACE("umount2(%s, %lx) = %d", (const char*)rdi, rsi, (int)result);
        break;
    }
    case felix86_x86_64_symlink: {
        std::string oldpath = (char*)rdi;
        if (is_proc_self_exe(oldpath)) {
            oldpath = fs.GetExecutablePath();
        }

        std::string newpath = (char*)rsi;
        if (is_proc_self_exe(newpath)) {
            newpath = fs.GetExecutablePath();
        }

        result = symlink(oldpath.c_str(), newpath.c_str());
        STRACE("symlink(%s, %s) = %d", oldpath.c_str(), (char*)rsi, (int)result);
        break;
    }
    case felix86_x86_64_poll: {
        result = poll((struct pollfd*)rdi, rsi, rdx);
        STRACE("poll(%p, %d, %d) = %d", (void*)rdi, (int)rsi, (int)rdx, (int)result);
        break;
    }
    case felix86_x86_64_ppoll: {
        result = HOST_SYSCALL(ppoll, rdi, rsi, rdx, r10);
        STRACE("ppoll(%p, %d, %p, %p) = %d", (void*)rdi, (int)rsi, (void*)rdx, (void*)r10, (int)result);
        break;
    }
    case felix86_x86_64_sched_getscheduler: {
        result = HOST_SYSCALL(sched_getscheduler, rdi);
        STRACE("sched_getscheduler(%d) = %d", (int)rdi, (int)result);
        break;
    }
    case felix86_x86_64_sched_getparam: {
        result = HOST_SYSCALL(sched_getparam, rdi, (struct sched_param*)rsi);
        STRACE("sched_getparam(%d, %p) = %d", (int)rdi, (void*)rsi, (int)result);
        break;
    }
    case felix86_x86_64_sched_setparam: {
        result = HOST_SYSCALL(sched_setparam, rdi, rsi);
        STRACE("sched_setparam(%d, %p) = %d", (int)rdi, (void*)rsi, (int)result);
        break;
    }
    case felix86_x86_64_clock_gettime: {
        result = HOST_SYSCALL(clock_gettime, rdi, (struct timespec*)rsi);
        STRACE("clock_gettime(%d, %p) = %d", (int)rdi, (void*)rsi, (int)result);
        break;
    }
    case felix86_x86_64_clock_getres: {
        result = HOST_SYSCALL(clock_getres, rdi, (struct timespec*)rsi);
        STRACE("clock_getres(%d, %p) = %d", (int)rdi, (void*)rsi, (int)result);
        break;
    }
    case felix86_x86_64_getresuid: {
        result = HOST_SYSCALL(getresuid, (uid_t*)rdi, (uid_t*)rsi, (uid_t*)rdx);
        STRACE("getresuid(%p, %p, %p) = %d", (void*)rdi, (void*)rsi, (void*)rdx, (int)result);
        break;
    }
    case felix86_x86_64_getresgid: {
        result = HOST_SYSCALL(getresgid, (gid_t*)rdi, (gid_t*)rsi, (gid_t*)rdx);
        STRACE("getresgid(%p, %p, %p) = %d", (void*)rdi, (void*)rsi, (void*)rdx, (int)result);
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
        if (result >= 0) {
            *guest_stat = host_stat;
        }
        break;
    }
    case felix86_x86_64_lstat: {
        std::string path = (char*)rdi;
        if (is_proc_self_exe(path)) {
            path = fs.GetExecutablePath();
        }

        x64Stat* guest_stat = (x64Stat*)rsi;
        struct stat host_stat;
        result = lstat(path.c_str(), &host_stat);
        STRACE("lstat(%s, %p) = %d", path.c_str(), (void*)rsi, (int)result);
        if (result >= 0) {
            *guest_stat = host_stat;
        }
        break;
    }
    case felix86_x86_64_fsync: {
        result = HOST_SYSCALL(fsync, rdi);
        STRACE("fsync(%d) = %d", (int)rdi, (int)result);
        break;
    }
    case felix86_x86_64_sync: {
        result = HOST_SYSCALL(sync);
        STRACE("sync() = %d", (int)result);
        break;
    }
    case felix86_x86_64_syncfs: {
        result = HOST_SYSCALL(syncfs, rdi);
        STRACE("syncfs(%d) = %d", (int)rdi, (int)result);
        break;
    }
    case felix86_x86_64_sendmmsg: {
        result = HOST_SYSCALL(sendmmsg, rdi, (struct mmsghdr*)rsi, rdx, r10);
        STRACE("sendmmsg(%d, %p, %d, %d) = %d", (int)rdi, (void*)rsi, (int)rdx, (int)r10, (int)result);
        break;
    }
    case felix86_x86_64_recvmmsg: {
        result = HOST_SYSCALL(recvmmsg, rdi, (struct mmsghdr*)rsi, rdx, r10, r8);
        STRACE("recvmmsg(%d, %p, %d, %d, %d) = %d", (int)rdi, (void*)rsi, (int)rdx, (int)r10, (int)r8, (int)result);
        break;
    }
    case felix86_x86_64_setsockopt: {
        result = HOST_SYSCALL(setsockopt, rdi, rsi, rdx, r10, r8);
        STRACE("setsockopt(%d, %d, %d, %d, %d) = %d", (int)rdi, (int)rsi, (int)rdx, (int)r10, (int)r8, (int)result);
        break;
    }
    case felix86_x86_64_getsockopt: {
        result = HOST_SYSCALL(getsockopt, rdi, rsi, rdx, r10, r8);
        STRACE("getsockopt(%d, %d, %d, %d, %d) = %d", (int)rdi, (int)rsi, (int)rdx, (int)r10, (int)r8, (int)result);
        break;
    }
    case felix86_x86_64_statx: {
        std::string path = (char*)rsi;
        if (is_proc_self_exe(path)) {
            path = fs.GetExecutablePath();
        }

        result = HOST_SYSCALL(statx, rdi, path.c_str(), rdx, r10, r8);
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
        result = HOST_SYSCALL(chdir, rdi);
        STRACE("chdir(%s) = %d", (const char*)rdi, (int)result);
        break;
    }
    case felix86_x86_64_fchown: {
        result = HOST_SYSCALL(fchown, rdi, rsi, rdx);
        STRACE("fchown(%d, %d, %d) = %d", (int)rdi, (int)rsi, (int)rdx, (int)result);
        break;
    }
    case felix86_x86_64_chown: {
        std::string path = (char*)rdi;
        if (is_proc_self_exe(path)) {
            path = fs.GetExecutablePath();
        }

        result = chown(path.c_str(), rsi, rdx);
        STRACE("chown(%s, %d, %d) = %d", path.c_str(), (int)rsi, (int)rdx, (int)result);
        break;
    }
    case felix86_x86_64_unlinkat: {
        std::string path = (char*)rsi;
        if (is_proc_self_exe(path)) {
            WARN("unlinkat called on /proc/self/exe");
            path = fs.GetExecutablePath();
        }

        result = HOST_SYSCALL(unlinkat, rdi, path.c_str(), rdx);
        STRACE("unlinkat(%d, %s, %d) = %d", (int)rdi, (const char*)rsi, (int)rdx, (int)result);
        break;
    }
    case felix86_x86_64_fchdir: {
        result = HOST_SYSCALL(fchdir, rdi);
        STRACE("fchdir(%d) = %d", (int)rdi, (int)result);
        break;
    }
    case felix86_x86_64_newfstatat: {
        std::string path = (char*)rsi;
        if (is_proc_self_exe(path)) {
            path = fs.GetExecutablePath();
        }

        x64Stat* guest_stat = (x64Stat*)rdx;
        struct stat host_stat;
        result = HOST_SYSCALL(newfstatat, rdi, path.c_str(), &host_stat, r10);
        STRACE("newfstatat(%d, %s, %p, %d) = %d", (int)rdi, path.c_str(), (void*)rdx, (int)r10, (int)result);
        if (result >= 0) {
            *guest_stat = host_stat;
        }
        break;
    }
    case felix86_x86_64_sysinfo: {
        result = HOST_SYSCALL(sysinfo, rdi);
        STRACE("sysinfo(%p) = %d", (void*)rdi, (int)result);
        break;
    }
    case felix86_x86_64_ioctl: {
        result = HOST_SYSCALL(ioctl, rdi, rsi, rdx);

        if (!try_strace_ioctl(rdi, rsi, rdx, result)) {
            STRACE("ioctl(%d, %016lx, %016lx) = %016lx", (int)rdi, rsi, rdx, (u64)result);
        }
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
        STRACE("exit_group(%d)", (int)rdi);
        state->exit_reason = EXIT_REASON_EXIT_GROUP_SYSCALL;
        state->exit_code = rdi;
        Emulator::ExitDispatcher(state);
        UNREACHABLE();
        break;
    }
    case felix86_x86_64_access: {
        if (is_proc_self_exe(rdi)) {
            std::filesystem::path path = fs.GetExecutablePath();
            result = HOST_SYSCALL(faccessat, AT_FDCWD, path.c_str(), rsi, 0);
        } else {
            result = HOST_SYSCALL(faccessat, AT_FDCWD, rdi, rsi, 0);
        }
        STRACE("access(%s, %d) = %d", (const char*)rdi, (int)rsi, (int)result);
        break;
    }
    case felix86_x86_64_faccessat:
    case felix86_x86_64_faccessat2: {
        if (is_proc_self_exe(rsi)) {
            std::filesystem::path path = fs.GetExecutablePath();
            result = HOST_SYSCALL(faccessat, rdi, path.c_str(), rdx, r10);
        } else {
            result = HOST_SYSCALL(faccessat, rdi, rsi, rdx, r10);
        }
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
        result = mkdir((char*)rdi, rsi);
        STRACE("mkdir(%s, %d) = %d", (char*)rdi, (int)rsi, (int)result);
        break;
    }
    case felix86_x86_64_lgetxattr: {
        result = HOST_SYSCALL(lgetxattr, rdi, rsi, rdx, r10);
        STRACE("lgetxattr(%s, %s, %p, %d) = %d", (char*)rdi, (char*)rsi, (void*)rdx, (int)r10, (int)result);
        break;
    }
    case felix86_x86_64_pwrite64: {
        result = HOST_SYSCALL(pwrite64, rdi, rsi, rdx, r10);
        STRACE("pwrite64(%d, %p, %d, %d) = %d", (int)rdi, (void*)rsi, (int)rdx, (int)r10, (int)result);
        break;
    }
    case felix86_x86_64_pread64: {
        result = HOST_SYSCALL(pread64, rdi, rsi, rdx, r10);
        STRACE("pread64(%d, %p, %d, %d) = %d", (int)rdi, (void*)rsi, (int)rdx, (int)r10, (int)result);
        break;
    }
    case felix86_x86_64_open: {
        u64 rdi_old = rdi;
        u64 rsi_old = rsi;
        u64 rdx_old = rdx;
        rdi = AT_FDCWD;
        rsi = rdi_old;
        rdx = rsi_old;
        r10 = rdx_old;
        [[fallthrough]]; // openat MUST be right after
    }
    case felix86_x86_64_openat: {
        std::string path = (char*)rsi;
        if (path == "/run/systemd/userdb/") { // TODO: There's some bug in Qt apps with this path
            result = -ENOENT;
            break;
        }

        if (is_proc_self_exe(rsi)) {
            std::filesystem::path path = fs.GetExecutablePath();
            result = HOST_SYSCALL(openat, rdi, path.c_str(), rdx, r10);
        } else {
            result = HOST_SYSCALL(openat, rdi, rsi, rdx, r10);
            std::filesystem::path path = (char*)rsi;
        }
        STRACE("openat(%d, %s, %d, %d) = %d", (int)rdi, (const char*)rsi, (int)rdx, (int)r10, (int)result);
        break;
    }
    case felix86_x86_64_tgkill: {
        STRACE("tgkill(%d, %d, %d) = %d", (int)rdi, (int)rsi, (int)rdx, (int)result);
        result = HOST_SYSCALL(tgkill, rdi, rsi, rdx);
        break;
    }
    case felix86_x86_64_kill: {
        STRACE("kill(%d, %d) = %d", (int)rdi, (int)rsi, (int)result);
        result = HOST_SYSCALL(kill, rdi, rsi);
        break;
    }
    case felix86_x86_64_mmap: {
        if ((int)r8 != -1) {
            // uses file descriptor, mmaps file to memory, may need to update mappings
            // this can occur when using something like dlopen or when the interpreter initially loads the symbols
            g_symbols_cached = false;
        }

#ifndef MAP_32BIT
#define MAP_32BIT 0x40
#endif
        u64 flags = r10;
        if ((flags & MAP_32BIT) && !(flags & MAP_FIXED)) {
            // This flag is x86 only but we need to emulate it
            // For example, Mono tries to use it to allocate code cache pages near the executable so that it can use
            // +-2GiB jumps. If it doesn't get them near enough it will eventually crash and die.
            if (rdi == 0) {
                // TODO: better less hacky support
                // We only wanna act in the case there's no hint, otherwise we don't care?
                r10 &= ~MAP_32BIT;
                u64 new_flags = r10 | MAP_FIXED_NOREPLACE;
                u64 aligned_size = (rsi + 0x1000) & ~0xFFF;
                // MAP_32BIT allocates in the first 2 GiB of memory
                u64 bottom = 0x4000'0000 - aligned_size;
                int attempts = (0x4000'0000 / aligned_size) - 1;
                bool ok = false;
                while (true) {
                    LOG("Attemping at: %lx with size %lx", bottom, rsi);
                    result = HOST_SYSCALL(mmap, bottom, aligned_size, rdx, new_flags, r8, r9);

                    if ((i64)result > 0) {
                        ok = true;
                        LOG("Returning mapped region with MAP_32BIT: %lx", (u64)result);
                        break;
                    }

                    bottom -= aligned_size;

                    if (attempts-- == 0) {
                        WARN("Ran out of attempts while allocating with MAP32_BIT, we might crash");
                        break;
                    }
                }

                if (ok) {
                    break;
                }
            } else {
                WARN("MAP32_BIT with hint: %lx?", rdi);
                flags |= MAP_FIXED_NOREPLACE; // <= at least fix it so it obeys the hint
            }
        }

        result = HOST_SYSCALL(mmap, rdi, rsi, rdx, flags, r8, r9);
        STRACE("mmap(%p, %016lx, %d, %d, %d, %d) = %016lx", (void*)rdi, rsi, (int)rdx, (int)flags, (int)r8, (int)r9, (u64)result);
        break;
    }
    case felix86_x86_64_munmap: {
        result = HOST_SYSCALL(munmap, rdi, rsi);
        STRACE("munmap(%p, %016lx) = %016lx", (void*)rdi, rsi, (u64)result);
        break;
    }
    case felix86_x86_64_setitimer: {
        result = HOST_SYSCALL(setitimer, rdi, rsi, rdx);
        STRACE("setitimer(%d, %p, %p) = %d", (int)rdi, (void*)rdi, (void*)rsi, (int)result);
        break;
    }
    case felix86_x86_64_getuid: {
        result = HOST_SYSCALL(getuid);
        STRACE("getuid() = %d", (int)result);
        break;
    }
    case felix86_x86_64_fdatasync: {
        result = HOST_SYSCALL(fdatasync, rdi);
        STRACE("fdatasync(%d) = %d", (int)rdi, (int)result);
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
    case felix86_x86_64_utimensat: {
        result = HOST_SYSCALL(utimensat, rdi, (const char*)rsi, (struct timespec*)rdx, r10);
        STRACE("utimensat(%d, %s, %p, %d) = %d", (int)rdi, (const char*)rsi, (void*)rdx, (int)r10, (int)result);
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
    case felix86_x86_64_mremap: {
        result = HOST_SYSCALL(mremap, rdi, rsi, rdx, r10, r8);
        STRACE("mremap(%p, %016lx, %016lx, %d, %016lx) = %016lx", (void*)rdi, rsi, rdx, (int)r10, r8, (u64)result);
        break;
    }
    case felix86_x86_64_msync: {
        result = HOST_SYSCALL(msync, rdi, rsi, rdx);
        STRACE("msync(%p, %016lx, %d) = %016lx", (void*)rdi, rsi, (int)rdx, (u64)result);
        break;
    }
    case felix86_x86_64_sendto: {
        result = HOST_SYSCALL(sendto, rdi, rsi, rdx, r10, r8, r9);
        STRACE("sendto(%d, %p, %d, %d, %p, %d) = %d", (int)rdi, (void*)rsi, (int)rdx, (int)r10, (void*)r8, (int)r9, (int)result);
        break;
    }
    case felix86_x86_64_alarm: {
        result = alarm(rdi);
        STRACE("alarm(%d) = %d", (int)rdi, (int)result);
        break;
    }
    case felix86_x86_64_times: {
        result = HOST_SYSCALL(times, (struct tms*)rdi);
        STRACE("times(%p) = %d", (void*)rdi, (int)result);
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
    case felix86_x86_64_timerfd_create: {
        result = HOST_SYSCALL(timerfd_create, rdi, rsi);
        STRACE("timerfd_create(%d, %d) = %d", (int)rdi, (int)rsi, (int)result);
        break;
    }
    case felix86_x86_64_timerfd_settime: {
        result = HOST_SYSCALL(timerfd_settime, rdi, rsi, rdx, r10);
        STRACE("timerfd_settime(%d, %d, %p, %p) = %d", (int)rdi, (int)rsi, (void*)rdx, (void*)r10, (int)result);
        break;
    }
    case felix86_x86_64_timerfd_gettime: {
        result = HOST_SYSCALL(timerfd_gettime, rdi, (struct itimerspec*)rsi);
        STRACE("timerfd_gettime(%d, %p) = %d", (int)rdi, (void*)rsi, (int)result);
        break;
    }
    case felix86_x86_64_statfs: {
        std::string path = (char*)rdi;
        if (is_proc_self_exe(path)) {
            path = fs.GetExecutablePath();
        }

        result = HOST_SYSCALL(statfs, path.c_str(), (struct statfs*)rsi);
        STRACE("statfs(%s, %p) = %d", path.c_str(), (void*)rsi, (int)result);
        break;
    }
    case felix86_x86_64_stat: {
        std::string path = (char*)rdi;
        if (is_proc_self_exe(path)) {
            path = fs.GetExecutablePath();
        }

        x64Stat* guest_stat = (x64Stat*)rsi;
        struct stat host_stat;
        result = stat(path.c_str(), &host_stat);
        STRACE("stat(%s, %p) = %d", path.c_str(), (void*)rsi, (int)result);
        if (result >= 0) {
            *guest_stat = host_stat;
        }
        break;
    }
    case felix86_x86_64_fstatfs: {
        result = HOST_SYSCALL(fstatfs, rdi, (struct statfs*)rsi);
        STRACE("fstatfs(%d, %p) = %d", (int)rdi, (void*)rsi, (int)result);
        break;
    }
    case felix86_x86_64_getsockname: {
        result = HOST_SYSCALL(getsockname, rdi, (struct sockaddr*)rsi, (socklen_t*)rdx);
        STRACE("getsockname(%d, %p, %p) = %d", (int)rdi, (void*)rsi, (void*)rdx, (int)result);
        break;
    }
    case felix86_x86_64_madvise: {
        result = HOST_SYSCALL(madvise, rdi, rsi, rdx);
        STRACE("madvise(%p, %016lx, %d) = %d", (void*)rdi, rsi, (int)rdx, (int)result);
        break;
    }
    case felix86_x86_64_exit: {
        STRACE("exit(%d)", (int)rdi);
        state->exit_reason = ExitReason::EXIT_REASON_EXIT_SYSCALL;
        state->exit_code = rdi;
        Emulator::ExitDispatcher(state);
        UNREACHABLE();
        break;
    }
    case felix86_x86_64_vfork: {
        result = -ENOSYS; // make it use clone instead
        STRACE("vfork() = %d", (int)result);
        break;
    }
    case felix86_x86_64_eventfd2: {
        result = HOST_SYSCALL(eventfd2, rdi, rsi);
        STRACE("eventfd2(%d, %d) = %d", (int)rdi, (int)rsi, (int)result);
        break;
    }
    case felix86_x86_64_fchmod: {
        result = HOST_SYSCALL(fchmod, rdi, rsi);
        STRACE("fchmod(%d, %d) = %d", (int)rdi, (int)rsi, (int)result);
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
    case felix86_x86_64_clock_nanosleep: {
        result = HOST_SYSCALL(clock_nanosleep, rdi, rsi, rdx, r10);
        STRACE("clock_nanosleep(%d, %d, %p, %p) = %d", (int)rdi, (int)rsi, (void*)rdx, (void*)r10, (int)result);
        break;
    }
    case felix86_x86_64_rt_sigaction: {
        struct x86_sigaction* act = (struct x86_sigaction*)rsi;
        if (act) {
            auto handler = act->handler;
            Signals::registerSignalHandler(state, rdi, GuestAddress{(u64)handler}, act->sa_mask, act->sa_flags);
            if (g_verbose) {
                printf("Installed signal handler %s at:\n", strsignal(rdi));
                print_address((u64)handler);
                printf("Flags: %lx\n", act->sa_flags);
            }
        }

        struct sigaction* old_act = (struct sigaction*)rdx;
        if (old_act) {
            RegisteredSignal old = Signals::getSignalHandler(state, rdi);
            bool was_sigaction = old.flags & SA_SIGINFO;
            if (was_sigaction) {
                old_act->sa_sigaction = (decltype(old_act->sa_sigaction))old.func.raw();
            } else {
                old_act->sa_handler = (decltype(old_act->sa_handler))old.func.raw();
            }
            old_act->sa_flags = old.flags;
            old_act->sa_mask = old.mask;
        }

        result = 0;
        STRACE("rt_sigaction(%d, %p, %p) = %d", (int)rdi, (void*)rsi, (void*)r10, (int)result);
        break;
    }
    case felix86_x86_64_rt_sigtimedwait: {
        result = HOST_SYSCALL(rt_sigtimedwait, rdi, rsi, rdx, r10);
        STRACE("rt_sigtimedwait(%p, %p, %p, %d, %d) = %d", (void*)rdi, (void*)rsi, (void*)rdx, (int)r10, (int)r8, (int)result);
        WARN_ONCE("This program uses rt_sigtimedwait");
        break;
    }
    case felix86_x86_64_sched_yield: {
        result = HOST_SYSCALL(sched_yield);
        STRACE("sched_yield() = %d", (int)result);
        break;
    }
    case felix86_x86_64_sigaltstack: {
        VERBOSE("----- sigaltstack was called -----");
        stack_t host_stack; // save old stack here while we check if guest stack is valid
        stack_t* guest_stack = (stack_t*)rdi;
        stack_t guest_stack_copy = *guest_stack;

        // Let the kernel decide if the guest_stack is valid
        int result_temp = sigaltstack(&guest_stack_copy, &host_stack);

        // Restore old stack
        int result_must = sigaltstack(&host_stack, nullptr);
        ASSERT(result_must == 0);

        if (result_temp != 0) {
            WARN("Failed to set sigaltstack");
            result = result_temp;
            break;
        }

        stack_t* new_ss = (stack_t*)rdi;
        stack_t* old_ss = (stack_t*)rsi;

        if (old_ss) {
            old_ss->ss_sp = state->alt_stack.ss_sp;
            old_ss->ss_flags = state->alt_stack.ss_flags;
            old_ss->ss_size = state->alt_stack.ss_size;
        }

        if (new_ss) {
            state->alt_stack.ss_sp = new_ss->ss_sp;
            state->alt_stack.ss_flags = new_ss->ss_flags;
            state->alt_stack.ss_size = new_ss->ss_size;
        }

        result = 0;
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
                size_t actual_size = std::min(size, g_guest_auxv_size);
                memcpy(addr, (void*)g_guest_auxv.raw(), actual_size);
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
        STRACE("prctl(%d, %016lx, %016lx, %016lx, %016lx) = %016lx", (int)rdi, rsi, rdx, r10, r8, (u64)result);
        break;
    }
    case felix86_x86_64_futex: {
        STRACE("futex(%p, %d, %d, %p, %p, %d) ...", (void*)rdi, (int)rsi, (int)rdx, (void*)r10, (void*)r8, (int)r9);
        result = HOST_SYSCALL(futex, rdi, rsi, rdx, r10, r8, r9);
        break;
    }
    case felix86_x86_64_inotify_init1: {
        result = HOST_SYSCALL(inotify_init1, rdi);
        STRACE("inotify_init1(%d) = %d", (int)rdi, (int)result);
        break;
    }
    case felix86_x86_64_inotify_add_watch: {
        result = HOST_SYSCALL(inotify_add_watch, rdi, (const char*)rsi, rdx);
        STRACE("inotify_add_watch(%d, %s, %d) = %d", (int)rdi, (const char*)rsi, (int)rdx, (int)result);
        break;
    }
    case felix86_x86_64_inotify_rm_watch: {
        result = HOST_SYSCALL(inotify_rm_watch, rdi, rsi);
        STRACE("inotify_rm_watch(%d, %d) = %d", (int)rdi, (int)rsi, (int)result);
        break;
    }
    case felix86_x86_64_fallocate: {
        result = HOST_SYSCALL(fallocate, rdi, rsi, rdx, r10);
        STRACE("fallocate(%d, %d, %d, %d) = %d", (int)rdi, (int)rsi, (int)rdx, (int)r10, (int)result);
        break;
    }
    case felix86_x86_64_sched_getaffinity: {
        result = HOST_SYSCALL(sched_getaffinity, rdi, rsi, rdx);
        STRACE("sched_getaffinity(%d, %d, %p) = %d", (int)rdi, (int)rsi, (void*)rdx, (int)result);
        break;
    }
    case felix86_x86_64_sched_setaffinity: {
        result = HOST_SYSCALL(sched_setaffinity, rdi, rsi, rdx);
        STRACE("sched_setaffinity(%d, %d, %p) = %d", (int)rdi, (int)rsi, (void*)rdx, (int)result);
        break;
    }
    case felix86_x86_64_sched_get_priority_min: {
        result = HOST_SYSCALL(sched_get_priority_min, rdi);
        STRACE("sched_get_priority_min(%d) = %d", (int)rdi, (int)result);
        break;
    }
    case felix86_x86_64_sched_get_priority_max: {
        result = HOST_SYSCALL(sched_get_priority_max, rdi);
        STRACE("sched_get_priority_max(%d) = %d", (int)rdi, (int)result);
        break;
    }
    case felix86_x86_64_sched_setscheduler: {
        result = HOST_SYSCALL(sched_setscheduler, rdi, rsi, rdx);
        STRACE("sched_setscheduler(%d, %d, %p) = %d", (int)rdi, (int)rsi, (void*)rdx, (int)result);
        break;
    }
    case felix86_x86_64_mincore: {
        result = HOST_SYSCALL(mincore, rdi, rsi, rdx);
        STRACE("mincore(%p, %d, %p) = %d", (void*)rdi, (int)rsi, (void*)rdx, (int)result);
        break;
    }
    case felix86_x86_64_clone3: {
        result = -ENOSYS; // don't support these for now
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
        STRACE("wait4(%d, %p, %d, %p)", (int)rdi, (void*)rsi, (int)rdx, (void*)r10);
        break;
    }
    case felix86_x86_64_execve: {
        std::string path = (char*)rdi;
        if (is_proc_self_exe(path)) {
            path = fs.GetExecutablePath();
        }

        if (!std::filesystem::exists(path)) {
            result = -ENOENT;
            break;
        }

        if (!std::filesystem::is_regular_file(path)) {
            result = -ENOENT;
            break;
        }

        std::vector<const char*> argv;
        std::vector<const char*> envp;

        argv.push_back("/proc/self/exe"); // emulator itself
        if (rsi) {
            const char** guest_argv = (const char**)rsi;
            guest_argv++;

            if (path.find('/') == std::string::npos) {
                // If there's no '/' characters, this is probably just a filename by itself
                // That means we need to look for the absolute path in PATH
                bool found = false;
                std::string PATH = getenv("PATH");
                size_t current_start = 0;
                size_t size = PATH.size();
                for (size_t i = 0; i < size; i++) {
                    if (PATH[i] == ':' || i == size - 1) {
                        // Set it to 0 so that the string creation ends there
                        if (PATH[i] == ':')
                            PATH[i] = '\0';
                        std::filesystem::path dir = PATH.data() + current_start;
                        current_start = i + 1;
                        std::filesystem::path executable = dir / path;
                        if (std::filesystem::exists(executable) && std::filesystem::is_regular_file(executable)) {
                            path = executable;
                            found = true;
                            break;
                        }
                    }
                }

                if (!found) {
                    ERROR("Failed to find %s during execve", path.c_str());
                }
            }

            argv.push_back(path.c_str());

            while (*guest_argv) {
                argv.push_back(*guest_argv);
                guest_argv++;
            }
        }
        argv.push_back(nullptr);

        if (rdx) {
            const char** guest_env = (const char**)rdx;
            while (*guest_env) {
                envp.push_back(*guest_env);
                guest_env++;
            }
        }
        envp.push_back("__FELIX86_LAUNCHED=1");
        envp.push_back("__FELIX86_EXECVE=1");
        envp.push_back("LD_LIBRARY_PATH=/felix86/lib");
        char** host_environ = environ;
        while (*host_environ) {
            std::string env = *host_environ;
            if (env.find("FELIX86") != std::string::npos) {
                envp.push_back(*host_environ);
            }
            host_environ++;
        }
        envp.push_back(nullptr);

        std::string args = "";
        for (auto arg : argv) {
            args += " ";
            args += arg ? arg : "";
        }

        LOG("Running execve, wish me luck:%s", args.c_str());

        syscall(SYS_execve, "/proc/self/exe", argv.data(), envp.data());

        UNREACHABLE();
        break;
    }
    case felix86_x86_64_umask: {
        result = HOST_SYSCALL(umask, rdi);
        STRACE("umask(%d) = %d", (int)rdi, (int)result);
        break;
    }
    case felix86_x86_64_linkat: {
        std::string oldpath = (char*)rsi;
        if (is_proc_self_exe(oldpath)) {
            oldpath = fs.GetExecutablePath();
        }

        std::string newpath = (char*)r10;
        if (is_proc_self_exe(newpath)) {
            newpath = fs.GetExecutablePath();
        }

        result = linkat(rdi, oldpath.c_str(), rdx, newpath.c_str(), r8);
        STRACE("linkat(%d, %s, %d, %s, %d) = %d", (int)rdi, oldpath.c_str(), (int)rdx, newpath.c_str(), (int)r8, (int)result);
        break;
    }
    case felix86_x86_64_unlink: {
        std::string path = (char*)rdi;
        if (is_proc_self_exe(path)) {
            path = fs.GetExecutablePath();
        }

        STRACE("unlink(%s)", path.c_str());
        unlink(path.c_str());
        result = 0;
        break;
    }
    case felix86_x86_64_getpeername: {
        result = HOST_SYSCALL(getpeername, rdi, (struct sockaddr*)rsi, (socklen_t*)rdx);
        STRACE("getpeername(%d, %p, %p) = %d", (int)rdi, (void*)rsi, (void*)rdx, (int)result);
        break;
    }
    case felix86_x86_64_rt_sigsuspend: {
        result = Signals::sigsuspend(state, (sigset_t*)rdi);
        STRACE("rt_sigsuspend(%p, %d) = %d", (void*)rdi, (int)rsi, (int)result);
        break;
    }
    case felix86_x86_64_rt_sigprocmask: {
        int how = rdi;
        sigset_t* set = (sigset_t*)rsi;
        sigset_t* oldset = (sigset_t*)rdx;

        sigset_t old_host_set = state->signal_mask;
        result = 0;
        if (set) {
            if (how == SIG_BLOCK) {
                sigorset(&state->signal_mask, &state->signal_mask, set);
            } else if (how == SIG_UNBLOCK) {
                sigset_t not_set;
                sigfillset(&not_set);
                u16 bit_size = sizeof(sigset_t) * 8;
                for (u16 i = 0; i < bit_size; i++) {
                    if (sigismember(set, i)) {
                        sigdelset(&state->signal_mask, i);
                    }
                }
                sigandset(&state->signal_mask, &state->signal_mask, &not_set);
            } else if (how == SIG_SETMASK) {
                memcpy(&state->signal_mask, set, sizeof(u64)); // copying the entire struct segfaults sometimes
            } else {
                result = -EINVAL;
                break;
            }

            sigset_t host_mask;
            sigandset(&host_mask, &state->signal_mask, Signals::hostSignalMask());
            pthread_sigmask(SIG_SETMASK, &host_mask, nullptr);
        }

        if (oldset) {
            memcpy(oldset, &old_host_set, sizeof(u64));
        }
        break;
    }
    default: {
        result = -ENOSYS;
        ERROR("Unimplemented syscall %s (%016lx)", print_syscall_name(syscall_number), syscall_number);
        break;
    }
    }

    state->SetGpr(X86_REF_RAX, result);
}