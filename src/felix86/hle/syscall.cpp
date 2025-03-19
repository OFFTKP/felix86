#include <csignal>
#include <errno.h>
#include <fcntl.h>
#include <fmt/format.h>
#include <linux/futex.h>
#include <poll.h>
#include <sched.h>
#include <sys/epoll.h>
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
#include "felix86/common/symlink.hpp"
#include "felix86/emulator.hpp"
#include "felix86/hle/brk.hpp"
#include "felix86/hle/filesystem.hpp"
#include "felix86/hle/stat.hpp"
#include "felix86/hle/syscall.hpp"
#include "felix86/hle/thread.hpp"

// Annoyingly, the ::syscall function returns -1 instead of the actual error number.
// But we also don't wanna check at the end because the STRACE printf and such might have modified errno
// We need to check the moment result gets set
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

#define SYSCALL32(name, ...) (syscall(x86_to_riscv(felix86_x86_32_##name), ##__VA_ARGS__))
#define SYSCALL64(name, ...) (syscall(x64_to_riscv(felix86_x86_64_##name), ##__VA_ARGS__))

// TODO: move me elsewhere
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

Result felix86_syscall_common(ThreadState* state, int rv_syscall, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5, u64 arg6) {
    Result result;
    switch (rv_syscall) {
    case felix86_riscv64_brk: {
        result = BRK::set(arg1);
        STRACE("brk(%p) = %p", (void*)arg1, (void*)result);
        break;
    }
    case felix86_riscv64_set_tid_address: {
        state->clear_tid_address = (pid_t*)arg1;
        result = gettid();
        STRACE("set_tid_address(%lx) = %lx", arg1, (u64)result);
        break;
    }
    case felix86_riscv64_set_robust_list: {
        result = -ENOSYS;
        break;
    }
    case felix86_riscv64_rseq: {
        // Couldn't find any solid documentation and FEX doesn't support it either
        result = -ENOSYS;
        STRACE("rseq(...) = %lx", (u64)result);
        break;
    }
    case felix86_riscv64_prlimit64: {
        result = SYSCALL64(prlimit64, arg1, arg2, arg3, arg4);
        STRACE("prlimit64(%lx, %lx, %lx, %lx) = %lx", arg1, arg2, arg3, arg4, (u64)result);
        break;
    }
    case felix86_riscv64_getrandom: {
        result = SYSCALL64(getrandom, arg1, arg2, arg3);
        STRACE("getrandom(%p, %lx, %d) = %lx", (void*)arg1, arg2, (int)arg3, (u64)result);
        break;
    }
    case felix86_riscv64_mprotect: {
        result = SYSCALL64(mprotect, arg1, arg2, arg3);
        STRACE("mprotect(%p, %lx, %d) = %lx", (void*)arg1, arg2, (int)arg3, (u64)result);
        break;
    }
    case felix86_riscv64_close: {
        // Don't close our stdout
        // TODO: better implementation where it closes an emulated stdout instead
        if (arg1 != 1 && arg1 != 2) {
            result = SYSCALL64(close, arg1);
        } else {
            result = 0;
        }
        STRACE("close(%d) = %d", (int)arg1, (int)result);
        // if (added_region && !(path_copy.empty() || name_copy.empty())) {
        //     Elf::LoadSymbols(name_copy, path_copy, (void*)min_address_copy);
        // }
        break;
    }
    case felix86_riscv64_shutdown: {
        result = SYSCALL64(shutdown, arg1, arg2);
        STRACE("shutdown(%d, %d) = %d", (int)arg1, (int)arg2, (int)result);
        break;
    }
    case felix86_riscv64_shmget: {
        result = SYSCALL64(shmget, arg1, arg2, arg3);
        STRACE("shmget(%d, %d, %d) = %d", (int)arg1, (int)arg2, (int)arg3, (int)result);
        break;
    }
    case felix86_riscv64_shmat: {
        result = SYSCALL64(shmat, arg1, (void*)arg2, arg3);
        STRACE("shmat(%d, %p, %d) = %d", (int)arg1, (void*)arg2, (int)arg3, (int)result);
        break;
    }
    case felix86_riscv64_shmctl: {
        result = SYSCALL64(shmctl, arg1, arg2, arg3);
        STRACE("shmctl(%d, %d, %p) = %d", (int)arg1, (int)arg2, (void*)arg3, (int)result);
        break;
    }
    case felix86_riscv64_shmdt: {
        result = SYSCALL64(shmdt, (void*)arg1);
        STRACE("shmdt(%p) = %d", (void*)arg1, (int)result);
        break;
    }
    case felix86_riscv64_bind: {
        result = SYSCALL64(bind, arg1, (struct sockaddr*)arg2, arg3);
        STRACE("bind(%d, %p, %d) = %d", (int)arg1, (void*)arg2, (int)arg3, (int)result);
        break;
    }
    case felix86_riscv64_setpgid: {
        result = SYSCALL64(setpgid, arg1, arg2);
        STRACE("setpgid(%d, %d) = %d", (int)arg1, (int)arg2, (int)result);
        break;
    }
    case felix86_riscv64_setpriority: {
        result = SYSCALL64(setpriority, arg1, arg2, arg3);
        STRACE("setpriority(%d, %d, %d) = %d", (int)arg1, (int)arg2, (int)arg3, (int)result);
        break;
    }
    case felix86_riscv64_getpriority: {
        result = SYSCALL64(getpriority, arg1, arg2);
        STRACE("getpriority(%d, %d) = %d", (int)arg1, (int)arg2, (int)result);
        break;
    }
    case felix86_riscv64_getrusage: {
        result = SYSCALL64(getrusage, arg1, (struct rusage*)arg2);
        STRACE("getrusage(%d, %p) = %d", (int)arg1, (void*)arg2, (int)result);
        break;
    }
    case felix86_riscv64_getcwd: {
        result = Filesystem::Getcwd((char*)arg1, arg2);
        STRACE("getcwd(%s, %d) = %d", (char*)arg1, (int)arg2, (int)result);
        break;
    }
    case felix86_riscv64_epoll_ctl: {
        result = SYSCALL64(epoll_ctl, arg1, arg2, arg3, arg4);
        STRACE("epoll_ctl(%d, %d, %d, %p) = %d", (int)arg1, (int)arg2, (int)arg3, (void*)arg4, (int)result);
        break;
    }
    case felix86_riscv64_epoll_pwait: {
        result = SYSCALL64(epoll_pwait, arg1, arg2, arg3, arg4, arg5, arg6);
        STRACE("epoll_pwait(%d, %p, %d, %d, %p, %d) = %d", (int)arg1, (void*)arg2, (int)arg3, (int)arg4, (void*)arg5, (int)arg6, (int)result);
        break;
    }
    case felix86_riscv64_epoll_pwait2: {
        result = SYSCALL64(epoll_pwait2, arg1, arg2, arg3, arg4, arg5, arg6);
        STRACE("epoll_pwait2(%d, %p, %d, %d, %p, %d) = %d", (int)arg1, (void*)arg2, (int)arg3, (int)arg4, (void*)arg5, (int)arg6, (int)result);
        break;
    }
    case felix86_riscv64_mount: {
        result = SYSCALL64(mount, arg1, arg2, arg3, arg4, arg5);
        STRACE("mount(%p, %p, %p, %lx, %p) = %d", (void*)arg1, (void*)arg2, (void*)arg3, arg4, (void*)arg5, (int)result);
        break;
    }
    case felix86_riscv64_accept: {
        result = SYSCALL64(accept, arg1, arg2, arg3);
        STRACE("accept(%d, %p, %p) = %d", (int)arg1, (void*)arg2, (void*)arg3, (int)result);
        break;
    }
    case felix86_riscv64_socketpair: {
        result = SYSCALL64(socketpair, arg1, arg2, arg3, arg4);
        STRACE("socketpair(%d, %d, %d, %p) = %d", (int)arg1, (int)arg2, (int)arg3, (void*)arg4, (int)result);
        break;
    }
    case felix86_riscv64_setgid: {
        result = SYSCALL64(setgid, arg1);
        STRACE("setgid(%d) = %d", (int)arg1, (int)result);
        break;
    }
    case felix86_riscv64_setsid: {
        result = SYSCALL64(setsid, arg1);
        STRACE("setsid(%d) = %d", (int)arg1, (int)result);
        break;
    }
    case felix86_riscv64_setreuid: {
        result = SYSCALL64(setreuid, arg1, arg2);
        STRACE("setreuid(%d, %d) = %d", (int)arg1, (int)arg2, (int)result);
        break;
    }
    case felix86_riscv64_setresuid: {
        result = SYSCALL64(setresuid, arg1, arg2, arg3);
        STRACE("setreuid(%d, %d, %d) = %d", (int)arg1, (int)arg2, (int)arg3, (int)result);
        break;
    }
    case felix86_riscv64_setregid: {
        result = SYSCALL64(setregid, arg1, arg2);
        STRACE("setregid(%d, %d) = %d", (int)arg1, (int)arg2, (int)result);
        break;
    }
    case felix86_riscv64_setgroups: {
        result = SYSCALL64(setgroups, arg1, arg2);
        STRACE("setgroups(%d, %p) = %d", (int)arg1, (void*)arg2, (int)result);
        break;
    }
    case felix86_riscv64_getgroups: {
        result = SYSCALL64(getgroups, arg1, arg2);
        STRACE("getgroups(%d, %p) = %d", (int)arg1, (void*)arg2, (int)result);
        break;
    }
    case felix86_riscv64_setuid: {
        result = SYSCALL64(setuid, arg1);
        STRACE("setuid(%d) = %d", (int)arg1, (int)result);
        break;
    }
    case felix86_riscv64_umount2: {
        result = SYSCALL64(umount2, arg1, arg2);
        STRACE("umount2(%s, %lx) = %d", (const char*)arg1, arg2, (int)result);
        break;
    }
    case felix86_riscv64_sched_getscheduler: {
        result = SYSCALL64(sched_getscheduler, arg1);
        STRACE("sched_getscheduler(%d) = %d", (int)arg1, (int)result);
        break;
    }
    case felix86_riscv64_sched_getparam: {
        result = SYSCALL64(sched_getparam, arg1, (struct sched_param*)arg2);
        STRACE("sched_getparam(%d, %p) = %d", (int)arg1, (void*)arg2, (int)result);
        break;
    }
    case felix86_riscv64_sched_setparam: {
        result = SYSCALL64(sched_setparam, arg1, arg2);
        STRACE("sched_setparam(%d, %p) = %d", (int)arg1, (void*)arg2, (int)result);
        break;
    }
    case felix86_riscv64_clock_gettime: {
        result = SYSCALL64(clock_gettime, arg1, (struct timespec*)arg2);
        STRACE("clock_gettime(%d, %p) = %d", (int)arg1, (void*)arg2, (int)result);
        break;
    }
    case felix86_riscv64_clock_getres: {
        result = SYSCALL64(clock_getres, arg1, (struct timespec*)arg2);
        STRACE("clock_getres(%d, %p) = %d", (int)arg1, (void*)arg2, (int)result);
        break;
    }
    case felix86_riscv64_getresuid: {
        result = SYSCALL64(getresuid, (uid_t*)arg1, (uid_t*)arg2, (uid_t*)arg3);
        STRACE("getresuid(%p, %p, %p) = %d", (void*)arg1, (void*)arg2, (void*)arg3, (int)result);
        break;
    }
    case felix86_riscv64_getresgid: {
        result = SYSCALL64(getresgid, (gid_t*)arg1, (gid_t*)arg2, (gid_t*)arg3);
        STRACE("getresgid(%p, %p, %p) = %d", (void*)arg1, (void*)arg2, (void*)arg3, (int)result);
        break;
    }
    case felix86_riscv64_gettimeofday: {
        result = SYSCALL64(gettimeofday, (struct timeval*)arg1, (struct timezone*)arg2);
        STRACE("gettimeofday(%p, %p) = %d", (void*)arg1, (void*)arg2, (int)result);
        break;
    }
    case felix86_riscv64_dup: {
        result = SYSCALL64(dup, arg1);
        STRACE("dup(%d) = %d", (int)arg1, (int)result);
        break;
    }
    case felix86_riscv64_dup3: {
        result = SYSCALL64(dup3, arg1, arg2, arg3);
        STRACE("dup3(%d, %d, %d) = %d", (int)arg1, (int)arg2, (int)arg3, (int)result);
        break;
    }
    case felix86_riscv64_fstat: {
        x64Stat* guest_stat = (x64Stat*)arg2;
        struct stat host_stat;
        result = SYSCALL64(fstat, arg1, &host_stat);
        STRACE("fstat(%d, %p) = %d", (int)arg1, (void*)arg2, (int)result);
        if (result >= 0) {
            *guest_stat = host_stat;
        }
        break;
    }
    case felix86_riscv64_fsync: {
        result = SYSCALL64(fsync, arg1);
        STRACE("fsync(%d) = %d", (int)arg1, (int)result);
        break;
    }
    case felix86_riscv64_sync: {
        result = SYSCALL64(sync);
        STRACE("sync() = %d", (int)result);
        break;
    }
    case felix86_riscv64_syncfs: {
        result = SYSCALL64(syncfs, arg1);
        STRACE("syncfs(%d) = %d", (int)arg1, (int)result);
        break;
    }
    case felix86_riscv64_sendmmsg: {
        result = SYSCALL64(sendmmsg, arg1, (struct mmsghdr*)arg2, arg3, arg4);
        STRACE("sendmmsg(%d, %p, %d, %d) = %d", (int)arg1, (void*)arg2, (int)arg3, (int)arg4, (int)result);
        break;
    }
    case felix86_riscv64_recvmmsg: {
        result = SYSCALL64(recvmmsg, arg1, (struct mmsghdr*)arg2, arg3, arg4, arg5);
        STRACE("recvmmsg(%d, %p, %d, %d, %d) = %d", (int)arg1, (void*)arg2, (int)arg3, (int)arg4, (int)arg5, (int)result);
        break;
    }
    case felix86_riscv64_setsockopt: {
        result = SYSCALL64(setsockopt, arg1, arg2, arg3, arg4, arg5);
        STRACE("setsockopt(%d, %d, %d, %d, %d) = %d", (int)arg1, (int)arg2, (int)arg3, (int)arg4, (int)arg5, (int)result);
        break;
    }
    case felix86_riscv64_getsockopt: {
        result = SYSCALL64(getsockopt, arg1, arg2, arg3, arg4, arg5);
        STRACE("getsockopt(%d, %d, %d, %d, %d) = %d", (int)arg1, (int)arg2, (int)arg3, (int)arg4, (int)arg5, (int)result);
        break;
    }
    case felix86_riscv64_statx: {
        result = Filesystem::Statx((int)arg1, (char*)arg2, (int)arg3, (u32)arg4, (struct statx*)arg5);
        STRACE("statx(%d, %s, %d, %d, %p) = %d", (int)arg1, (const char*)arg2, (int)arg3, (int)arg4, (void*)arg5, (int)result);
        break;
    }
    case felix86_riscv64_fadvise64: {
        result = SYSCALL64(fadvise64, arg1, arg2, arg3, arg4);
        STRACE("fadvise64(%d, %d, %d, %d) = %d", (int)arg1, (int)arg2, (int)arg3, (int)arg4, (int)result);
        break;
    }
    case felix86_riscv64_fcntl: {
        result = SYSCALL64(fcntl, arg1, arg2, arg3);
        STRACE("fcntl(%d, %d, %d) = %d", (int)arg1, (int)arg2, (int)arg3, (int)result);
        break;
    }
    case felix86_riscv64_pselect6: {
        result = SYSCALL64(pselect6, arg1, arg2, arg3, arg4, arg5, arg6);
        STRACE("pselect6(%d, %p, %p, %p, %p, %p) = %d", (int)arg1, (void*)arg2, (void*)arg3, (void*)arg4, (void*)arg5, (void*)arg6, (int)result);
        break;
    }
    case felix86_riscv64_chdir: {
        result = Filesystem::Chdir((char*)arg1);
        STRACE("chdir(%s) = %d", (const char*)arg1, (int)result);
        break;
    }
    case felix86_riscv64_fchown: {
        result = SYSCALL64(fchown, arg1, arg2, arg3);
        STRACE("fchown(%d, %d, %d) = %d", (int)arg1, (int)arg2, (int)arg3, (int)result);
        break;
    }
    case felix86_riscv64_unlinkat: {
        result = Filesystem::UnlinkAt((int)arg1, (char*)arg2, (int)arg3);
        STRACE("unlinkat(%d, %s, %d) = %d", (int)arg1, (const char*)arg2, (int)arg3, (int)result);
        break;
    }
    case felix86_riscv64_fchdir: {
        result = SYSCALL64(fchdir, arg1);
        STRACE("fchdir(%d) = %d", (int)arg1, (int)result);
        break;
    }
    case felix86_riscv64_newfstatat: {
        result = Filesystem::FStatAt((int)arg1, (char*)arg2, (x64Stat*)arg3, (int)arg4);
        STRACE("newfstatat(%d, %s, %p, %d) = %d", (int)arg1, (char*)arg2, (void*)arg3, (int)arg4, (int)result);
        break;
    }
    case felix86_riscv64_sysinfo: {
        result = SYSCALL64(sysinfo, arg1);
        STRACE("sysinfo(%p) = %d", (void*)arg1, (int)result);
        break;
    }
    case felix86_riscv64_ioctl: {
        result = SYSCALL64(ioctl, arg1, arg2, arg3);

        if (!try_strace_ioctl(arg1, arg2, arg3, result)) {
            STRACE("ioctl(%d, %lx, %lx) = %lx", (int)arg1, arg2, arg3, (u64)result);
        }
        break;
    }
    case felix86_riscv64_write: {
        result = SYSCALL64(write, arg1, arg2, arg3);

        if (g_strace) {
            STRACE("write(%d, %s, %d) = %d", (int)arg1, (char*)arg2, (int)arg3, (int)result);
        }
        break;
    }
    case felix86_riscv64_writev: {
        result = SYSCALL64(writev, arg1, arg2, arg3);
        STRACE("writev(%d, %p, %d) = %d", (int)arg1, (void*)arg2, (int)arg3, (int)result);
        break;
    }
    case felix86_riscv64_exit_group: {
        STRACE("exit_group(%d)", (int)arg1);
        state->exit_reason = EXIT_REASON_EXIT_GROUP_SYSCALL;
        state->exit_code = arg1;
        Emulator::ExitDispatcher(state);
        UNREACHABLE();
        break;
    }
    case felix86_riscv64_faccessat:
    case felix86_riscv64_faccessat2: {
        result = Filesystem::FAccessAt((int)arg1, (char*)arg2, (int)arg3, (int)arg4);
        STRACE("faccessat2(%d, %s, %d, %d) = %d", (int)arg1, (const char*)arg2, (int)arg3, (int)arg4, (int)result);
        break;
    }
    case felix86_riscv64_pipe2: {
        result = SYSCALL64(pipe2, arg1, arg2);
        STRACE("pipe2(%p, %d) = %d", (void*)arg1, (int)arg2, (int)result);
        break;
    }
    case felix86_riscv64_memfd_create: {
        result = SYSCALL64(memfd_create, (const char*)arg1, arg2);
        STRACE("memfd_create(%s, %d) = %d", (const char*)arg1, (int)arg2, (int)result);
        break;
    }
    case felix86_riscv64_ftruncate: {
        result = SYSCALL64(ftruncate, arg1, arg2);
        STRACE("ftruncate(%d, %d) = %d", (int)arg1, (int)arg2, (int)result);
        break;
    }
    case felix86_riscv64_read: {
        result = SYSCALL64(read, arg1, arg2, arg3);
        STRACE("read(%d, %p, %d) = %d", (int)arg1, (void*)arg2, (int)arg3, (int)result);
        break;
    }
    case felix86_riscv64_getdents64: {
        result = SYSCALL64(getdents64, arg1, arg2, arg3);
        STRACE("getdents64(%d, %p, %d) = %d", (int)arg1, (void*)arg2, (int)arg3, (int)result);
        break;
    }
    case felix86_riscv64_lgetxattr: {
        result = Filesystem::LGetXAttr((char*)arg1, (char*)arg2, (void*)arg3, arg4);
        STRACE("lgetxattr(%s, %s, %p, %d) = %d", (char*)arg1, (char*)arg2, (void*)arg3, (int)arg4, (int)result);
        break;
    }
    case felix86_riscv64_pwrite64: {
        result = SYSCALL64(pwrite64, arg1, arg2, arg3, arg4);
        STRACE("pwrite64(%d, %p, %d, %d) = %d", (int)arg1, (void*)arg2, (int)arg3, (int)arg4, (int)result);
        break;
    }
    case felix86_riscv64_pread64: {
        result = SYSCALL64(pread64, arg1, arg2, arg3, arg4);
        STRACE("pread64(%d, %p, %d, %d) = %d", (int)arg1, (void*)arg2, (int)arg3, (int)arg4, (int)result);
        break;
    }
    case felix86_riscv64_openat: {
        if (std::string((char*)arg2) == "/run/systemd/userdb/") { // TODO: There's some bug in Qt apps with this path??
            WARN("Accessing /run/systemd/userdb/, returning -ENOENT");
            result = -ENOENT;
            break;
        }

        result = Filesystem::OpenAt((int)arg1, (char*)arg2, (int)arg3, arg4);
        STRACE("openat(%d, %s, %d, %d) = %d", (int)arg1, (const char*)arg2, (int)arg3, (int)arg4, (int)result);
        break;
    }
    case felix86_riscv64_tgkill: {
        result = SYSCALL64(tgkill, arg1, arg2, arg3);
        STRACE("tgkill(%d, %d, %d) = %d", (int)arg1, (int)arg2, (int)arg3, (int)result);
        break;
    }
    case felix86_riscv64_kill: {
        result = SYSCALL64(kill, arg1, arg2);
        STRACE("kill(%d, %d) = %d", (int)arg1, (int)arg2, (int)result);
        break;
    }
    case felix86_riscv64_mmap: {
        if ((int)arg5 != -1) {
            // uses file descriptor, mmaps file to memory, may need to update mappings
            // this can occur when using something like dlopen or when the interpreter initially loads the symbols
            g_symbols_cached = false;
        }

#ifndef MAP_32BIT
#define MAP_32BIT 0x40
#endif
        u64 flags = arg4;
        bool is_fixed = (flags & MAP_FIXED) || (flags & MAP_FIXED_NOREPLACE);
        if ((flags & MAP_32BIT) || (is_fixed && arg1 < Mapper::addressSpaceEnd32) || g_mode32) {
            // The MAP_32BIT flag is x86 only so we need to emulate it
            // For example, Mono tries to use it to allocate code cache pages near the executable so that it can use
            // +-2GiB jumps. If it doesn't get them near enough it will eventually crash and die.
            // We need to also track fixed mappings in the 32-bit address space
            result = (ssize_t)g_mapper->map32((void*)arg1, arg2, arg3, (int)arg4, (int)arg5, arg6);
            STRACE("mmap32(%p, %lx, %d, %x, %d, %d) = %lx", (void*)arg1, arg2, (int)arg3, (int)arg4, (int)arg5, (int)arg6, (u64)result);
        } else {
            // No need to use mapper
            result = SYSCALL64(mmap, arg1, arg2, arg3, (int)arg4, (int)arg5, arg6);
            STRACE("mmap(%p, %lx, %d, %x, %d, %d) = %lx", (void*)arg1, arg2, (int)arg3, (int)arg4, (int)arg5, (int)arg6, (u64)result);
        }
        break;
    }
    case felix86_riscv64_munmap: {
        if (arg1 < Mapper::addressSpaceEnd32 || g_mode32) {
            // Track unmaps in the 32-bit address space for MAP_32BIT in 64-bit mode
            result = g_mapper->unmap32((void*)arg1, arg2);
            STRACE("munmap32(%p, %lx) = %lx", (void*)arg1, arg2, (u64)result);
        } else {
            result = SYSCALL64(munmap, arg1, arg2);
            STRACE("munmap(%p, %lx) = %lx", (void*)arg1, arg2, (u64)result);
        }
        break;
    }
    case felix86_riscv64_setitimer: {
        result = SYSCALL64(setitimer, arg1, arg2, arg3);
        STRACE("setitimer(%d, %p, %p) = %d", (int)arg1, (void*)arg1, (void*)arg2, (int)result);
        break;
    }
    case felix86_riscv64_timer_create: {
        result = SYSCALL64(timer_create, arg1, arg2, arg3);
        STRACE("timer_create(%ld, %p, %p) = %d", (long)arg1, (void*)arg2, (void*)arg3, (int)result);
        break;
    }
    case felix86_riscv64_timer_gettime: {
        result = SYSCALL64(timer_gettime, arg1, arg2);
        STRACE("timer_gettime(%ld, %p) = %d", (long)arg1, (void*)arg2, (int)result);
        break;
    }
    case felix86_riscv64_timer_settime: {
        result = SYSCALL64(timer_settime, arg1, arg2, arg3, arg4);
        STRACE("timer_gettime(%ld, %p, %p, %p) = %d", (long)arg1, (void*)arg2, (void*)arg3, (void*)arg4, (int)result);
        break;
    }
    case felix86_riscv64_timer_getoverrun: {
        result = SYSCALL64(timer_getoverrun, arg1);
        STRACE("timer_getoverrun(%ld) = %d", (long)arg1, (int)result);
        break;
    }
    case felix86_riscv64_timer_delete: {
        result = SYSCALL64(timer_delete, arg1);
        STRACE("timer_delete(%ld) = %d", (long)arg1, (int)result);
        break;
    }
    case felix86_riscv64_getuid: {
        result = SYSCALL64(getuid);
        STRACE("getuid() = %d", (int)result);
        break;
    }
    case felix86_riscv64_fdatasync: {
        result = SYSCALL64(fdatasync, arg1);
        STRACE("fdatasync(%d) = %d", (int)arg1, (int)result);
        break;
    }
    case felix86_riscv64_geteuid: {
        result = SYSCALL64(geteuid);
        STRACE("geteuid() = %d", (int)result);
        break;
    }
    case felix86_riscv64_getegid: {
        result = SYSCALL64(getegid);
        STRACE("getegid() = %d", (int)result);
        break;
    }
    case felix86_riscv64_utimensat: {
        result = Filesystem::UtimensAt(arg1, (const char*)arg2, (struct timespec*)arg3, arg4);
        STRACE("utimensat(%d, %s, %p, %d) = %d", (int)arg1, (const char*)arg2, (void*)arg3, (int)arg4, (int)result);
        break;
    }
    case felix86_riscv64_getgid: {
        result = SYSCALL64(getgid);
        STRACE("getgid() = %d", (int)result);
        break;
    }
    case felix86_riscv64_setfsgid: {
        result = SYSCALL64(setfsgid, arg1);
        STRACE("setfsgid(%d) = %d", (int)arg1, (int)result);
        break;
    }
    case felix86_riscv64_setfsuid: {
        result = SYSCALL64(setfsuid, arg1);
        STRACE("setfsuid(%d) = %d", (int)arg1, (int)result);
        break;
    }
    case felix86_riscv64_getppid: {
        result = SYSCALL64(getppid);
        STRACE("getppid() = %d", (int)result);
        break;
    }
    case felix86_riscv64_getpid: {
        result = SYSCALL64(getpid);
        STRACE("getpid() = %d", (int)result);
        break;
    }
    case felix86_riscv64_gettid: {
        result = SYSCALL64(gettid);
        STRACE("gettid() = %d", (int)result);
        break;
    }
    case felix86_riscv64_socket: {
        result = SYSCALL64(socket, arg1, arg2, arg3);
        STRACE("socket(%d, %d, %d) = %d", (int)arg1, (int)arg2, (int)arg3, (int)result);
        break;
    }
    case felix86_riscv64_connect: {
        result = SYSCALL64(connect, arg1, arg2, arg3);
        STRACE("connect(%d, %p, %d) = %d", (int)arg1, (void*)arg2, (int)arg3, (int)result);
        break;
    }
    case felix86_riscv64_mremap: {
        result = SYSCALL64(mremap, arg1, arg2, arg3, arg4, arg5);
        STRACE("mremap(%p, %lx, %lx, %d, %lx) = %lx", (void*)arg1, arg2, arg3, (int)arg4, arg5, (u64)result);
        break;
    }
    case felix86_riscv64_msync: {
        result = SYSCALL64(msync, arg1, arg2, arg3);
        STRACE("msync(%p, %lx, %d) = %lx", (void*)arg1, arg2, (int)arg3, (u64)result);
        break;
    }
    case felix86_riscv64_sendto: {
        result = SYSCALL64(sendto, arg1, arg2, arg3, arg4, arg5, arg6);
        STRACE("sendto(%d, %p, %d, %d, %p, %d) = %d", (int)arg1, (void*)arg2, (int)arg3, (int)arg4, (void*)arg5, (int)arg6, (int)result);
        break;
    }
    case felix86_riscv64_times: {
        result = SYSCALL64(times, (struct tms*)arg1);
        STRACE("times(%p) = %d", (void*)arg1, (int)result);
        break;
    }
    case felix86_riscv64_recvfrom: {
        result = SYSCALL64(recvfrom, arg1, arg2, arg3, arg4, arg5, arg6);
        STRACE("recvfrom(%d, %p, %d, %d, %p, %p) = %d", (int)arg1, (void*)arg2, (int)arg3, (int)arg4, (void*)arg5, (void*)arg6, (int)result);
        break;
    }
    case felix86_riscv64_lseek: {
        result = SYSCALL64(lseek, arg1, arg2, arg3);
        STRACE("lseek(%d, %d, %d) = %d", (int)arg1, (int)arg2, (int)arg3, (int)result);
        break;
    }
    case felix86_riscv64_uname: {
        struct utsname host_uname;
        struct utsname* guest_uname = (struct utsname*)arg1;
        if (uname(&host_uname) == 0) {
            memcpy(guest_uname->nodename, host_uname.nodename, sizeof(host_uname.nodename));
            memcpy(guest_uname->domainname, host_uname.domainname, sizeof(host_uname.domainname));
        } else {
            strcpy(guest_uname->nodename, "felix86");
            WARN("Failed to determine host node name");
        }
        strcpy(guest_uname->sysname, "Linux");
        strcpy(guest_uname->release, "5.0.0");
        std::string vearg2on = "#1 SMP " __DATE__ " " __TIME__;
        strcpy(guest_uname->version, vearg2on.c_str());
        strcpy(guest_uname->machine, "x86_64");
        result = 0;
        break;
    }
    case felix86_riscv64_timerfd_create: {
        result = SYSCALL64(timerfd_create, arg1, arg2);
        STRACE("timerfd_create(%d, %d) = %d", (int)arg1, (int)arg2, (int)result);
        break;
    }
    case felix86_riscv64_timerfd_settime: {
        result = SYSCALL64(timerfd_settime, arg1, arg2, arg3, arg4);
        STRACE("timerfd_settime(%d, %d, %p, %p) = %d", (int)arg1, (int)arg2, (void*)arg3, (void*)arg4, (int)result);
        break;
    }
    case felix86_riscv64_timerfd_gettime: {
        result = SYSCALL64(timerfd_gettime, arg1, (struct itimerspec*)arg2);
        STRACE("timerfd_gettime(%d, %p) = %d", (int)arg1, (void*)arg2, (int)result);
        break;
    }
    case felix86_riscv64_statfs: {
        result = Filesystem::StatFs((char*)arg1, (struct statfs*)arg2);
        STRACE("statfs(%s, %p) = %d", (char*)arg1, (void*)arg2, (int)result);
        break;
    }
    case felix86_riscv64_fstatfs: {
        result = SYSCALL64(fstatfs, arg1, (struct statfs*)arg2);
        STRACE("fstatfs(%d, %p) = %d", (int)arg1, (void*)arg2, (int)result);
        break;
    }
    case felix86_riscv64_getsockname: {
        result = SYSCALL64(getsockname, arg1, (struct sockaddr*)arg2, (socklen_t*)arg3);
        STRACE("getsockname(%d, %p, %p) = %d", (int)arg1, (void*)arg2, (void*)arg3, (int)result);
        break;
    }
    case felix86_riscv64_madvise: {
        result = SYSCALL64(madvise, arg1, arg2, arg3);
        STRACE("madvise(%p, %lx, %d) = %d", (void*)arg1, arg2, (int)arg3, (int)result);
        break;
    }
    case felix86_riscv64_exit: {
        STRACE("exit(%d)", (int)arg1);
        state->exit_reason = ExitReason::EXIT_REASON_EXIT_SYSCALL;
        state->exit_code = arg1;
        Emulator::ExitDispatcher(state);
        UNREACHABLE();
        break;
    }
    case felix86_riscv64_eventfd2: {
        result = SYSCALL64(eventfd2, arg1, arg2);
        STRACE("eventfd2(%d, %d) = %d", (int)arg1, (int)arg2, (int)result);
        break;
    }
    case felix86_riscv64_fchmod: {
        result = SYSCALL64(fchmod, arg1, arg2);
        STRACE("fchmod(%d, %d) = %d", (int)arg1, (int)arg2, (int)result);
        break;
    }
    case felix86_riscv64_fchmodat: {
        result = Filesystem::FChmodAt((int)arg1, (char*)arg2, arg3);
        STRACE("fchmodat(%d, %s, %lx) = %d", (int)arg1, (char*)arg2, arg3, (int)result);
        break;
    }
    case felix86_riscv64_recvmsg: {
        result = SYSCALL64(recvmsg, arg1, (struct msghdr*)arg2, arg3);
        STRACE("recvmsg(%d, %p, %d) = %d", (int)arg1, (void*)arg2, (int)arg3, (int)result);
        break;
    }
    case felix86_riscv64_sendmsg: {
        result = SYSCALL64(sendmsg, arg1, (struct msghdr*)arg2, arg3);
        STRACE("sendmsg(%d, %p, %d) = %d", (int)arg1, (void*)arg2, (int)arg3, (int)result);
        break;
    }
    case felix86_riscv64_semget: {
        result = SYSCALL64(semget, arg1, arg2, arg3);
        STRACE("semget(%d, %d, %d) = %d", (int)arg1, (int)arg2, (int)arg3, (int)result);
        break;
    }
    case felix86_riscv64_semop: {
        result = SYSCALL64(semop, arg1, arg2, arg3);
        STRACE("semop(%d, %p, %d) = %d", (int)arg1, (void*)arg2, (int)arg3, (int)result);
        break;
    }
    case felix86_riscv64_semtimedop: {
        result = SYSCALL64(semtimedop, arg1, arg2, arg3, arg4);
        STRACE("semtimedop(%d, %p, %d, %p) = %d", (int)arg1, (void*)arg2, (int)arg3, (void*)arg4, (int)result);
        break;
    }
    case felix86_riscv64_semctl: {
        result = SYSCALL64(semctl, arg1, arg2, arg3, arg4);
        STRACE("semctl(%d, %d, %d, %ld) = %d", (int)arg1, (int)arg2, (int)arg3, arg4, (int)result);
        break;
    }
    case felix86_riscv64_flock: {
        result = SYSCALL64(flock, arg1, arg2);
        STRACE("flock(%d, %d) = %d", (int)arg1, (int)arg2, (int)result);
        break;
    }
    case felix86_riscv64_clock_nanosleep: {
        result = SYSCALL64(clock_nanosleep, arg1, arg2, arg3, arg4);
        STRACE("clock_nanosleep(%d, %d, %p, %p) = %d", (int)arg1, (int)arg2, (void*)arg3, (void*)arg4, (int)result);
        break;
    }
    case felix86_riscv64_rt_sigaction: {
        struct x86_sigaction* act = (struct x86_sigaction*)arg2;
        if (act) {
            auto handler = act->handler;
            Signals::registerSignalHandler(state, arg1, GuestAddress{(u64)handler}, act->sa_mask, act->sa_flags);
            if (g_verbose) {
                PLAIN("Installed signal handler %s at:", strsignal(arg1));
                print_address((u64)handler);
                PLAIN("Flags: %lx\n", act->sa_flags);
            }
        }

        struct sigaction* old_act = (struct sigaction*)arg3;
        if (old_act) {
            RegisteredSignal old = Signals::getSignalHandler(state, arg1);
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
        STRACE("rt_sigaction(%d, %p, %p) = %d", (int)arg1, (void*)arg2, (void*)arg4, (int)result);
        break;
    }
    case felix86_riscv64_rt_sigtimedwait: {
        result = SYSCALL64(rt_sigtimedwait, arg1, arg2, arg3, arg4);
        STRACE("rt_sigtimedwait(%p, %p, %p, %d, %d) = %d", (void*)arg1, (void*)arg2, (void*)arg3, (int)arg4, (int)arg5, (int)result);
        WARN_ONCE("This program uses rt_sigtimedwait");
        break;
    }
    case felix86_riscv64_sched_yield: {
        result = SYSCALL64(sched_yield);
        STRACE("sched_yield() = %d", (int)result);
        break;
    }
    case felix86_riscv64_sigaltstack: {
        VERBOSE("----- sigaltstack was called -----");
        stack_t host_stack; // save old stack here while we check if guest stack is valid
        stack_t* guest_stack = (stack_t*)arg1;
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

        stack_t* new_ss = (stack_t*)arg1;
        stack_t* old_ss = (stack_t*)arg2;

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
    case felix86_riscv64_prctl: {
#ifndef PR_GET_AUXV
#define PR_GET_AUXV 0x41555856
#endif
        int option = arg1;
        switch (option) {
        case PR_GET_AUXV: {
            if (arg4 || arg5) {
                WARN("PR_GET_AUXV with arg4 or arg5");
                result = -EINVAL;
            } else {
                void* addr = (void*)arg2;
                size_t size = arg3;
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
            result = SYSCALL64(prctl, arg1, arg2, arg3, arg4, arg5);
            break;
        }
        }
        STRACE("prctl(%d, %lx, %lx, %lx, %lx) = %d", (int)arg1, arg2, arg3, arg4, arg5, (int)result);
        break;
    }
    case felix86_riscv64_futex: {
        result = SYSCALL64(futex, arg1, arg2, arg3, arg4, arg5, arg6);
        STRACE("futex(%p, %d, %d, %p, %p, %d) = %d", (void*)arg1, (int)arg2, (int)arg3, (void*)arg4, (void*)arg5, (int)arg6, (int)result);
        break;
    }
    case felix86_riscv64_inotify_init1: {
        result = SYSCALL64(inotify_init1, arg1);
        STRACE("inotify_init1(%d) = %d", (int)arg1, (int)result);
        break;
    }
    case felix86_riscv64_inotify_add_watch: {
        ERROR("TODO: inotify_add_watch move me to Filesystem");
        result = SYSCALL64(inotify_add_watch, arg1, (const char*)arg2, arg3);
        STRACE("inotify_add_watch(%d, %s, %d) = %d", (int)arg1, (const char*)arg2, (int)arg3, (int)result);
        break;
    }
    case felix86_riscv64_inotify_rm_watch: {
        result = SYSCALL64(inotify_rm_watch, arg1, arg2);
        STRACE("inotify_rm_watch(%d, %d) = %d", (int)arg1, (int)arg2, (int)result);
        break;
    }
    case felix86_riscv64_fallocate: {
        result = SYSCALL64(fallocate, arg1, arg2, arg3, arg4);
        STRACE("fallocate(%d, %d, %d, %d) = %d", (int)arg1, (int)arg2, (int)arg3, (int)arg4, (int)result);
        break;
    }
    case felix86_riscv64_sched_getaffinity: {
        result = SYSCALL64(sched_getaffinity, arg1, arg2, arg3);
        STRACE("sched_getaffinity(%d, %d, %p) = %d", (int)arg1, (int)arg2, (void*)arg3, (int)result);
        break;
    }
    case felix86_riscv64_sched_setaffinity: {
        result = SYSCALL64(sched_setaffinity, arg1, arg2, arg3);
        STRACE("sched_setaffinity(%d, %d, %p) = %d", (int)arg1, (int)arg2, (void*)arg3, (int)result);
        break;
    }
    case felix86_riscv64_sched_get_priority_min: {
        result = SYSCALL64(sched_get_priority_min, arg1);
        STRACE("sched_get_priority_min(%d) = %d", (int)arg1, (int)result);
        break;
    }
    case felix86_riscv64_sched_get_priority_max: {
        result = SYSCALL64(sched_get_priority_max, arg1);
        STRACE("sched_get_priority_max(%d) = %d", (int)arg1, (int)result);
        break;
    }
    case felix86_riscv64_sched_setscheduler: {
        result = SYSCALL64(sched_setscheduler, arg1, arg2, arg3);
        STRACE("sched_setscheduler(%d, %d, %p) = %d", (int)arg1, (int)arg2, (void*)arg3, (int)result);
        break;
    }
    case felix86_riscv64_mincore: {
        result = SYSCALL64(mincore, arg1, arg2, arg3);
        STRACE("mincore(%p, %d, %p) = %d", (void*)arg1, (int)arg2, (void*)arg3, (int)result);
        break;
    }
    case felix86_riscv64_listen: {
        result = SYSCALL64(listen, arg1, arg2);
        STRACE("listen(%d, %d) = %d", (int)arg1, (int)arg2, (int)result);
        break;
    }
    case felix86_riscv64_clone3: {
        result = -ENOSYS; // don't support these for now
        break;
    }
    case felix86_riscv64_clone: {
        clone_args args;
        memset(&args, 0, sizeof(clone_args));
        args.flags = arg1;
        args.stack = arg2;
        args.parent_tid = arg3;
        args.child_tid = arg4;
        args.tls = arg5;
        result = Threads::Clone(state, &args);
        break;
    }
    case felix86_riscv64_wait4: {
        result = SYSCALL64(wait4, arg1, arg2, arg3, arg4);
        STRACE("wait4(%d, %p, %d, %p)", (int)arg1, (void*)arg2, (int)arg3, (void*)arg4);
        break;
    }
    case felix86_riscv64_execve: {
        if (!arg1) {
            WARN("execve with nullptr as executable path?");
            result = -EINVAL;
            break;
        }

        std::filesystem::path path = Symlinker::resolve((char*)arg1);

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

        // Resolving this symlink helps gdb find the path
        std::filesystem::path emulator = g_emulator_path;
        argv.push_back(emulator.c_str());

        if (arg2) {
            const char** guest_argv = (const char**)arg2;
            while (*guest_argv) {
                argv.push_back(*guest_argv);
                guest_argv++;
            }
        } else {
            WARN("argv null during execve...?");
            // Args shouldn't be null normally, but at least push the emulated executable here
            argv.push_back(path.c_str());
        }
        argv.push_back(nullptr);

        if (arg3) {
            const char** guest_env = (const char**)arg3;
            while (*guest_env) {
                envp.push_back(*guest_env);
                guest_env++;
            }
        } else {
            WARN("envp null during execve...?");
        }

        std::string log_env = std::string("__FELIX86_PIPE=") + Logger::getPipeName();
        envp.push_back("__FELIX86_EXECVE=1");
        envp.push_back(log_env.c_str());
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

        syscall(SYS_execve, emulator.c_str(), argv.data(), envp.data());

        UNREACHABLE();
        break;
    }
    case felix86_riscv64_umask: {
        result = SYSCALL64(umask, arg1);
        STRACE("umask(%d) = %d", (int)arg1, (int)result);
        break;
    }
    case felix86_riscv64_linkat: {
        result = Filesystem::LinkAt((int)arg1, (char*)arg2, (int)arg3, (char*)arg4, (int)arg5);
        STRACE("linkat(%d, %s, %d, %s, %d) = %d", (int)arg1, (char*)arg2, (int)arg3, (char*)arg4, (int)arg5, (int)result);
        break;
    }
    case felix86_riscv64_readlinkat: {
        result = Filesystem::ReadlinkAt((int)arg1, (char*)arg2, (char*)arg3, (int)arg4);
        STRACE("readlinkat(%d, %s, %s, %d) = %d", (int)arg1, (const char*)arg2, (char*)arg3, (int)arg4, (int)result);
        break;
    }
    case felix86_riscv64_getpeername: {
        result = SYSCALL64(getpeername, arg1, (struct sockaddr*)arg2, (socklen_t*)arg3);
        STRACE("getpeername(%d, %p, %p) = %d", (int)arg1, (void*)arg2, (void*)arg3, (int)result);
        break;
    }
    case felix86_riscv64_rt_sigsuspend: {
        result = Signals::sigsuspend(state, (sigset_t*)arg1);
        STRACE("rt_sigsuspend(%p, %d) = %d", (void*)arg1, (int)arg2, (int)result);
        break;
    }
    case felix86_riscv64_rt_sigprocmask: {
        int how = arg1;
        sigset_t* set = (sigset_t*)arg2;
        sigset_t* oldset = (sigset_t*)arg3;

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

        STRACE("rt_sigprocmask(%d, %p, %p) = %d", (int)arg1, (void*)arg2, (void*)arg3, (int)result);
        break;
    }
    default: {
        result = -ENOSYS;
        ERROR("Unimplemented syscall %s (%d)", riscv_get_name(rv_syscall), rv_syscall);
        break;
    }
    }
    return result;
}

void felix86_syscall(ThreadState* state) {
    u64 syscall_number = state->GetGpr(X86_REF_RAX);
    u64 arg1 = state->GetGpr(X86_REF_RDI);
    u64 arg2 = state->GetGpr(X86_REF_RSI);
    u64 arg3 = state->GetGpr(X86_REF_RDX);
    u64 arg4 = state->GetGpr(X86_REF_R10);
    u64 arg5 = state->GetGpr(X86_REF_R8);
    u64 arg6 = state->GetGpr(X86_REF_R9);

    bool is_common = is_x64_common(syscall_number);
    Result result;

    if (is_common) {
        int rv_syscall = x64_to_riscv(syscall_number);
        result = felix86_syscall_common(state, rv_syscall, arg1, arg2, arg3, arg4, arg5, arg6);
    } else {
        switch (syscall_number) {
        case felix86_x86_64_time: {
            result = ::time((time_t*)arg1);
            STRACE("time(%p) = %lx", (void*)arg1, (u64)result);
            break;
        }
        case felix86_x86_64_readlink: {
            result = Filesystem::ReadlinkAt(AT_FDCWD, (char*)arg1, (char*)arg2, (int)arg3);
            STRACE("readlink(%s, %s, %d) = %d", (const char*)arg1, (char*)arg2, (int)arg3, (int)result);
            break;
        }
        case felix86_x86_64_getpgrp: {
            result = getpgrp();
            STRACE("getpgrp() = %d", (int)result);
            break;
        }
        case felix86_x86_64_rename: {
            result = Filesystem::Rename((char*)arg1, (char*)arg2);
            STRACE("rename(%s, %s) = %d", (char*)arg1, (char*)arg2, (int)result);
            break;
        }
        case felix86_x86_64_epoll_create: {
            // epoll_create has obsolete and ignored argument size, acts the same as epoll_create1 with flags=0
            result = SYSCALL64(epoll_create1, 0);
            STRACE("epoll_create1(%d) = %d", (int)arg1, (int)result);
            break;
        }
        case felix86_x86_64_epoll_create1: {
            result = SYSCALL64(epoll_create1, arg1);
            STRACE("epoll_create1(%d) = %d", (int)arg1, (int)result);
            break;
        }
        case felix86_x86_64_epoll_wait: {
            result = epoll_wait((int)arg1, (struct epoll_event*)arg2, (int)arg3, (int)arg3);
            STRACE("epoll_wait(%d, %p, %d, %d) = %d", (int)arg1, (void*)arg2, (int)arg3, (int)arg4, (int)result);
            break;
        }
        case felix86_x86_64_chmod: {
            result = Filesystem::Chmod((char*)arg1, arg2);
            STRACE("chmod(%s, %d) = %d", (char*)arg1, (int)arg2, (int)result);
            break;
        }
        case felix86_x86_64_symlink: {
            result = Filesystem::Symlink((char*)arg1, (char*)arg2);
            STRACE("symlink(%s, %s) = %d", (char*)arg1, (char*)arg2, (int)result);
            break;
        }
        case felix86_x86_64_poll: {
            result = poll((struct pollfd*)arg1, arg2, arg3);
            STRACE("poll(%p, %d, %d) = %d", (void*)arg1, (int)arg2, (int)arg3, (int)result);
            break;
        }
        case felix86_x86_64_ppoll: {
            result = SYSCALL64(ppoll, arg1, arg2, arg3, arg4);
            STRACE("ppoll(%p, %d, %p, %p) = %d", (void*)arg1, (int)arg2, (void*)arg3, (void*)arg4, (int)result);
            break;
        }
        case felix86_x86_64_dup2: {
            result = ::dup2(arg1, arg2);
            STRACE("dup2(%d, %d) = %d", (int)arg1, (int)arg2, (int)result);
            break;
        }
        case felix86_x86_64_lstat: {
            result = Filesystem::FStatAt(AT_FDCWD, (char*)arg1, (x64Stat*)arg2, AT_SYMLINK_NOFOLLOW);
            STRACE("lstat(%s, %p) = %d", (char*)arg1, (void*)arg2, (int)result);
            break;
        }
        case felix86_x86_64_chown: {
            result = Filesystem::Chown((char*)arg1, arg2, arg3);
            STRACE("chown(%s, %d, %d) = %d", (char*)arg1, (int)arg2, (int)arg3, (int)result);
            break;
        }
        case felix86_x86_64_access: {
            result = Filesystem::FAccessAt(AT_FDCWD, (char*)arg1, (int)arg2, 0);
            STRACE("access(%s, %d) = %d", (const char*)arg1, (int)arg2, (int)result);
            break;
        }
        case felix86_x86_64_pipe: {
            result = ::pipe((int*)arg1);
            STRACE("pipe(%p) = %d", (void*)arg1, (int)result);
            break;
        }
        case felix86_x86_64_mkdir: {
            result = Filesystem::Mkdir((char*)arg1, arg2);
            STRACE("mkdir(%s, %d) = %d", (char*)arg1, (int)arg2, (int)result);
            break;
        }
        case felix86_x86_64_open: {
            result = Filesystem::OpenAt(AT_FDCWD, (char*)arg1, (int)arg2, arg3);
            STRACE("open(%s, %d, %lx) = %d", (char*)arg1, (int)arg2, (long)arg3, (int)result);
            break;
        }
        case felix86_x86_64_alarm: {
            result = ::alarm(arg1);
            STRACE("alarm(%d) = %d", (int)arg1, (int)result);
            break;
        }
        case felix86_x86_64_unlink: {
            result = Filesystem::UnlinkAt(AT_FDCWD, (char*)arg1, 0);
            STRACE("unlink(%s)", (char*)arg1);
            break;
        }
        case felix86_x86_64_stat: {
            result = Filesystem::FStatAt(AT_FDCWD, (char*)arg1, (x64Stat*)arg2, 0);
            STRACE("stat(%s, %p) = %d", (char*)arg1, (void*)arg2, (int)result);
            break;
        }
        case felix86_x86_64_vfork: {
            result = -ENOSYS; // make it use clone instead
            STRACE("vfork() = %d", (int)result);
            break;
        }
        case felix86_x86_64_arch_prctl: {
            switch (arg1) {
            case felix86_x86_64_ARCH_SET_GS: {
                state->gsbase = arg2;
                result = 0;
                break;
            }
            case felix86_x86_64_ARCH_SET_FS: {
                state->fsbase = arg2;
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
                WARN("Unimplemented arch_prctl: %d", (int)arg1);
                result = -EINVAL;
                break;
            }
            }
            STRACE("arch_prctl(%lx, %lx) = %lx", arg1, arg2, (u64)result);
            break;
        }
        default: {
            result = -ENOSYS;
            ERROR("Unimplemented syscall %s (%d)", x64_get_name(syscall_number), (int)syscall_number);
            break;
        }
        }
    }

    state->SetGpr(X86_REF_RAX, result);
}

void felix86_syscall32(ThreadState* state) {
    u64 syscall_number = state->GetGpr(X86_REF_RAX);
    u32 arg1 = state->GetGpr(X86_REF_RBX);
    u32 arg2 = state->GetGpr(X86_REF_RCX);
    u32 arg3 = state->GetGpr(X86_REF_RDX);
    u32 arg4 = state->GetGpr(X86_REF_RSI);
    u32 arg5 = state->GetGpr(X86_REF_RDI);
    u32 arg6 = state->GetGpr(X86_REF_RBP);

    Result result;

    bool is_common = is_x86_common(syscall_number);

    if (is_common) {
        int rv_syscall = x86_to_riscv(syscall_number);
        result = felix86_syscall_common(state, rv_syscall, arg1, arg2, arg3, arg4, arg5, arg6);
    } else {
        switch (syscall_number) {
        case felix86_x86_32_mmap2: {
            // mmap2 is like mmap but file offset is in pages (4096 bytes) to help with the lack of big enough integers in x86-32
            u64 offset = arg6 * 4096;
            result = (ssize_t)g_mapper->map((void*)(u64)arg1, arg2, arg3, arg4, arg5, offset);
            STRACE("mmap2(%p, %x, %d, %x, %d, %d) = %lx", (void*)(u64)arg1, arg2, (int)arg3, (int)arg4, (int)arg5, (int)arg6, (u64)result);
            break;
        }
        default: {
            result = -ENOSYS;
            ERROR("Unimplemented syscall %s (%d)", x86_get_name(syscall_number), (int)syscall_number);
            break;
        }
        }
    }

    state->SetGpr(X86_REF_RAX, result);
}