#include <csignal>
#include <errno.h>
#include <fcntl.h>
#include <fmt/format.h>
#include <linux/futex.h>
#include <linux/sem.h>
#include <poll.h>
#include <sched.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/personality.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#undef VMIN
#include "felix86/common/log.hpp"
#include "felix86/common/script.hpp"
#include "felix86/common/state.hpp"
#include "felix86/common/strace.hpp"
#include "felix86/common/types.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/emulator.hpp"
#include "felix86/hle/brk.hpp"
#include "felix86/hle/fd.hpp"
#include "felix86/hle/filesystem.hpp"
#include "felix86/hle/guest_types.hpp"
#include "felix86/hle/ioctl32.hpp"
#include "felix86/hle/ipc32.hpp"
#include "felix86/hle/mmap.hpp"
#include "felix86/hle/signals.hpp"
#include "felix86/hle/socket32.hpp"
#include "felix86/hle/syscall.hpp"
#include "felix86/hle/thread.hpp"
#include "felix86/v2/recompiler.hpp"

// Annoyingly, the ::syscall function returns -1 instead of the actual error number.
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

#define felix86_x86_64_ARCH_SET_GS 0x1001
#define felix86_x86_64_ARCH_SET_FS 0x1002
#define felix86_x86_64_ARCH_GET_FS 0x1003
#define felix86_x86_64_ARCH_GET_GS 0x1004

#define SYSCALL(name, ...) (syscall(x64_to_riscv(felix86_x86_64_##name), ##__VA_ARGS__))

// TODO: move me elsewhere
bool try_strace_ioctl(int rdi, u64 rsi, u64 rdx, u64 result) {
    if (!g_config.strace) {
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

Result felix86_syscall_common(felix86_frame* frame, int rv_syscall, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5, u64 arg6) {
    ThreadState* state = frame->state;
    Result result;
    switch (rv_syscall) {
    case felix86_riscv64_brk: {
        result = BRK::set(arg1);
        break;
    }
    case felix86_riscv64_kcmp: {
        result = SYSCALL(kcmp, arg1, arg2, arg3, arg4, arg5);
        break;
    }
    case felix86_riscv64_readv: {
        result = SYSCALL(readv, arg1, arg2, arg3);
        break;
    }
    case felix86_riscv64_getrlimit: {
        result = SYSCALL(getrlimit, arg1, arg2);
        break;
    }
    case felix86_riscv64_pidfd_open: {
        result = SYSCALL(pidfd_open, arg1, arg2);
        break;
    }
    case felix86_riscv64_pidfd_getfd: {
        result = SYSCALL(pidfd_getfd, arg1, arg2, arg3);
        break;
    }
    case felix86_riscv64_setrlimit: {
        result = SYSCALL(setrlimit, arg1, arg2);
        break;
    }
    case felix86_riscv64_set_tid_address: {
        state->clear_tid_address = (pid_t*)arg1;
        result = gettid();
        break;
    }
    case felix86_riscv64_set_robust_list: {
        result = SYSCALL(set_robust_list, arg1, arg2);
        break;
    }
    case felix86_riscv64_get_robust_list: {
        result = SYSCALL(get_robust_list, arg1, arg2, arg3);
        break;
    }
    case felix86_riscv64_landlock_create_ruleset: {
        result = SYSCALL(landlock_create_ruleset, arg1, arg2, arg3);
        break;
    }
    case felix86_riscv64_landlock_add_rule: {
        result = SYSCALL(landlock_add_rule, arg1, arg2, arg3, arg4);
        break;
    }
    case felix86_riscv64_landlock_restrict_self: {
        result = SYSCALL(landlock_restrict_self, arg1, arg2);
        break;
    }
    case felix86_riscv64_rseq: {
        // Couldn't find any solid documentation and FEX doesn't support it either
        result = -ENOSYS;
        break;
    }
    case felix86_riscv64_userfaultfd: {
        result = -ENOSYS;
        break;
    }
    case felix86_riscv64_futex_waitv: {
        result = SYSCALL(futex_waitv, arg1, arg2, arg3, arg4, arg5);
        break;
    }
    case felix86_riscv64_personality: {
        result = SYSCALL(personality, arg1 & ~PER_LINUX32);
        break;
    }
    case felix86_riscv64_process_vm_readv: {
        result = SYSCALL(process_vm_readv, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_process_vm_writev: {
        result = SYSCALL(process_vm_writev, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_prlimit64: {
        result = SYSCALL(prlimit64, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_getrandom: {
        result = SYSCALL(getrandom, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_mprotect: {
        result = SYSCALL(mprotect, arg1, arg2, arg3, arg4, arg5, arg6);
        u64 start = arg1;
        u64 size = arg2;
        int prot = arg3;
        if ((prot & PROT_WRITE) && result == 0) {
            // We mark pages as read-only when we recompile their code. But if the guest program
            // decides to override that by marking them as PROT_WRITE then we can no longer keep track
            // of them. So in that case, we will invalidate everything in that region to be safe.
            Recompiler::invalidateRangeGlobal(start, start + size);
        }
        break;
    }
    case felix86_riscv64_close: {
        result = FD::close(arg1);
        break;
    }
    case felix86_riscv64_close_range: {
        result = FD::close_range(arg1, arg2, arg3);
        break;
    }
    case felix86_riscv64_copy_file_range: {
        result = SYSCALL(copy_file_range, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_shutdown: {
        result = SYSCALL(shutdown, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_shmget: {
        result = SYSCALL(shmget, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_shmat: {
        result = SYSCALL(shmat, arg1, arg2, arg3);
        if (result >= 0 && ((u64)result) > mmap_min_addr() && result < UINT32_MAX) {
            WARN("shmat in 32-bit address space, this could cause problems with MAP_32BIT");
        }
        break;
    }
    case felix86_riscv64_shmctl: {
        result = SYSCALL(shmctl, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_shmdt: {
        if (arg1 > mmap_min_addr() && arg1 < UINT32_MAX) {
            WARN("shmdt in 32-bit address space, this could cause problems with MAP_32BIT");
        }
        result = SYSCALL(shmdt, arg1);
        break;
    }
    case felix86_riscv64_getsid: {
        result = SYSCALL(getsid, arg1);
        break;
    }
    case felix86_riscv64_bind: {
        // TODO: HACK: does this need porting to other socket related syscalls?
        auto sock = (sockaddr*)arg2;

        if (arg3 <= 2 || sock->sa_family != AF_UNIX || (sock->sa_family == AF_UNIX && sock->sa_data[0] == 0)) {
            result = SYSCALL(bind, arg1, arg2, arg3);
        } else {
            FdPath socket_path = Filesystem::resolve((char*)sock->sa_data, true);
            int len = strlen(socket_path.full_path());
            if (len >= 108) {
                WARN("AF_UNIX path overflowed: %s %d", socket_path.full_path(), len);
            }
            sockaddr_un addr = {0};
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, socket_path.full_path(), sizeof(addr.sun_path));
            size_t addrlen = offsetof(sockaddr_un, sun_path) + strlen(addr.sun_path) + 1;

            result = SYSCALL(bind, arg1, &addr, addrlen);
        }
        break;
    }
    case felix86_riscv64_setpgid: {
        result = SYSCALL(setpgid, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_setpriority: {
        result = SYSCALL(setpriority, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_getpriority: {
        result = SYSCALL(getpriority, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_getrusage: {
        result = SYSCALL(getrusage, arg1, arg2);
        break;
    }
    case felix86_riscv64_getcwd: {
        SignalGuard guard;
        result = Filesystem::Getcwd((char*)arg1, arg2);
        break;
    }
    case felix86_riscv64_symlinkat: {
        SignalGuard guard;
        result = Filesystem::SymlinkAt((char*)arg1, arg2, (char*)arg3);
        break;
    }
    case felix86_riscv64_renameat2: {
        SignalGuard guard;
        result = Filesystem::RenameAt2(arg1, (char*)arg2, arg3, (char*)arg4, arg5);
        break;
    }
    case felix86_riscv64_io_uring_enter: {
        result = SYSCALL(io_uring_enter, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_io_uring_register: {
        result = SYSCALL(io_uring_register, arg1, arg2, arg3, arg4);
        break;
    }
    case felix86_riscv64_io_uring_setup: {
        result = SYSCALL(io_uring_setup, arg1, arg2);
        break;
    }
    case felix86_riscv64_epoll_ctl: {
        if (arg4) {
            epoll_event host_event = *(x86_epoll_event*)arg4;
            result = SYSCALL(epoll_ctl, arg1, arg2, arg3, &host_event);
            if (result == 0) {
                *(x86_epoll_event*)arg4 = host_event;
            }
        } else {
            result = SYSCALL(epoll_ctl, arg1, arg2, arg3, nullptr);
        }
        break;
    }
    case felix86_riscv64_epoll_pwait: {
        epoll_event* host_events = (epoll_event*)alloca(std::max(0, (int)arg3) * sizeof(epoll_event));
        result = SYSCALL(epoll_pwait, arg1, host_events, arg3, arg4, arg5, arg6);
        if (result >= 0) {
            x86_epoll_event* guest_event = (x86_epoll_event*)arg2;
            for (int i = 0; i < result; i++) {
                guest_event[i] = host_events[i];
            }
        }
        break;
    }
    case felix86_riscv64_epoll_pwait2: {
        epoll_event* host_events = (epoll_event*)alloca(std::max(0, (int)arg3) * sizeof(epoll_event));
        result = SYSCALL(epoll_pwait2, arg1, host_events, arg3, arg4, arg5, arg6);
        if (result >= 0) {
            x86_epoll_event* guest_event = (x86_epoll_event*)arg2;
            for (int i = 0; i < result; i++) {
                guest_event[i] = host_events[i];
            }
        }
        break;
    }
    case felix86_riscv64_mount: {
        SignalGuard guard;
        result = Filesystem::Mount((char*)arg1, (char*)arg2, (char*)arg3, arg4, (void*)arg5);
        break;
    }
    case felix86_riscv64_chroot: {
        SignalGuard guard;
        result = Filesystem::Chroot((char*)arg1);
        break;
    }
    case felix86_riscv64_pivot_root: {
        SignalGuard guard;
        result = Filesystem::PivotRoot((char*)arg1, (char*)arg2);
        break;
    }
    case felix86_riscv64_unshare: {
        result = Threads::Unshare(arg1);
        break;
    }
    case felix86_riscv64_accept: {
        result = SYSCALL(accept, arg1, arg2, arg3);
        break;
    }
    case felix86_riscv64_accept4: {
        result = SYSCALL(accept4, arg1, arg2, arg3, arg4);
        break;
    }
    case felix86_riscv64_socketpair: {
        result = SYSCALL(socketpair, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_setgid: {
        result = SYSCALL(setgid, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_setsid: {
        result = SYSCALL(setsid, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_setreuid: {
        result = SYSCALL(setreuid, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_capset: {
        result = SYSCALL(capset, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_capget: {
        result = SYSCALL(capget, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_setresuid: {
        result = SYSCALL(setresuid, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_setregid: {
        result = SYSCALL(setregid, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_setresgid: {
        result = SYSCALL(setresgid, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_setgroups: {
        result = SYSCALL(setgroups, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_getgroups: {
        result = SYSCALL(getgroups, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_setuid: {
        result = SYSCALL(setuid, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_umount2: {
        SignalGuard guard;
        result = Filesystem::Umount((char*)arg1, arg2);
        break;
    }
    case felix86_riscv64_sched_getscheduler: {
        result = SYSCALL(sched_getscheduler, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_sched_getparam: {
        result = SYSCALL(sched_getparam, arg1, arg2);
        break;
    }
    case felix86_riscv64_sched_setparam: {
        result = SYSCALL(sched_setparam, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_clock_gettime: {
        result = SYSCALL(clock_gettime, arg1, arg2);
        break;
    }
    case felix86_riscv64_clock_getres: {
        result = SYSCALL(clock_getres, arg1, arg2);
        break;
    }
    case felix86_riscv64_getresuid: {
        result = SYSCALL(getresuid, arg1, arg2, arg3);
        break;
    }
    case felix86_riscv64_getresgid: {
        result = SYSCALL(getresgid, arg1, arg2, arg3);
        break;
    }
    case felix86_riscv64_gettimeofday: {
        result = SYSCALL(gettimeofday, arg1, arg2);
        break;
    }
    case felix86_riscv64_dup: {
        result = SYSCALL(dup, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_dup3: {
        result = FD::dup3(arg1, arg2, arg3);
        break;
    }
    case felix86_riscv64_fstat: {
        x86_stat* guest_stat = (x86_stat*)arg2;
        struct stat host_stat;
        result = SYSCALL(fstat, arg1, &host_stat);
        if (result >= 0) {
            *guest_stat = host_stat;
        }
        break;
    }
    case felix86_riscv64_fsync: {
        result = SYSCALL(fsync, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_sync: {
        result = SYSCALL(sync);
        break;
    }
    case felix86_riscv64_syncfs: {
        result = SYSCALL(syncfs, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_sendfile: {
        result = SYSCALL(sendfile, arg1, arg2, arg3, arg4);
        break;
    }
    case felix86_riscv64_sendmmsg: {
        result = SYSCALL(sendmmsg, arg1, arg2, arg3, arg4);
        break;
    }
    case felix86_riscv64_recvmmsg: {
        result = SYSCALL(recvmmsg, arg1, arg2, arg3, arg4, arg5);
        break;
    }
    case felix86_riscv64_setsockopt: {
        result = SYSCALL(setsockopt, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_getsockopt: {
        result = SYSCALL(getsockopt, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_statx: {
        SignalGuard guard;
        result = Filesystem::Statx((int)arg1, (char*)arg2, (int)arg3, (u32)arg4, (struct statx*)arg5);
        break;
    }
    case felix86_riscv64_fadvise64: {
        result = SYSCALL(fadvise64, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_fcntl: {
        result = SYSCALL(fcntl, arg1, arg2, arg3);
        break;
    }
    case felix86_riscv64_pselect6: {
        result = SYSCALL(pselect6, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_chdir: {
        SignalGuard guard;
        result = Filesystem::Chdir((char*)arg1);
        break;
    }
    case felix86_riscv64_fchown: {
        result = SYSCALL(fchown, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_unlinkat: {
        SignalGuard guard;
        result = Filesystem::UnlinkAt((int)arg1, (char*)arg2, (int)arg3);
        break;
    }
    case felix86_riscv64_fchdir: {
        result = SYSCALL(fchdir, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_newfstatat: {
        SignalGuard guard;
        struct stat stat;
        result = Filesystem::FStatAt((int)arg1, (char*)arg2, &stat, (int)arg4);
        if (result >= 0) {
            *(x86_stat*)arg3 = stat;
        }
        break;
    }
    case felix86_riscv64_sysinfo: {
        result = SYSCALL(sysinfo, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_ioctl: {
        result = SYSCALL(ioctl, arg1, arg2, arg3, arg4, arg5, arg6);

        if (!try_strace_ioctl(arg1, arg2, arg3, result)) {
        }
        break;
    }
    case felix86_riscv64_write: {
        result = SYSCALL(write, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_writev: {
        result = SYSCALL(writev, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_exit_group: {
        state->exit_reason = EXIT_REASON_EXIT_GROUP_SYSCALL;
        state->exit_code = arg1;
        if (g_config.calltrace_on_exit) {
            dump_states();
        }
        Emulator::ExitDispatcher(frame);
        UNREACHABLE();
        break;
    }
    case felix86_riscv64_faccessat:
        arg4 = 0; // faccessat doesn't take flags while faccessat2 does
        [[fallthrough]];
    case felix86_riscv64_faccessat2: {
        SignalGuard guard;
        result = Filesystem::FAccessAt((int)arg1, (char*)arg2, (int)arg3, (int)arg4);
        break;
    }
    case felix86_riscv64_pipe2: {
        result = SYSCALL(pipe2, arg1, arg2);
        break;
    }
    case felix86_riscv64_memfd_create: {
        result = SYSCALL(memfd_create, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_ftruncate: {
        result = SYSCALL(ftruncate, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_read: {
        result = SYSCALL(read, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_getdents64: {
        result = SYSCALL(getdents64, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_lgetxattr: {
        SignalGuard guard;
        result = Filesystem::LGetXAttr((char*)arg1, (char*)arg2, (void*)arg3, arg4);
        break;
    }
    case felix86_riscv64_getxattr: {
        SignalGuard guard;
        result = Filesystem::GetXAttr((char*)arg1, (char*)arg2, (void*)arg3, arg4);
        break;
    }
    case felix86_riscv64_fgetxattr: {
        result = SYSCALL(fgetxattr, arg1, arg2, arg3, arg4);
        break;
    }
    case felix86_riscv64_setxattr: {
        SignalGuard guard;
        result = Filesystem::SetXAttr((char*)arg1, (char*)arg2, (void*)arg3, arg4, arg5);
        break;
    }
    case felix86_riscv64_lsetxattr: {
        SignalGuard guard;
        result = Filesystem::LSetXAttr((char*)arg1, (char*)arg2, (void*)arg3, arg4, arg5);
        break;
    }
    case felix86_riscv64_fsetxattr: {
        result = SYSCALL(fsetxattr, arg1, arg2, arg3, arg4, arg5);
        break;
    }
    case felix86_riscv64_removexattr: {
        SignalGuard guard;
        result = Filesystem::RemoveXAttr((char*)arg1, (char*)arg2);
        break;
    }
    case felix86_riscv64_lremovexattr: {
        SignalGuard guard;
        result = Filesystem::LRemoveXAttr((char*)arg1, (char*)arg2);
        break;
    }
    case felix86_riscv64_fremovexattr: {
        result = SYSCALL(fremovexattr, arg1, arg2);
        break;
    }
    case felix86_riscv64_pwrite64: {
        result = SYSCALL(pwrite64, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_pread64: {
        result = SYSCALL(pread64, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_signalfd4: {
        result = SYSCALL(signalfd4, arg1, arg2, arg3, arg4);
        break;
    }
    case felix86_riscv64_openat: {
        SignalGuard guard;
        result = g_fs->OpenAt((int)arg1, (char*)arg2, (int)arg3, arg4);
        break;
    }
    case felix86_riscv64_tgkill: {
        if (arg3 != 0) { // sig==0 is used to check if process exists
            SIGLOG("Calling tgkill with sig: %d for TGID: %d and TID: %d", arg3, arg1, arg2);
        }
        result = SYSCALL(tgkill, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_tkill: {
        if (arg2 != 0) {
            SIGLOG("Calling tkill with sig: %d for TID: %d", arg2, arg1);
        }
        result = SYSCALL(tkill, arg1, arg2);
        break;
    }
    case felix86_riscv64_kill: {
        if (arg2 != 0) {
            SIGLOG("Calling kill with sig: %d and PID: %d", arg2, arg1);
        }
        result = SYSCALL(kill, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_mmap: {
#ifndef MAP_32BIT
#define MAP_32BIT 0x40
#endif
        SignalGuard guard;
        u64 flags = arg4;
        bool is_fixed = (flags & MAP_FIXED) || (flags & MAP_FIXED_NOREPLACE);
        if ((flags & MAP_32BIT) || (is_fixed && arg1 < UINT32_MAX) || g_mode32) {
            // The MAP_32BIT flag is x86 only so we need to emulate it
            // For example, Mono tries to use it to allocate code cache pages near the executable so that it can use
            // +-2GiB jumps. If it doesn't get them near enough it will eventually crash and die.
            // We need to also track fixed mappings in the 32-bit address space
            result = (ssize_t)g_mapper->map32((void*)arg1, arg2, arg3, (int)arg4, (int)arg5, arg6);
        } else {
            // No need to use mapper
            result = SYSCALL(mmap, arg1, arg2, arg3, (int)arg4, (int)arg5, arg6);
        }

        // If there's any blocks in any threads that match this mmapped range they need to be invalidated
        if (result > 0) {
            Recompiler::invalidateRangeGlobal(result, result + arg2);

            if ((int)arg5 != -1) {
                // uses file descriptor, mmaps file to memory, may need to update mappings
                // this can occur when using something like dlopen or when the interpreter initially loads the symbols
                g_symbols_cached = false;
            }
        }
        break;
    }
    case felix86_riscv64_munmap: {
        if (arg1 < UINT32_MAX || g_mode32) {
            // Track unmaps in the 32-bit address space for MAP_32BIT in 64-bit mode
            SignalGuard guard;
            result = g_mapper->unmap32((void*)arg1, arg2);
        } else {
            result = SYSCALL(munmap, arg1, arg2, arg3, arg4, arg5, arg6);
        }

        if (result == 0) {
            Recompiler::invalidateRangeGlobal(arg1, arg1 + arg2);
            g_symbols_cached = false;
        }
        break;
    }
    case felix86_riscv64_setitimer: {
        result = SYSCALL(setitimer, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_timer_create: {
        result = SYSCALL(timer_create, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_timer_gettime: {
        result = SYSCALL(timer_gettime, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_timer_settime: {
        result = SYSCALL(timer_settime, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_timer_getoverrun: {
        result = SYSCALL(timer_getoverrun, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_timer_delete: {
        result = SYSCALL(timer_delete, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_getuid: {
        result = SYSCALL(getuid);
        break;
    }
    case felix86_riscv64_fdatasync: {
        result = SYSCALL(fdatasync, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_geteuid: {
        result = SYSCALL(geteuid);
        break;
    }
    case felix86_riscv64_getegid: {
        result = SYSCALL(getegid);
        break;
    }
    case felix86_riscv64_utimensat: {
        SignalGuard guard;
        result = Filesystem::UtimensAt(arg1, (const char*)arg2, (struct timespec*)arg3, arg4);
        break;
    }
    case felix86_riscv64_getgid: {
        result = SYSCALL(getgid);
        break;
    }
    case felix86_riscv64_setfsgid: {
        result = SYSCALL(setfsgid, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_setfsuid: {
        result = SYSCALL(setfsuid, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_getppid: {
        result = SYSCALL(getppid);
        break;
    }
    case felix86_riscv64_getpid: {
        result = SYSCALL(getpid);
        break;
    }
    case felix86_riscv64_getpgid: {
        result = SYSCALL(getpgid, arg1);
        break;
    }
    case felix86_riscv64_gettid: {
        result = SYSCALL(gettid);
        break;
    }
    case felix86_riscv64_seccomp: {
        WARN("This app uses seccomp");
        // We don't implement this... yet
        result = -EINVAL;
        break;
    }
    case felix86_riscv64_socket: {
        result = SYSCALL(socket, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_connect: {
        // TODO: HACK: port this to 32-bit socket syscalls too
        auto sock = (sockaddr*)arg2;

        if (arg3 <= 2 || sock->sa_family != AF_UNIX || (sock->sa_family == AF_UNIX && sock->sa_data[0] == 0)) {
            result = SYSCALL(connect, arg1, arg2, arg3, arg4, arg5, arg6);
        } else {
            FdPath socket_path = Filesystem::resolve((char*)sock->sa_data, true);
            int len = strlen(socket_path.full_path());
            if (len >= 108) {
                WARN("AF_UNIX path overflowed: %s %d", socket_path.full_path(), len);
            }
            sockaddr_un addr = {0};
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, socket_path.full_path(), sizeof(addr.sun_path));
            size_t addrlen = offsetof(sockaddr_un, sun_path) + strlen(addr.sun_path) + 1;

            result = SYSCALL(connect, arg1, &addr, addrlen);
        }
        break;
    }
    case felix86_riscv64_mremap: {
        SignalGuard guard;
        result = (u64)g_mapper->remap((void*)arg1, arg2, arg3, arg4, (void*)arg5);
        if (result > 0) {
            Recompiler::invalidateRangeGlobal(arg1, arg1 + arg2);
            Recompiler::invalidateRangeGlobal(result, result + arg3);
        }
        break;
    }
    case felix86_riscv64_msync: {
        result = SYSCALL(msync, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_sendto: {
        result = SYSCALL(sendto, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_times: {
        result = SYSCALL(times, arg1);
        break;
    }
    case felix86_riscv64_recvfrom: {
        result = SYSCALL(recvfrom, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_lseek: {
        result = SYSCALL(lseek, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_getcpu: {
        result = SYSCALL(getcpu, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_nanosleep: {
        result = SYSCALL(nanosleep, arg1, arg2);
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
        std::string version = "#1 SMP " __DATE__ " " __TIME__;
        strcpy(guest_uname->version, version.c_str());
        strcpy(guest_uname->machine, (state->persona & PER_LINUX32) ? "i686" : "x86_64");
        ASSERT(!(state->persona & UNAME26));
        result = 0;
        break;
    }
    case felix86_riscv64_listxattr: {
        SignalGuard guard;
        result = Filesystem::Listxattr((char*)arg1, (char*)arg2, arg3, false);
        break;
    }
    case felix86_riscv64_llistxattr: {
        SignalGuard guard;
        result = Filesystem::Listxattr((char*)arg1, (char*)arg2, arg3, true);
        break;
    }
    case felix86_riscv64_flistxattr: {
        result = SYSCALL(flistxattr, arg1, arg2, arg3);
        break;
    }
    case felix86_riscv64_timerfd_create: {
        result = SYSCALL(timerfd_create, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_timerfd_settime: {
        result = SYSCALL(timerfd_settime, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_timerfd_gettime: {
        result = SYSCALL(timerfd_gettime, arg1, arg2);
        break;
    }
    case felix86_riscv64_statfs: {
        SignalGuard guard;
        result = Filesystem::StatFs((char*)arg1, (struct statfs*)arg2);
        break;
    }
    case felix86_riscv64_fstatfs: {
        result = SYSCALL(fstatfs, arg1, arg2);
        break;
    }
    case felix86_riscv64_getsockname: {
        result = SYSCALL(getsockname, arg1, arg2, arg3);
        break;
    }
    case felix86_riscv64_madvise: {
        result = SYSCALL(madvise, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_exit: {
        state->exit_reason = ExitReason::EXIT_REASON_EXIT_SYSCALL;
        state->exit_code = arg1;
        if (g_config.calltrace_on_exit) {
            dump_states();
        }
        Emulator::ExitDispatcher(frame);
        UNREACHABLE();
        break;
    }
    case felix86_riscv64_eventfd2: {
        result = SYSCALL(eventfd2, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_fchmod: {
        result = SYSCALL(fchmod, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_fchmodat: {
        SignalGuard guard;
        result = Filesystem::FChmodAt((int)arg1, (char*)arg2, arg3);
        break;
    }
    case felix86_riscv64_fchmodat2: {
        result = -ENOSYS; // TODO: support me in newer kernel versions (>= 6.6)
        break;
    }
    case felix86_riscv64_recvmsg: {
        result = SYSCALL(recvmsg, arg1, arg2, arg3);
        break;
    }
    case felix86_riscv64_sendmsg: {
        result = SYSCALL(sendmsg, arg1, arg2, arg3);
        break;
    }
    case felix86_riscv64_semget: {
        result = SYSCALL(semget, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_semop: {
        result = SYSCALL(semop, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_semtimedop: {
        result = SYSCALL(semtimedop, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_semctl: {
        x64_semid64_ds* guest_semi = (x64_semid64_ds*)arg4;
        switch (arg3) {
        case IPC_SET: {
            ASSERT(guest_semi);
            struct semid64_ds host_semi{};
            host_semi = *guest_semi;
            result = SYSCALL(semctl, arg1, arg2, arg3, &host_semi);
            if (result == 0) {
                *guest_semi = host_semi;
            }
            break;
        }
        case SEM_STAT:
        case SEM_STAT_ANY:
        case IPC_STAT: {
            struct semid64_ds host_semi{};
            result = SYSCALL(semctl, arg1, arg2, arg3, &host_semi);
            if (result == 0) {
                ASSERT(guest_semi);
                *guest_semi = host_semi;
            }
            break;
        }
        case SEM_INFO:
        case IPC_INFO:
        case IPC_RMID:
        case GETPID:
        case GETNCNT:
        case GETZCNT:
        case GETVAL:
        case GETALL:
        case SETALL:
        case SETVAL: {
            result = SYSCALL(semctl, arg1, arg2, arg3, arg4, arg5, arg6);
            break;
        }
        default: {
            UNREACHABLE();
            break;
        }
        }
        break;
    }
    case felix86_riscv64_flock: {
        result = SYSCALL(flock, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_clock_nanosleep: {
        result = SYSCALL(clock_nanosleep, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_rt_sigaction: {
        if (arg1 > 64) {
            result = -EINVAL;
            break;
        }

        x64_sigaction* act = (x64_sigaction*)arg2;
        if (act && (arg1 == SIGKILL || arg1 == SIGSTOP)) {
            result = -EINVAL;
            break;
        }

        RegisteredSignal old = Signals::getSignalHandler(state, arg1);
        if (act) {
            auto handler = act->handler;
            Signals::registerSignalHandler(state, arg1, (u64)handler, act->sa_mask, act->sa_flags, act->restorer);
#if 0
            PLAIN("Installed signal handler %s at:", strsignal(arg1));
            print_address((u64)handler);
            PLAIN("Flags: %lx\n", act->sa_flags);
#endif
        }

        x64_sigaction* old_act = (x64_sigaction*)arg3;
        if (old_act) {
            old_act->handler = old.func;
            old_act->sa_flags = old.flags;
            old_act->sa_mask = old.mask;
            old_act->restorer = old.restorer;
        }

        result = 0;
        break;
    }
    case felix86_riscv64_rt_sigtimedwait: {
        result = SYSCALL(rt_sigtimedwait, arg1, arg2, arg3, arg4, arg5, arg6);
        WARN_ONCE("This program uses rt_sigtimedwait");
        break;
    }
    case felix86_riscv64_sched_yield: {
        result = SYSCALL(sched_yield);
        break;
    }
    case felix86_riscv64_get_mempolicy: {
        result = SYSCALL(get_mempolicy, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_set_mempolicy: {
        result = SYSCALL(set_mempolicy, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_membarrier: {
        result = SYSCALL(membarrier, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_mknodat: {
        SignalGuard guard;
        result = Filesystem::MknodAt(arg1, (const char*)arg2, arg3, arg4);
        break;
    }
    case felix86_riscv64_sigaltstack: {
        VERBOSE("----- sigaltstack was called -----");
        stack_t* new_ss = (stack_t*)arg1;
        stack_t* old_ss = (stack_t*)arg2;
        u64 current_rsp = state->gprs[X86_REF_RSP];

        bool on_stack = false;
        if (!(state->alt_stack.ss_flags & SS_DISABLE) && current_rsp >= (u64)state->alt_stack.ss_sp &&
            current_rsp < (u64)state->alt_stack.ss_sp + state->alt_stack.ss_size) {
            on_stack = true;
        }

        if (old_ss) {
            old_ss->ss_sp = state->alt_stack.ss_sp;
            old_ss->ss_flags = 0;
            old_ss->ss_size = state->alt_stack.ss_size;

            if (on_stack) {
                old_ss->ss_flags = SS_ONSTACK;
            } else {
                old_ss->ss_flags = SS_DISABLE;
            }
        }

        if (new_ss) {
            if (on_stack) {
                WARN("Tried to set sigaltstack while using it");
                result = -EPERM;
                break;
            }

            state->alt_stack.ss_sp = new_ss->ss_sp;
            state->alt_stack.ss_flags = new_ss->ss_flags;
            state->alt_stack.ss_size = new_ss->ss_size;
            VERBOSE("New altstack: %lx", new_ss->ss_sp);
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
                memcpy(addr, (void*)g_guest_auxv, actual_size);
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
            result = SYSCALL(prctl, arg1, arg2, arg3, arg4, arg5, arg6);
            break;
        }
        }
        break;
    }
    case felix86_riscv64_futex: {
        result = SYSCALL(futex, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_inotify_init1: {
        result = SYSCALL(inotify_init1, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_inotify_add_watch: {
        SignalGuard guard;
        result = Filesystem::INotifyAddWatch(arg1, (char*)arg2, arg3);
        break;
    }
    case felix86_riscv64_inotify_rm_watch: {
        result = SYSCALL(inotify_rm_watch, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_fallocate: {
        result = SYSCALL(fallocate, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_sched_getaffinity: {
        result = SYSCALL(sched_getaffinity, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_sched_getattr: {
        result = SYSCALL(sched_getattr, arg1, arg2, arg3, arg4);
        break;
    }
    case felix86_riscv64_sched_setattr: {
        result = SYSCALL(sched_getattr, arg1, arg2, arg3);
        break;
    }
    case felix86_riscv64_sched_setaffinity: {
        result = SYSCALL(sched_setaffinity, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_sched_get_priority_min: {
        result = SYSCALL(sched_get_priority_min, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_sched_get_priority_max: {
        result = SYSCALL(sched_get_priority_max, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_sched_setscheduler: {
        result = SYSCALL(sched_setscheduler, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_mincore: {
        result = SYSCALL(mincore, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_listen: {
        result = SYSCALL(listen, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_clone3: {
        result = -ENOSYS; // don't support these for now
        break;
    }
    case felix86_riscv64_clone: {
        u64 child_tid = arg4;
        u64 parent_tid = arg3;
        u64 guest_flags = arg1;
        CloneArgs args{
            .parent_state = state,
            .guest_flags = guest_flags,
            .parent_tid = (pid_t*)parent_tid,
            .child_tid = (pid_t*)child_tid,
            .new_tls = arg5,
            .new_rsp = arg2,
            .new_rip = state->gprs[X86_REF_RCX],
            .new_thread = 0,
            .new_tid = 0,
        };
        result = Threads::Clone(state, &args);
        break;
    }
    case felix86_riscv64_wait4: {
        result = SYSCALL(wait4, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_waitid: {
        result = SYSCALL(waitid, arg1, arg2, arg3, arg4, arg5);
        break;
    }
    case felix86_riscv64_fchownat: {
        result = SYSCALL(fchownat, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_sync_file_range: {
        result = SYSCALL(sync_file_range, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_mlock: {
        result = SYSCALL(mlock, arg1, arg2);
        break;
    }
    case felix86_riscv64_munlock: {
        result = SYSCALL(munlock, arg1, arg2);
        break;
    }
    case felix86_riscv64_mlock2: {
        result = SYSCALL(mlock2, arg1, arg2, arg3);
        break;
    }
    case felix86_riscv64_mlockall: {
        result = SYSCALL(mlockall, arg1);
        break;
    }
    case felix86_riscv64_munlockall: {
        result = SYSCALL(munlockall);
        break;
    }
    case felix86_riscv64_mkdirat: {
        SignalGuard guard;
        result = Filesystem::MkdirAt(arg1, (char*)arg2, arg3);
        break;
    }
    case felix86_riscv64_execve: {
        if (!arg1) {
            WARN("execve with nullptr as executable path?");
            result = -EINVAL;
            break;
        }

        SignalGuard guard;
        FdPath fd_path = Filesystem::resolve((char*)arg1, true);

        if (!fd_path.path() || !std::filesystem::exists(fd_path.full_path())) {
            WARN("Execve couldn't find path: %s", arg1);
            result = -ENOENT;
            break;
        }

        std::filesystem::path path = fd_path.full_path();
        if (!std::filesystem::is_regular_file(path)) {
            WARN("Not regular file during execve: %s", path.c_str());
            result = -ENOENT;
            break;
        }

        std::vector<const char*> argv;
        std::vector<const char*> envp;
        std::string script_interpreter;
        std::vector<std::string> script_args;

        // This is going to be the first argument in execve
        // Depending on if binfmt_misc is installed or not, it will be either the guest
        // executable or the emulator
        std::filesystem::path executable;

        bool is_script = Script::Peek(path) == Script::PeekResult::Script;
        if (!g_config.binfmt_misc_installed) {
            // If binfmt_misc is not installed, push the emulator at the start of the arguments
            executable = g_emulator_path;
            argv.push_back(executable.c_str());

            if (check_if_privileged_executable(path)) {
                ASSERT_MSG(!is_script, "Script with setuid...?");

                // If this is a privileged executable, it won't work out if we don't have binfmt_misc support
                // Because binfmt_misc would see that the binary has extra permissions and give the emulator
                // those permissions as well. When we run it through the emulator manually however, we can't
                // do the same. So warn that this might end badly.
                WARN("About to run privileged executable %s, but there's no binfmt_misc support, so things may go wrong. Please enable "
                     "binfmt_misc support by running `felix86 -b` and disabling it for any other x86/x86-64 emulators");
            }
        } else {
            executable = path;
        }

        // Don't push the emulator, push just the executable and binfmt_misc will figure it out
        // If it's a script we need to push the interpreter too
        if (is_script) {
            Script script(path);
            const std::string& args = script.GetArgs();
            script_interpreter = script.GetInterpreter();
            FdPath interpreter_fd_path = Filesystem::resolve(script_interpreter.c_str(), true);
            ASSERT(interpreter_fd_path.full_path());
            ASSERT(interpreter_fd_path.full_path()[0] == '/');
            script_interpreter = interpreter_fd_path.full_path();
            script_args = split_string(args, ' ');
            argv.push_back(script_interpreter.c_str());
            for (auto it = script_args.begin(); it < script_args.end(); it++) {
                if (it->empty())
                    continue;

                argv.push_back(it->c_str());
            }

            if (g_config.binfmt_misc_installed) {
                executable = script_interpreter;
            } else {
                // Retain emulator path
            }

            path = std::filesystem::absolute(path);
            ASSERT_MSG(path.string().find(g_config.rootfs_path.string()) == 0, "Script path is not inside rootfs? %s", path.c_str());

            // We are running it through emulated bash, so the script itself needs to be a regular path
            path = path.string().substr(g_config.rootfs_path.string().size());
        }

        argv.push_back(path.c_str());

        if (arg2) {
            u8* guest_argv = (u8*)arg2;
            guest_argv += g_mode32 ? 4 : 8;
            while (true) {
                u64 ptr = 0;
                memcpy(&ptr, guest_argv, g_mode32 ? 4 : 8);
                if (ptr == 0) {
                    break;
                }

                argv.push_back((const char*)ptr);
                guest_argv += g_mode32 ? 4 : 8;
            }
        } else {
            WARN("argv null during execve...?");
        }
        argv.push_back(nullptr);

        char** host_environ = environ;
        while (*host_environ) {
            std::string env = *host_environ;
            if (env.find("__FELIX86") == std::string::npos) {
                envp.push_back(*host_environ);
            }
            host_environ++;
        }

        std::string argv0_original = std::string("__FELIX86_ARGV0=") + (char*)arg1;
        std::string guest_envs = "__FELIX86_GUEST_ENVS=";
        if (arg3) {
            u8* guest_envp = (u8*)arg3;
            while (true) {
                u64 ptr = 0;
                memcpy(&ptr, guest_envp, g_mode32 ? 4 : 8);
                if (ptr == 0) {
                    break;
                }

                guest_envs += (const char*)ptr;
                guest_envs += ",";

                guest_envp += g_mode32 ? 4 : 8;
            }

            if (!guest_envs.empty()) {
                guest_envs.pop_back();
            }

            envp.push_back(guest_envs.c_str());
        } else {
            WARN("envp null during execve...?");
        }

        // We need to tell the new process where the server is
        std::string log_env = std::string("__FELIX86_PIPE=") + Logger::getPipeName();
        envp.push_back("__FELIX86_EXECVE=1");
        envp.push_back(argv0_original.c_str());
        envp.push_back(log_env.c_str());
        std::string rootfs_env = std::string("__FELIX86_ROOTFS=") + g_config.rootfs_path.string();
        envp.push_back(rootfs_env.c_str());
        std::string mounts_env = std::string("__FELIX86_MOUNTS=") + g_mounts_path.string();
        envp.push_back(mounts_env.c_str());
        size_t current_mount = 0;
        for (auto& mount_path : g_process_globals.mount_paths) {
            envp.push_back(strdup((std::string("__FELIX86_MOUNT_") + std::to_string(current_mount++) + "=" + mount_path.string()).c_str()));
        }
        envp.push_back(nullptr);

        std::string args = "";
        for (auto arg : argv) {
            args += " ";
            args += arg ? arg : "";
        }

        LOG("Running execve, wish me luck:%s", args.c_str());

        // Undo signal guard so the child doesn't inherit the bad mask
        guard.kill();

        syscall(SYS_execve, executable.c_str(), &argv[0], envp.data());

        ASSERT_MSG(false, "Error during execve: %s", strerror(errno));
        break;
    }
    case felix86_riscv64_umask: {
        result = SYSCALL(umask, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_ptrace: {
        LOG("Tried to run ptrace, returning -EPERM");
        result = -EPERM;
        break;
    }
    case felix86_riscv64_ppoll: {
        result = SYSCALL(ppoll, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_linkat: {
        SignalGuard guard;
        result = Filesystem::LinkAt((int)arg1, (char*)arg2, (int)arg3, (char*)arg4, (int)arg5);
        break;
    }
    case felix86_riscv64_readlinkat: {
        if (arg2 == arg3) {
            WARN("arg2 == arg3 during readlinkat");
        }
        SignalGuard guard;
        result = Filesystem::ReadlinkAt((int)arg1, (char*)arg2, (char*)arg3, (int)arg4);
        break;
    }
    case felix86_riscv64_getpeername: {
        result = SYSCALL(getpeername, arg1, arg2, arg3);
        break;
    }
    case felix86_riscv64_rt_sigsuspend: {
        result = Signals::sigsuspend(state, (sigset_t*)arg1);
        break;
    }
    case felix86_riscv64_rt_sigpending: {
        WARN("rt_sigpending");
        result = SYSCALL(rt_sigpending, arg1, arg2);
        break;
    }
    case felix86_riscv64_rt_sigreturn: {
        Signals::sigreturn(frame->state);
        Emulator::ExitDispatcher(frame);
        UNREACHABLE();
        break;
    }
    case felix86_riscv64_epoll_create1: {
        result = SYSCALL(epoll_create1, arg1);
        break;
    }
    case felix86_riscv64_rt_sigprocmask: {
        result = Signals::sigprocmask(state, arg1, (sigset_t*)arg2, (sigset_t*)arg3);
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

void felix86_syscall(felix86_frame* frame) {
    ASSERT(frame->magic == felix86_frame::expected_magic);
    ThreadState* state = frame->state;
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
        result = felix86_syscall_common(frame, rv_syscall, arg1, arg2, arg3, arg4, arg5, arg6);
    } else {
        switch (syscall_number) {
        case felix86_x86_64_time: {
            result = ::time((time_t*)arg1);
            break;
        }
        case felix86_x86_64_inotify_init: {
            result = SYSCALL(inotify_init1, 0);
            break;
        }
        case felix86_x86_64_link: {
            SignalGuard guard;
            result = Filesystem::LinkAt(AT_FDCWD, (char*)arg1, AT_FDCWD, (char*)arg2, 0);
            break;
        }
        case felix86_x86_64_readlink: {
            if (arg1 == arg2) {
                WARN("arg1 == arg2 during readlink");
            }
            SignalGuard guard;
            result = Filesystem::ReadlinkAt(AT_FDCWD, (char*)arg1, (char*)arg2, (int)arg3);
            break;
        }
        case felix86_x86_64_getpgrp: {
            result = getpgrp();
            break;
        }
        case felix86_x86_64_rename: {
            SignalGuard guard;
            result = Filesystem::RenameAt2(AT_FDCWD, (char*)arg1, AT_FDCWD, (char*)arg2, 0);
            break;
        }
        case felix86_x86_64_epoll_create: {
            // epoll_create has obsolete and ignored argument size, acts the same as epoll_create1 with flags=0
            result = SYSCALL(epoll_create1, 0);
            break;
        }
        case felix86_x86_64_epoll_wait: {
            epoll_event* host_events = (epoll_event*)alloca(std::max(0, (int)arg3) * sizeof(epoll_event));
            result = epoll_wait((int)arg1, host_events, (int)arg3, (int)arg4);
            if (result >= 0) {
                x86_epoll_event* guest_event = (x86_epoll_event*)arg2;
                for (int i = 0; i < result; i++) {
                    guest_event[i] = host_events[i];
                }
            }
            break;
        }
        case felix86_x86_64_chmod: {
            SignalGuard guard;
            result = Filesystem::Chmod((char*)arg1, arg2);
            break;
        }
        case felix86_x86_64_creat: {
            SignalGuard guard;
            result = Filesystem::Creat((char*)arg1, arg2);
            break;
        }
        case felix86_x86_64_symlink: {
            SignalGuard guard;
            result = Filesystem::SymlinkAt((char*)arg1, AT_FDCWD, (char*)arg2);
            break;
        }
        case felix86_x86_64_renameat: {
            SignalGuard guard;
            result = Filesystem::RenameAt2(arg1, (char*)arg2, arg3, (char*)arg4, 0);
            break;
        }
        case felix86_x86_64_poll: {
            result = poll((struct pollfd*)arg1, arg2, arg3);
            break;
        }
        case felix86_x86_64_dup2: {
            result = FD::dup2(arg1, arg2);
            break;
        }
        case felix86_x86_64_lstat: {
            SignalGuard guard;
            struct stat stat;
            result = Filesystem::FStatAt(AT_FDCWD, (char*)arg1, &stat, AT_SYMLINK_NOFOLLOW);
            if (result >= 0) {
                *(x86_stat*)arg2 = stat;
            }
            break;
        }
        case felix86_x86_64_chown: {
            SignalGuard guard;
            result = Filesystem::Chown((char*)arg1, arg2, arg3);
            break;
        }
        case felix86_x86_64_lchown: {
            SignalGuard guard;
            result = Filesystem::LChown((char*)arg1, arg2, arg3);
            break;
        }
        case felix86_x86_64_access: {
            SignalGuard guard;
            result = Filesystem::FAccessAt(AT_FDCWD, (char*)arg1, (int)arg2, 0);
            break;
        }
        case felix86_x86_64_pipe: {
            result = ::pipe((int*)arg1);
            break;
        }
        case felix86_x86_64_mknod: {
            SignalGuard guard;
            result = Filesystem::MknodAt(AT_FDCWD, (char*)arg1, arg2, arg3);
            break;
        }
        case felix86_x86_64_mkdir: {
            SignalGuard guard;
            result = Filesystem::MkdirAt(AT_FDCWD, (char*)arg1, arg2);
            break;
        }
        case felix86_x86_64_open: {
            SignalGuard guard;
            result = g_fs->OpenAt(AT_FDCWD, (char*)arg1, (int)arg2, arg3);
            break;
        }
        case felix86_x86_64_alarm: {
            result = ::alarm(arg1);
            break;
        }
        case felix86_x86_64_unlink: {
            SignalGuard guard;
            result = Filesystem::UnlinkAt(AT_FDCWD, (char*)arg1, 0);
            break;
        }
        case felix86_x86_64_stat: {
            SignalGuard guard;
            struct stat stat;
            result = Filesystem::FStatAt(AT_FDCWD, (char*)arg1, &stat, 0);
            if (result >= 0) {
                *(x86_stat*)arg2 = stat;
            }
            break;
        }
        case felix86_x86_64_rmdir: {
            SignalGuard guard;
            result = Filesystem::Rmdir((char*)arg1);
            break;
        }
        case felix86_x86_64_fork: {
            CloneArgs args = {};
            u64 guest_flags = SIGCHLD;
            args.guest_flags = guest_flags;
            args.parent_state = state;
            result = Threads::Clone(state, &args);
            break;
        }
        case felix86_x86_64_vfork: {
            CloneArgs args = {};
            u64 guest_flags = CLONE_VM | CLONE_VFORK | SIGCLD;
            args.guest_flags = guest_flags;
            result = Threads::Clone(state, &args);
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

    if (g_config.strace || (g_config.strace_errors && (i64)result < 0)) {
        std::string trace = trace64(syscall_number, arg1, arg2, arg3, arg4, arg5, arg6);
        trace += " = ";
        if (result < 0) {
            trace += std::to_string(result);
        } else {
            trace += fmt::format("0x{:x}", (u64)result);
        }
        STRACE("%s", trace.c_str());
    }
}

void felix86_syscall32(felix86_frame* frame, u32 rip_next) {
    ASSERT(frame->magic == felix86_frame::expected_magic);
    ASSERT(g_mode32);
    ThreadState* state = frame->state;
    u64 syscall_number = state->GetGpr(X86_REF_RAX);
    u64 arg1 = state->GetGpr(X86_REF_RBX);
    u64 arg2 = state->GetGpr(X86_REF_RCX);
    u64 arg3 = state->GetGpr(X86_REF_RDX);
    u64 arg4 = state->GetGpr(X86_REF_RSI);
    u64 arg5 = state->GetGpr(X86_REF_RDI);
    u64 arg6 = state->GetGpr(X86_REF_RBP);

    ASSERT(!(arg1 & ~0xFFFF'FFFF));
    ASSERT(!(arg2 & ~0xFFFF'FFFF));
    ASSERT(!(arg3 & ~0xFFFF'FFFF));
    ASSERT(!(arg4 & ~0xFFFF'FFFF));
    ASSERT(!(arg5 & ~0xFFFF'FFFF));
    ASSERT(!(arg6 & ~0xFFFF'FFFF));

    Result result;

    bool is_common = is_x86_common(syscall_number);

    if (is_common) {
        int rv_syscall = x86_to_riscv(syscall_number);
        result = felix86_syscall_common(frame, rv_syscall, arg1, arg2, arg3, arg4, arg5, arg6);
    } else {
        switch (syscall_number) {
        case felix86_x86_32_alarm: {
            result = ::alarm(arg1);
            break;
        }
        case felix86_x86_32_symlink: {
            SignalGuard guard;
            result = Filesystem::SymlinkAt((char*)arg1, AT_FDCWD, (char*)arg2);
            break;
        }
        case felix86_x86_32_clone: {
            u64 child_tid = arg5;
            u64 parent_tid = arg3;
            u64 guest_flags = arg1;
            CloneArgs args{
                .parent_state = state,
                .guest_flags = guest_flags,
                .parent_tid = (pid_t*)parent_tid,
                .child_tid = (pid_t*)child_tid,
                .new_tls = arg4, // in this case it's a x86_user_desc*
                .new_rsp = arg2,
                .new_rip = rip_next,
                .new_thread = 0,
                .new_tid = 0,
            };
            result = Threads::Clone(state, &args);
            break;
        }
        case felix86_x86_32_chmod: {
            SignalGuard guard;
            result = Filesystem::Chmod((char*)arg1, arg2);
            break;
        }
        case felix86_x86_32_rename: {
            SignalGuard guard;
            result = Filesystem::RenameAt2(AT_FDCWD, (char*)arg1, AT_FDCWD, (char*)arg2, 0);
            break;
        }
        case felix86_x86_32_rmdir: {
            SignalGuard guard;
            result = Filesystem::Rmdir((char*)arg1);
            break;
        }
        case felix86_x86_32_mkdir: {
            SignalGuard guard;
            result = Filesystem::MkdirAt(AT_FDCWD, (char*)arg1, arg2);
            break;
        }
        case felix86_x86_32_mknod: {
            SignalGuard guard;
            result = Filesystem::MknodAt(AT_FDCWD, (char*)arg1, arg2, arg3);
            break;
        }
        case felix86_x86_32_pipe: {
            result = ::pipe((int*)arg1);
            break;
        }
        case felix86_x86_32_llseek: {
            int fd = arg1;
            u64 offset_high = arg2;
            u64 offset_low = arg3;
            loff_t* res = (loff_t*)arg4;
            u64 whence = arg5;
            u64 offset = (offset_high << 32) | offset_low;
            result = ::lseek(fd, offset, whence);
            if (result >= 0) {
                *res = result;
                result = 0;
            }
            break;
        }
        case felix86_x86_32_getuid16: {
            result = SYSCALL(getuid);
            break;
        }
        case felix86_x86_32_getgid16: {
            result = SYSCALL(getgid);
            break;
        }
        case felix86_x86_32_getegid16: {
            result = SYSCALL(geteuid);
            break;
        }
        case felix86_x86_32_geteuid16: {
            result = SYSCALL(geteuid);
            break;
        }
        case felix86_x86_32_setuid16: {
            result = SYSCALL(setuid, arg1);
            break;
        }
        case felix86_x86_32_setgid16: {
            result = SYSCALL(setgid, arg1);
            break;
        }
        case felix86_x86_32_sendfile: {
            u32* offset = (u32*)arg3;
            off_t host_offset;
            off_t* host_offset_ptr = nullptr;
            if (offset) {
                host_offset = *offset;
                host_offset_ptr = &host_offset;
            }
            result = SYSCALL(sendfile, arg1, arg2, host_offset_ptr, arg4);
            break;
        }
        case felix86_x86_32_ia32_fallocate: {
            int fd = arg1;
            int mode = arg2;
            u64 offset_low = arg3;
            u64 offset_high = arg4;
            u64 length_low = arg5;
            u64 length_high = arg6;
            u64 offset = (offset_high << 32) | offset_low;
            u64 length = (length_high << 32) | length_low;
            result = fallocate(fd, mode, offset, length);
            break;
        }
        case felix86_x86_32_readv: {
            x86_iovec* iovecs32 = (x86_iovec*)arg2;
            std::vector<iovec> iovecs(iovecs32, iovecs32 + arg3);
            result = SYSCALL(readv, arg1, iovecs.data(), arg3);
            break;
        }
        case felix86_x86_32_writev: {
            x86_iovec* iovecs32 = (x86_iovec*)arg2;
            std::vector<iovec> iovecs(iovecs32, iovecs32 + arg3);
            result = SYSCALL(writev, arg1, iovecs.data(), arg3);
            break;
        }
        case felix86_x86_32_epoll_create: {
            // epoll_create has obsolete and ignored argument size, acts the same as epoll_create1 with flags=0
            result = SYSCALL(epoll_create1, 0);
            break;
        }
        case felix86_x86_32_epoll_wait: {
            epoll_event* host_events = (epoll_event*)alloca(std::max(0, (int)arg3) * sizeof(epoll_event));
            result = epoll_wait((int)arg1, host_events, (int)arg3, (int)arg4);
            if (result >= 0) {
                x86_epoll_event* guest_event = (x86_epoll_event*)arg2;
                for (int i = 0; i < result; i++) {
                    guest_event[i] = host_events[i];
                }
            }
            break;
        }
        case felix86_x86_32_inotify_init: {
            result = SYSCALL(inotify_init1, 0);
            break;
        }
        case felix86_x86_32_set_robust_list: {
            state->robust_list = arg1;
            result = 0;
            break;
        }
        case felix86_x86_32_get_robust_list: {
            u32* head_ptr = (u32*)arg2;
            u32* size_ptr = (u32*)arg3;
            *head_ptr = state->robust_list;
            *size_ptr = 12;
            result = 0;
            break;
        }
        case felix86_x86_32_clock_gettime32: {
            if (arg2) {
                struct timespec time;
                result = clock_gettime(arg1, &time);
                *(x86_timespec*)arg2 = time;
            } else {
                result = -EFAULT;
            }
            break;
        }
        case felix86_x86_32_mlockall: {
            result = SYSCALL(mlock2, mmap_min_addr(), 0x1'0000'0000 - mmap_min_addr(), arg1);
            break;
        }
        case felix86_x86_32_munlockall: {
            result = SYSCALL(munlock, mmap_min_addr(), 0x1'0000'0000 - mmap_min_addr());
            break;
        }
        case felix86_x86_32_process_vm_readv: {
            x86_iovec* lvec = (x86_iovec*)arg2;
            size_t liovcnt = arg3;
            x86_iovec* rvec = (x86_iovec*)arg4;
            size_t riovcnt = arg5;

            std::vector<iovec> lvec_host(lvec, lvec + liovcnt);
            std::vector<iovec> rvec_host(rvec, rvec + riovcnt);

            result = SYSCALL(process_vm_readv, arg1, lvec_host.data(), liovcnt, rvec_host.data(), riovcnt, arg6);
            break;
        }
        case felix86_x86_32_process_vm_writev: {
            x86_iovec* lvec = (x86_iovec*)arg2;
            size_t liovcnt = arg3;
            x86_iovec* rvec = (x86_iovec*)arg4;
            size_t riovcnt = arg5;

            std::vector<iovec> lvec_host(lvec, lvec + liovcnt);
            std::vector<iovec> rvec_host(rvec, rvec + riovcnt);

            result = SYSCALL(process_vm_writev, arg1, lvec_host.data(), liovcnt, rvec_host.data(), riovcnt, arg6);
            break;
        }
        case felix86_x86_32_clock_settime32: {
            if (arg2) {
                struct timespec time;
                time = *(x86_timespec*)arg2;
                result = clock_settime(arg1, &time);
            } else {
                result = -EFAULT;
            }
            break;
        }
        case felix86_x86_32_sigprocmask: {
            result = Signals::sigprocmask(state, arg1, (sigset_t*)arg2, (sigset_t*)arg3);
            break;
        }
        case felix86_x86_32_mmap_pgoff: {
            // mmap2 is like mmap but file offset is in pages (4096 bytes) to help with the lack of big enough integers in x86-32
            u64 offset = arg6 * 4096;
            SignalGuard guard;
            result = (ssize_t)g_mapper->map((void*)arg1, arg2, arg3, arg4, arg5, offset);
            if (result > 0) {
                Recompiler::invalidateRangeGlobal(result, result + arg2);

                if ((int)arg5 != -1) {
                    g_symbols_cached = false;
                }
            }
            break;
        }
        case felix86_x86_32_mremap: {
            SignalGuard guard;
            result = (ssize_t)g_mapper->remap32((void*)arg1, arg2, arg3, arg4, (void*)arg5);
            if (result > 0) {
                Recompiler::invalidateRangeGlobal(arg1, arg1 + arg2);
                Recompiler::invalidateRangeGlobal(result, result + arg3);
            }
            break;
        }
        case felix86_x86_32_rt_sigaction: {
            if (arg1 > 64) {
                result = -EINVAL;
                break;
            }

            x86_sigaction* act = (x86_sigaction*)arg2;
            if (act && (arg1 == SIGKILL || arg1 == SIGSTOP)) {
                result = -EINVAL;
                break;
            }

            RegisteredSignal old = Signals::getSignalHandler(state, arg1);
            if (act) {
                auto handler = act->handler;
                Signals::registerSignalHandler(state, arg1, (u64)handler, act->sa_mask, act->sa_flags, act->restorer);
#if 0
                PLAIN("Installed signal handler %s at:", strsignal(arg1));
                print_address((u64)handler);
                PLAIN("Flags: %lx\n", act->sa_flags);
#endif
            }

            x86_sigaction* old_act = (x86_sigaction*)arg3;
            if (old_act) {
                old_act->handler = (decltype(old_act->handler))old.func;
                old_act->sa_flags = old.flags;
                old_act->sa_mask = old.mask;
            }

            result = 0;
            break;
        }
        case felix86_x86_32_open: {
            SignalGuard guard;
            result = g_fs->OpenAt(AT_FDCWD, (char*)arg1, (int)arg2, arg3);
            break;
        }
        case felix86_x86_32_shmat: {
            u32 result_address = 0;
            result = g_mapper->shmat32((int)arg1, (void*)arg2, (int)arg3, &result_address);
            if (result == 0) {
                ASSERT(result_address != 0);
                result = result_address;
            }
            break;
        }
        case felix86_x86_32_shmdt: {
            result = g_mapper->shmdt32((void*)arg1);
            break;
        }
        case felix86_x86_32_waitid: {
            siginfo_t host_siginfo;
            rusage host_rusage;
            siginfo_t* host_siginfo_ptr = nullptr;
            rusage* host_rusage_ptr = nullptr;

            if (arg3) {
                host_siginfo_ptr = &host_siginfo;
            }

            if (arg5) {
                host_rusage = *(x86_rusage*)arg5;
                host_rusage_ptr = &host_rusage;
            }

            result = SYSCALL(waitid, arg1, arg2, host_siginfo_ptr, arg4, host_rusage_ptr);

            if (arg3) {
                *(x86_siginfo_t*)arg3 = host_siginfo;
            }

            if (arg5) {
                *(x86_rusage*)arg5 = host_rusage;
            }
            break;
        }
        case felix86_x86_32_set_thread_area: {
            x86_user_desc* udesc = (x86_user_desc*)arg1;
            result = state->SetUserDesc(udesc);
            break;
        }
        case felix86_x86_32_get_thread_area: {
            x86_user_desc* udesc = (x86_user_desc*)arg1;
            int index = udesc->entry_number;

            *udesc = {};
            udesc->base_addr = state->gdt[index];

            if (udesc->base_addr != 0) {
                udesc->limit = 0xFFFFF;
                udesc->seg_32bit = 1;
                udesc->limit_in_pages = 1;
                udesc->usable = 1;
            } else {
                udesc->read_exec_only = 1;
                udesc->seg_not_present = 1;
            }

            LOG("Getting thread area %d which is %lx", index, udesc->base_addr);

            result = 0;
            break;
        }
        case felix86_x86_32_getrlimit: {
            rlimit limit;
            result = getrlimit((int)arg1, &limit);
            if (result == 0) {
                x86_rlimit* guest_limit = (x86_rlimit*)arg2;
                *guest_limit = limit;
            }
            break;
        }
        case felix86_x86_32_sigaltstack: {
            VERBOSE("----- sigaltstack was called -----");
            x86_stack_t* new_ss = (x86_stack_t*)arg1;
            x86_stack_t* old_ss = (x86_stack_t*)arg2;
            u64 current_rsp = state->gprs[X86_REF_RSP];

            bool on_stack = false;
            if (!(state->alt_stack.ss_flags & SS_DISABLE) && current_rsp >= (u64)state->alt_stack.ss_sp && current_rsp < state->alt_stack.ss_size) {
                on_stack = true;
            }

            if (old_ss) {
                old_ss->ss_sp = (u32)(u64)state->alt_stack.ss_sp;
                old_ss->ss_flags = 0;
                old_ss->ss_size = state->alt_stack.ss_size;

                if (on_stack) {
                    old_ss->ss_flags = SS_ONSTACK;
                } else {
                    old_ss->ss_flags = SS_DISABLE;
                }
            }

            if (new_ss) {
                if (on_stack) {
                    WARN("Tried to set sigaltstack while using it");
                    result = -EPERM;
                    break;
                }

                state->alt_stack.ss_sp = (void*)(u64)new_ss->ss_sp;
                state->alt_stack.ss_flags = new_ss->ss_flags;
                state->alt_stack.ss_size = new_ss->ss_size;
                VERBOSE("New altstack: %lx", new_ss->ss_sp);
            }

            result = 0;
            break;
        }
        case felix86_x86_32_access: {
            SignalGuard guard;
            result = Filesystem::FAccessAt(AT_FDCWD, (char*)arg1, (int)arg2, 0);
            break;
        }
        case felix86_x86_32_unlink: {
            SignalGuard guard;
            result = Filesystem::UnlinkAt(AT_FDCWD, (char*)arg1, 0);
            break;
        }
        case felix86_x86_32_fcntl:
        case felix86_x86_32_fcntl64: {
            constexpr int X86_GETLK64 = 12;
            constexpr int X86_SETLK64 = 13;
            constexpr int X86_SETLKW64 = 14;
            const int fd = arg1;
            struct flock host_flock;
            x86_flock64* guest_flock64 = (x86_flock64*)arg3;
            x86_flock* guest_flock32 = (x86_flock*)arg3;
            switch (arg2) {
            case X86_GETLK64: {
                host_flock = *guest_flock64;
                result = ::fcntl(fd, F_GETLK, &host_flock);
                if (result >= 0) {
                    *guest_flock64 = host_flock;
                }
                break;
            }
            case X86_SETLK64: {
                host_flock = *guest_flock64;
                result = ::fcntl(fd, F_SETLK, &host_flock);
                break;
            }
            case X86_SETLKW64: {
                host_flock = *guest_flock64;
                result = ::fcntl(fd, F_SETLKW, &host_flock);
                break;
            }
            case F_OFD_GETLK: {
                host_flock = *guest_flock64;
                result = ::fcntl(fd, F_OFD_GETLK, &host_flock);
                if (result >= 0) {
                    *guest_flock64 = host_flock;
                }
                break;
            }
            case F_OFD_SETLK:
            case F_OFD_SETLKW: {
                host_flock = *guest_flock64;
                result = ::fcntl(fd, arg2, &host_flock);
                break;
            }
            case F_GETLK: {
                host_flock = *guest_flock32;
                result = ::fcntl(fd, F_GETLK, &host_flock);
                if (result >= 0) {
                    *guest_flock32 = host_flock;
                }
                break;
            }
            case F_SETLK: {
                host_flock = *guest_flock32;
                result = ::fcntl(fd, F_SETLK, &host_flock);
                break;
            }
            case F_SETLKW: {
                host_flock = *guest_flock32;
                result = ::fcntl(fd, F_SETLKW, &host_flock);
                break;
            }
            case F_SETFL:
            case F_DUPFD:
            case F_DUPFD_CLOEXEC:
            case F_GETFD:
            case F_SETFD:
            case F_GETFL:
            case F_ADD_SEALS:
            case F_GET_SEALS:
            case F_GETPIPE_SZ:
            case F_SETPIPE_SZ:
            case F_NOTIFY: {
                result = ::fcntl(arg1, arg2, arg3);
                break;
            }
            default: {
                WARN("Unknown fcntl: %d", arg2);
                result = ::fcntl(arg1, arg2, arg3);
                break;
            }
            }
            break;
        }
        case felix86_x86_32_waitpid: {
            result = ::waitpid((pid_t)arg1, (int*)arg2, (int)arg3);
            break;
        }
        case felix86_x86_32_ioctl: {
            result = ::ioctl32(arg1, arg2, arg3);
            break;
        }
        case felix86_x86_32_statfs64: {
            SignalGuard guard;
            ASSERT(arg2 == sizeof(x86_statfs64));
            struct statfs statfs;
            x86_statfs64* guest_statfs = (x86_statfs64*)arg3;
            result = Filesystem::StatFs((char*)arg1, &statfs);
            if (result >= 0) {
                *guest_statfs = statfs;
            }
            break;
        }
        case felix86_x86_32_fstatfs64: {
            SignalGuard guard;
            ASSERT(arg2 == sizeof(x86_statfs64));
            struct statfs statfs;
            x86_statfs64* guest_statfs = (x86_statfs64*)arg3;
            result = ::fstatfs(arg1, &statfs);
            if (result >= 0) {
                *guest_statfs = statfs;
            }
            break;
        }
        case felix86_x86_32_sysinfo: {
            struct sysinfo host_sysinfo;
            result = ::sysinfo(&host_sysinfo);
            if (result == 0) {
                *(x86_sysinfo*)arg1 = host_sysinfo;
            }
            break;
        }
        case felix86_x86_32_link: {
            SignalGuard guard;
            result = Filesystem::LinkAt(AT_FDCWD, (char*)arg1, AT_FDCWD, (char*)arg2, 0);
            break;
        }
        case felix86_x86_32_time32: {
            time_t time;
            result = ::time(&time);
            if (result == 0) {
                *(u32*)arg1 = time;
            }
            break;
        }
        case felix86_x86_32_ia32_fadvise64: {
            int fd = arg1;
            u64 offset_low = arg2;
            u64 offset_high = arg3;
            u64 len = arg4;
            int advice = arg5;
            u64 offset = offset_low | (offset_high << 32);
            result = posix_fadvise64(fd, offset, len, advice);
            break;
        }
        case felix86_x86_32_ia32_fadvise64_64: {
            int fd = arg1;
            u64 offset_low = arg2;
            u64 offset_high = arg3;
            u64 len_low = arg4;
            u64 len_high = arg5;
            int advice = arg6;
            u64 offset = offset_low | (offset_high << 32);
            u64 len = len_low | (len_high << 32);
            result = posix_fadvise64(fd, offset, len, advice);
            break;
        }
        case felix86_x86_32_readlink: {
            SignalGuard guard;
            if (arg1 == arg2) {
                WARN("arg1 == arg2 during readlink");
            }
            result = Filesystem::ReadlinkAt(AT_FDCWD, (char*)arg1, (char*)arg2, (int)arg3);
            break;
        }
        case felix86_x86_32_ipc: {
            SignalGuard guard;
            result = ::ipc32(arg1, arg2, arg3, arg4, (void*)arg5, arg6);
            break;
        }
        case felix86_x86_32_stat64: {
            SignalGuard guard;
            struct stat stat;
            result = Filesystem::FStatAt(AT_FDCWD, (char*)arg1, &stat, 0);
            if (result >= 0) {
                *(x86_stat64*)arg2 = stat;
            }
            break;
        }
        case felix86_x86_32_lstat64: {
            SignalGuard guard;
            struct stat host_stat;
            result = Filesystem::FStatAt(AT_FDCWD, (char*)arg1, &host_stat, AT_SYMLINK_NOFOLLOW);
            if (result >= 0) {
                *(x86_stat64*)arg2 = host_stat;
            }
            break;
        }
        case felix86_x86_32_getdents: {
            WARN("This program uses getdents");
            result = -ENOSYS;
            break;
        }
        case felix86_x86_32_getdents64: {
            u32 fd = arg1;
            u64 dirp = arg2;
            u32 count = arg3;

            result = SYSCALL(getdents64, fd, dirp, count);
            if (result >= 0) {
                size_t bytes = result;
                size_t num = 0;
                for (size_t i = 0; i < bytes;) {
                    x86_linux_dirent* current = (x86_linux_dirent*)(dirp + i);
                    current->d_off = num++;
                    i += current->d_reclen;
                }
            }
            break;
        }
        case felix86_x86_32_ppoll_time32: {
            struct pollfd* ufds = (struct pollfd*)arg1;
            u64 nfds = arg2;
            struct x86_timespec* tsp = (struct x86_timespec*)arg3;
            u64* sigmask = (u64*)arg4;
            u64 sigsetsize = arg5;
            struct timespec host_timespec;
            if (tsp) {
                host_timespec = *tsp;
            }

            result = SYSCALL(ppoll, ufds, nfds, tsp ? &host_timespec : nullptr, sigmask, sigsetsize);

            if (tsp) {
                *tsp = host_timespec;
            }
            break;
        }
        case felix86_x86_32_fstat64: {
            struct stat host_stat;
            result = ::fstat(arg1, &host_stat);
            if (result >= 0) {
                *(x86_stat64*)arg2 = host_stat;
            }
            break;
        }
        case felix86_x86_32_clock_nanosleep_time32: {
            timespec rqtp, rmtp;
            const x86_timespec* guest_rqtp = (x86_timespec*)arg3;
            x86_timespec* guest_rmtp = (x86_timespec*)arg4;
            if (!guest_rqtp) {
                result = -EFAULT;
                break;
            }

            rqtp = *guest_rqtp;
            result = SYSCALL(clock_nanosleep, arg1, arg2, &rqtp, &rmtp);

            if (result == 0 && guest_rmtp) {
                *guest_rmtp = rmtp;
            }
            break;
        }
        case felix86_x86_32_clock_getres: {
            timespec tp;
            x86_timespec* guest_tp = (x86_timespec*)arg2;
            if (!guest_tp) {
                result = -EFAULT;
                break;
            }

            result = SYSCALL(clock_getres, arg1, &tp);

            if (result == 0) {
                *guest_tp = tp;
            }
            break;
        }
        case felix86_x86_32_ia32_pread64: {
            u64 pos_low = arg4;
            u64 pos_high = arg5;
            u64 pos = pos_low | (pos_high << 32);
            result = pread64(arg1, (void*)arg2, arg3, pos);
            break;
        }
        case felix86_x86_32_ia32_pwrite64: {
            u64 pos_low = arg4;
            u64 pos_high = arg5;
            u64 pos = pos_low | (pos_high << 32);
            result = pwrite64(arg1, (void*)arg2, arg3, pos);
            break;
        }
        case felix86_x86_32_futex_time32: {
            const x86_timespec* guest_spec = (x86_timespec*)arg4;
            if (guest_spec) {
                const timespec host_spec = *guest_spec;
                result = SYSCALL(futex, arg1, arg2, arg3, &host_spec, arg5, arg6);
            } else {
                result = SYSCALL(futex, arg1, arg2, arg3, arg4, arg5, arg6);
            }
            break;
        }
        case felix86_x86_32_poll: {
            result = ::poll((pollfd*)arg1, arg2, arg3);
            break;
        }
        case felix86_x86_32_sendmsg: {
            result = ::sendmsg32(arg1, (x86_msghdr*)arg2, arg3);
            break;
        }
        case felix86_x86_32_recvmsg: {
            result = ::recvmsg32(arg1, (x86_msghdr*)arg2, arg3);
            break;
        }
        case felix86_x86_32_getsockopt: {
            result = ::getsockopt32(arg1, arg2, arg3, (char*)arg4, (u32*)arg5);
            break;
        }
        case felix86_x86_32_setsockopt: {
            result = ::setsockopt32(arg1, arg2, arg3, (char*)arg4, arg5);
            break;
        }
        case felix86_x86_32_wait4: {
            x86_rusage* guest_rusage = (x86_rusage*)arg4;
            rusage host_rusage;
            rusage* host_rusage_ptr = nullptr;
            if (guest_rusage) {
                host_rusage = *guest_rusage;
                host_rusage_ptr = &host_rusage;
            }

            result = ::wait4(arg1, (int*)arg2, arg3, host_rusage_ptr);

            if (guest_rusage) {
                *guest_rusage = host_rusage;
            }
            break;
        }
        case felix86_x86_32_getrusage: {
            if (!arg2) {
                WARN("getrusage with nullptr argument?");
                result = -EINVAL;
            } else {
                x86_rusage* guest_rusage = (x86_rusage*)arg2;
                rusage host_rusage = *guest_rusage;
                result = getrusage(arg1, &host_rusage);
                *guest_rusage = host_rusage;
            }
            break;
        }
        case felix86_x86_32_pselect6_time32: {
            // fd_set in x86-32 is a bunch of 32-bit integers, while in 64-bit architectures it's
            // a bunch of 64-bit integers, so some marshalling is due
            int nfds = arg1;
            x86_fdset* readfds = (x86_fdset*)arg2;
            x86_fdset* writefds = (x86_fdset*)arg3;
            x86_fdset* exceptfds = (x86_fdset*)arg4;
            x86_timespec* timeout = (x86_timespec*)arg5;
            x86_sigset_argpack* sigset = (x86_sigset_argpack*)arg6;
            struct timespec host_timespec;
            if (timeout) {
                host_timespec = *timeout;
            }

            fd_set host_readfds;
            fd_set host_writefds;
            fd_set host_exceptfds;
            sigset_t host_sigset;
            FD_ZERO(&host_readfds);
            FD_ZERO(&host_writefds);
            FD_ZERO(&host_exceptfds);
            sigemptyset(&host_sigset);

            nfds += 31;
            nfds &= ~31;

            int word_count = nfds / sizeof(u32);

            if (readfds) {
                for (int word_index = 0; word_index < word_count; word_index++) {
                    int remaining = nfds - (word_index * 32);
                    for (int i = 0; i < 32 && i < remaining; i++) {
                        bool is_set = (readfds[word_index] >> i) & 1;
                        if (is_set) {
                            FD_SET(word_index * 32 + i, &host_readfds);
                        }
                    }
                }
            }

            if (writefds) {
                for (int word_index = 0; word_index < word_count; word_index++) {
                    int remaining = nfds - (word_index * 32);
                    for (int i = 0; i < 32 && i < remaining; i++) {
                        bool is_set = (writefds[word_index] >> i) & 1;
                        if (is_set) {
                            FD_SET(word_index * 32 + i, &host_writefds);
                        }
                    }
                }
            }

            if (exceptfds) {
                for (int word_index = 0; word_index < word_count; word_index++) {
                    int remaining = nfds - (word_index * 32);
                    for (int i = 0; i < 32 && i < remaining; i++) {
                        bool is_set = (exceptfds[word_index] >> i) & 1;
                        if (is_set) {
                            FD_SET(word_index * 32 + i, &host_exceptfds);
                        }
                    }
                }
            }

            if (sigset) {
                ASSERT(sigset->data);
                u64* ptr = (u64*)(u64)sigset->data;
                u64 val = *ptr;
                u32 size = sigset->size;
                ASSERT(size <= 8);
                for (u32 i = 0; i < size * 8; i++) {
                    if (val & (1ull << i)) {
                        sigaddset(&host_sigset, i + 1);
                    }
                }
            }

            result = ::pselect(nfds, readfds ? &host_readfds : nullptr, writefds ? &host_writefds : nullptr, exceptfds ? &host_exceptfds : nullptr,
                               timeout ? &host_timespec : nullptr, &host_sigset);

            if (readfds) {
                for (int i = 0; i < nfds; i++) {
                    if (FD_ISSET(i, &host_readfds)) {
                        readfds[i / 32] |= 1 << (i & 31);
                    } else {
                        readfds[i / 32] &= ~(1 << (i & 31));
                    }
                }
            }

            if (writefds) {
                for (int i = 0; i < nfds; i++) {
                    if (FD_ISSET(i, &host_writefds)) {
                        writefds[i / 32] |= 1 << (i & 31);
                    } else {
                        writefds[i / 32] &= ~(1 << (i & 31));
                    }
                }
            }

            if (exceptfds) {
                for (int i = 0; i < nfds; i++) {
                    if (FD_ISSET(i, &host_exceptfds)) {
                        exceptfds[i / 32] |= 1 << (i & 31);
                    } else {
                        exceptfds[i / 32] &= ~(1 << (i & 31));
                    }
                }
            }

            if (timeout) {
                *timeout = host_timespec;
            }
            break;
        }
        case felix86_x86_32_dup2: {
            result = FD::dup2(arg1, arg2);
            break;
        }
        case felix86_x86_32_fstatat64: {
            x86_stat64* stat = (x86_stat64*)arg3;
            struct stat64 host_stat;
            result = Filesystem::FStatAt64(arg1, (char*)arg2, &host_stat, arg4);
            if (result == 0) {
                *stat = host_stat;
            }
            break;
        }
        case felix86_x86_32_utimensat_time32: {
            SignalGuard guard;
            struct timespec host_times[2];
            int dirfd = arg1;
            const char* pathname = (const char*)arg2;
            x86_timespec* guest_times = (x86_timespec*)arg3;
            int flags = arg4;

            if (guest_times) {
                host_times[0] = guest_times[0];
                host_times[1] = guest_times[1];
                result = Filesystem::UtimensAt(dirfd, pathname, host_times, flags);
            } else {
                result = Filesystem::UtimensAt(dirfd, pathname, nullptr, flags);
            }
            break;
        }
        case felix86_x86_32_ia32_truncate64: {
            u64 offset_low = arg2;
            u64 offset_high = arg3;
            u64 offset = offset_low | offset_high << 32;
            result = Filesystem::Truncate((char*)arg1, offset);
            break;
        }
        case felix86_x86_32_ia32_ftruncate64: {
            int fd = arg1;
            u64 offset_low = arg2;
            u64 offset_high = arg3;
            u64 offset = (offset_high << 32) | offset_low;
            result = ftruncate(fd, offset);
            break;
        }
        case felix86_x86_32_socketcall: { // Funny syscall before the functions were seperated
            enum {
                SYS_SOCKET = 1,
                SYS_BIND = 2,
                SYS_CONNECT = 3,
                SYS_LISTEN = 4,
                SYS_ACCEPT = 5,
                SYS_GETSOCKNAME = 6,
                SYS_GETPEERNAME = 7,
                SYS_SOCKETPAIR = 8,
                SYS_SEND = 9,
                SYS_RECV = 10,
                SYS_SENDTO = 11,
                SYS_RECVFROM = 12,
                SYS_SHUTDOWN = 13,
                SYS_SETSOCKOPT = 14,
                SYS_GETSOCKOPT = 15,
                SYS_SENDMSG = 16,
                SYS_RECVMSG = 17,
                SYS_ACCEPT4 = 18,
                SYS_RECVMMSG = 19,
                SYS_SENDMMSG = 20,
            };

            u32* args = (u32*)arg2;
            switch (arg1) {
            case SYS_SOCKET: {
                result = ::socket(args[0], args[1], args[2]);
                break;
            }
            case SYS_BIND: {
                auto sock = (sockaddr*)(u64)args[1];

                if (args[2] <= 2 || sock->sa_family != AF_UNIX || (sock->sa_family == AF_UNIX && sock->sa_data[0] == 0)) {
                    result = ::bind(args[0], (sockaddr*)(u64)args[1], args[2]);
                } else {
                    FdPath socket_path = Filesystem::resolve((char*)sock->sa_data, true);
                    int len = strlen(socket_path.full_path());
                    if (len >= 108) {
                        WARN("AF_UNIX path overflowed: %s %d", socket_path.full_path(), len);
                    }
                    sockaddr_un addr = {0};
                    addr.sun_family = AF_UNIX;
                    strncpy(addr.sun_path, socket_path.full_path(), sizeof(addr.sun_path));
                    size_t addrlen = offsetof(sockaddr_un, sun_path) + strlen(addr.sun_path) + 1;

                    result = ::bind(args[0], (sockaddr*)&addr, addrlen);
                }
                break;
            }
            case SYS_CONNECT: {
                auto sock = (sockaddr*)(u64)args[1];

                if (args[2] <= 2 || sock->sa_family != AF_UNIX || (sock->sa_family == AF_UNIX && sock->sa_data[0] == 0)) {
                    result = ::connect(args[0], (sockaddr*)(u64)args[1], args[2]);
                } else {
                    FdPath socket_path = Filesystem::resolve((char*)sock->sa_data, true);
                    int len = strlen(socket_path.full_path());
                    if (len >= 108) {
                        WARN("AF_UNIX path overflowed: %s %d", socket_path.full_path(), len);
                    }
                    sockaddr_un addr = {0};
                    addr.sun_family = AF_UNIX;
                    strncpy(addr.sun_path, socket_path.full_path(), sizeof(addr.sun_path));
                    size_t addrlen = offsetof(sockaddr_un, sun_path) + strlen(addr.sun_path) + 1;

                    result = ::connect(args[0], (sockaddr*)&addr, addrlen);
                }
                break;
            }
            case SYS_LISTEN: {
                result = ::listen(args[0], args[1]);
                break;
            }
            case SYS_ACCEPT: {
                result = ::accept(args[0], (sockaddr*)(u64)args[1], (socklen_t*)(u64)args[2]);
                break;
            }
            case SYS_GETSOCKNAME: {
                result = ::getsockname(args[0], (sockaddr*)(u64)args[1], (socklen_t*)(u64)args[2]);
                break;
            }
            case SYS_GETPEERNAME: {
                result = ::getpeername(args[0], (sockaddr*)(u64)args[1], (socklen_t*)(u64)args[2]);
                break;
            }
            case SYS_SOCKETPAIR: {
                result = ::socketpair(args[0], args[1], args[2], (i32*)(u64)args[3]);
                break;
            }
            case SYS_SEND: {
                result = ::send(args[0], (void*)(u64)args[1], args[2], args[3]);
                break;
            }
            case SYS_RECV: {
                result = ::recv(args[0], (void*)(u64)args[1], args[2], args[3]);
                break;
            }
            case SYS_SENDTO: {
                result = ::sendto(args[0], (void*)(u64)args[1], args[2], args[3], (sockaddr*)(u64)args[4], args[5]);
                break;
            }
            case SYS_RECVFROM: {
                result = ::recvfrom(args[0], (void*)(u64)args[1], args[2], args[3], (sockaddr*)(u64)args[4], (socklen_t*)(u64)args[5]);
                break;
            }
            case SYS_SHUTDOWN: {
                result = ::shutdown(args[0], args[1]);
                break;
            }
            case SYS_SETSOCKOPT: {
                result = ::setsockopt32(args[0], args[1], args[2], (char*)(u64)args[3], args[4]);
                break;
            }
            case SYS_GETSOCKOPT: {
                result = ::getsockopt32(args[0], args[1], args[2], (char*)(u64)args[3], (u32*)(u64)args[4]);
                break;
            }
            case SYS_SENDMSG: {
                result = ::sendmsg32(args[0], (x86_msghdr*)(u64)args[1], args[2]);
                break;
            }
            case SYS_RECVMSG: {
                result = ::recvmsg32(args[0], (x86_msghdr*)(u64)args[1], args[2]);
                break;
            }
            case SYS_ACCEPT4: {
                result = ::accept4(args[0], (sockaddr*)(u64)args[1], (socklen_t*)(u64)args[2], args[3]);
                break;
            }
            default: {
                ERROR("Unimplemented socketcall command: %d", arg1);
                result = -EINVAL;
                break;
            }
            }
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

    if (g_config.strace || (g_config.strace_errors && (i64)result < 0)) {
        std::string trace = trace32(syscall_number, arg1, arg2, arg3, arg4, arg5, arg6);
        trace += " = ";
        if (result < 0) {
            trace += std::to_string(result);
        } else {
            trace += fmt::format("0x{:x}", (u64)result);
        }
        STRACE("%s", trace.c_str());
    }
}