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
#include <sys/utsname.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#undef VMIN
#include "felix86/common/log.hpp"
#include "felix86/common/state.hpp"
#include "felix86/common/strace.hpp"
#include "felix86/common/symlink.hpp"
#include "felix86/emulator.hpp"
#include "felix86/hle/brk.hpp"
#include "felix86/hle/filesystem.hpp"
#include "felix86/hle/guest_types.hpp"
#include "felix86/hle/ipc32.hpp"
#include "felix86/hle/socket32.hpp"
#include "felix86/hle/syscall.hpp"
#include "felix86/hle/thread.hpp"

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

Result felix86_syscall_common(ThreadState* state, int rv_syscall, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5, u64 arg6) {
    Result result;
    switch (rv_syscall) {
    case felix86_riscv64_brk: {
        result = BRK::set(arg1);
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
    case felix86_riscv64_rseq: {
        // Couldn't find any solid documentation and FEX doesn't support it either
        result = -ENOSYS;
        break;
    }
    case felix86_riscv64_personality: {
        result = SYSCALL(personality, arg1 & ~PER_LINUX32);
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
        break;
    }
    case felix86_riscv64_close: {
        result = SYSCALL(close, arg1, arg2, arg3, arg4, arg5, arg6);
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
        result = SYSCALL(shmat, arg1, (void*)arg2, arg3);
        break;
    }
    case felix86_riscv64_shmctl: {
        result = SYSCALL(shmctl, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_shmdt: {
        result = SYSCALL(shmdt, (void*)arg1);
        break;
    }
    case felix86_riscv64_bind: {
        result = SYSCALL(bind, arg1, arg2, arg3);
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
        result = Filesystem::Getcwd((char*)arg1, arg2);
        break;
    }
    case felix86_riscv64_symlinkat: {
        result = Filesystem::SymlinkAt((char*)arg1, arg2, (char*)arg3);
        break;
    }
    case felix86_riscv64_renameat2: {
        result = Filesystem::RenameAt2(arg1, (char*)arg2, arg3, (char*)arg4, arg5);
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
        std::vector<epoll_event> host_events(std::max(0, (int)arg3));
        result = SYSCALL(epoll_pwait, arg1, host_events.data(), arg3, arg4, arg5, arg6);
        if (result >= 0) {
            x86_epoll_event* guest_event = (x86_epoll_event*)arg2;
            for (int i = 0; i < result; i++) {
                guest_event[i] = host_events[i];
            }
        }
        break;
    }
    case felix86_riscv64_epoll_pwait2: {
        std::vector<epoll_event> host_events(std::max(0, (int)arg3));
        result = SYSCALL(epoll_pwait2, arg1, host_events.data(), arg3, arg4, arg5, arg6);
        if (result >= 0) {
            x86_epoll_event* guest_event = (x86_epoll_event*)arg2;
            for (int i = 0; i < result; i++) {
                guest_event[i] = host_events[i];
            }
        }
        break;
    }
    case felix86_riscv64_mount: {
        result = Filesystem::Mount((char*)arg1, (char*)arg2, (char*)arg3, arg4, (void*)arg5);
        break;
    }
    case felix86_riscv64_accept: {
        result = SYSCALL(accept, arg1, arg2, arg3, arg4, arg5, arg6);
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
        result = SYSCALL(umount2, arg1, arg2, arg3, arg4, arg5, arg6);
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
        result = SYSCALL(dup3, arg1, arg2, arg3);
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
        result = Filesystem::Chdir((char*)arg1);
        break;
    }
    case felix86_riscv64_fchown: {
        result = SYSCALL(fchown, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_unlinkat: {
        result = Filesystem::UnlinkAt((int)arg1, (char*)arg2, (int)arg3);
        break;
    }
    case felix86_riscv64_fchdir: {
        result = SYSCALL(fchdir, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_newfstatat: {
        result = Filesystem::FStatAt((int)arg1, (char*)arg2, (x86_stat*)arg3, (int)arg4);
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
        Emulator::ExitDispatcher(state);
        UNREACHABLE();
        break;
    }
    case felix86_riscv64_faccessat:
    case felix86_riscv64_faccessat2: {
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
        result = Filesystem::LGetXAttr((char*)arg1, (char*)arg2, (void*)arg3, arg4);
        break;
    }
    case felix86_riscv64_getxattr: {
        result = Filesystem::GetXAttr((char*)arg1, (char*)arg2, (void*)arg3, arg4);
        break;
    }
    case felix86_riscv64_fgetxattr: {
        result = SYSCALL(fgetxattr, arg1, arg2, arg3, arg4);
        break;
    }
    case felix86_riscv64_setxattr: {
        result = Filesystem::SetXAttr((char*)arg1, (char*)arg2, (void*)arg3, arg4, arg5);
        break;
    }
    case felix86_riscv64_lsetxattr: {
        result = Filesystem::LSetXAttr((char*)arg1, (char*)arg2, (void*)arg3, arg4, arg5);
        break;
    }
    case felix86_riscv64_fsetxattr: {
        result = SYSCALL(fsetxattr, arg1, arg2, arg3, arg4, arg5);
        break;
    }
    case felix86_riscv64_removexattr: {
        result = Filesystem::RemoveXAttr((char*)arg1, (char*)arg2);
        break;
    }
    case felix86_riscv64_lremovexattr: {
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
    case felix86_riscv64_openat: {
        result = Filesystem::OpenAt((int)arg1, (char*)arg2, (int)arg3, arg4);
        break;
    }
    case felix86_riscv64_tgkill: {
        result = SYSCALL(tgkill, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_kill: {
        result = SYSCALL(kill, arg1, arg2, arg3, arg4, arg5, arg6);
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
            state->signals_disabled = true;
            result = (ssize_t)g_mapper->map32((void*)arg1, arg2, arg3, (int)arg4, (int)arg5, arg6);
            state->signals_disabled = false;
        } else {
            // No need to use mapper
            result = SYSCALL(mmap, arg1, arg2, arg3, (int)arg4, (int)arg5, arg6);
        }
        break;
    }
    case felix86_riscv64_munmap: {
        if (arg1 < Mapper::addressSpaceEnd32 || g_mode32) {
            // Track unmaps in the 32-bit address space for MAP_32BIT in 64-bit mode
            state->signals_disabled = true;
            result = g_mapper->unmap32((void*)arg1, arg2);
            state->signals_disabled = false;
        } else {
            result = SYSCALL(munmap, arg1, arg2, arg3, arg4, arg5, arg6);
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
    case felix86_riscv64_gettid: {
        result = SYSCALL(gettid);
        break;
    }
    case felix86_riscv64_socket: {
        result = SYSCALL(socket, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_connect: {
        result = SYSCALL(connect, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_mremap: {
        state->signals_disabled = true;
        result = (u64)g_mapper->remap((void*)arg1, arg2, arg3, arg4, (void*)arg5);
        state->signals_disabled = false;
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
        Emulator::ExitDispatcher(state);
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
        result = Filesystem::FChmodAt((int)arg1, (char*)arg2, arg3);
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
        x86_semid64_ds* guest_semi = (x86_semid64_ds*)arg4;
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
        x64_sigaction* act = (x64_sigaction*)arg2;
        if (act) {
            auto handler = act->handler;
            Signals::registerSignalHandler(state, arg1, GuestAddress{(u64)handler}, act->sa_mask, act->sa_flags);
            if (g_config.verbose) {
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
        result = SYSCALL(mknodat, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_sigaltstack: {
        VERBOSE("----- sigaltstack was called -----");
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
    case felix86_riscv64_fchownat: {
        result = SYSCALL(fchownat, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_sync_file_range: {
        result = SYSCALL(sync_file_range, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_mkdirat: {
        result = Filesystem::MkdirAt(arg1, (char*)arg2, arg3);
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
            WARN("Execve couldn't find path: %s", path.c_str());
            result = -ENOENT;
            break;
        }

        if (!std::filesystem::is_regular_file(path)) {
            WARN("Not regular file during execve: %s", path.c_str());
            result = -ENOENT;
            break;
        }

        std::vector<const char*> argv;
        std::vector<const char*> envp;

        // Resolving this symlink helps gdb find the path
        std::filesystem::path emulator = g_emulator_path;
        argv.push_back(emulator.c_str());

        if (arg2) {
            u8* guest_argv = (u8*)arg2;
            argv.push_back(path.c_str()); // push the resolved path instead of the path in argv[0];
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
            // Args shouldn't be null normally, but at least push the emulated executable here
            argv.push_back(path.c_str());
        }
        argv.push_back(nullptr);

        // Pass the host arguments first because they may need to be overwritten by the ones the guest specifies
        char** host_environ = environ;
        while (*host_environ) {
            std::string env = *host_environ;
            if (env.find("FELIX86") != std::string::npos) {
                envp.push_back(*host_environ);
            }
            host_environ++;
        }

        if (arg3) {
            u8* guest_envp = (u8*)arg3;
            while (true) {
                u64 ptr = 0;
                memcpy(&ptr, guest_envp, g_mode32 ? 4 : 8);
                if (ptr == 0) {
                    break;
                }

                envp.push_back((const char*)ptr);
                guest_envp += g_mode32 ? 4 : 8;
            }
        } else {
            WARN("envp null during execve...?");
        }

        // We need to tell the new process where the server is
        std::string log_env = std::string("__FELIX86_PIPE=") + Logger::getPipeName();
        envp.push_back("__FELIX86_EXECVE=1");
        envp.push_back(log_env.c_str());
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
        result = SYSCALL(umask, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_ppoll: {
        result = SYSCALL(ppoll, arg1, arg2, arg3, arg4, arg5, arg6);
        break;
    }
    case felix86_riscv64_linkat: {
        result = Filesystem::LinkAt((int)arg1, (char*)arg2, (int)arg3, (char*)arg4, (int)arg5);
        break;
    }
    case felix86_riscv64_readlinkat: {
        if (arg2 == arg3) {
            WARN("arg2 == arg3 during readlinkat");
        }
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
    case felix86_riscv64_epoll_create1: {
        result = SYSCALL(epoll_create1, arg1);
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
            int result = pthread_sigmask(SIG_SETMASK, &host_mask, nullptr);
            ASSERT(result == 0);
        }

        if (oldset) {
            memcpy(oldset, &old_host_set, sizeof(u64));
        }

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
            break;
        }
        case felix86_x86_64_link: {
            result = Filesystem::SymlinkAt((char*)arg1, AT_FDCWD, (char*)arg2);
            break;
        }
        case felix86_x86_64_readlink: {
            if (arg1 == arg2) {
                WARN("arg1 == arg2 during readlink");
            }
            result = Filesystem::ReadlinkAt(AT_FDCWD, (char*)arg1, (char*)arg2, (int)arg3);
            break;
        }
        case felix86_x86_64_getpgrp: {
            result = getpgrp();
            break;
        }
        case felix86_x86_64_rename: {
            result = Filesystem::Rename((char*)arg1, (char*)arg2);
            break;
        }
        case felix86_x86_64_epoll_create: {
            // epoll_create has obsolete and ignored argument size, acts the same as epoll_create1 with flags=0
            result = SYSCALL(epoll_create1, 0);
            break;
        }
        case felix86_x86_64_epoll_wait: {
            std::vector<epoll_event> host_events(std::max(0, (int)arg3));
            result = epoll_wait((int)arg1, host_events.data(), (int)arg3, (int)arg4);
            if (result >= 0) {
                x86_epoll_event* guest_event = (x86_epoll_event*)arg2;
                for (int i = 0; i < result; i++) {
                    guest_event[i] = host_events[i];
                }
            }
            break;
        }
        case felix86_x86_64_chmod: {
            result = Filesystem::Chmod((char*)arg1, arg2);
            break;
        }
        case felix86_x86_64_symlink: {
            result = Filesystem::SymlinkAt((char*)arg1, AT_FDCWD, (char*)arg2);
            break;
        }
        case felix86_x86_64_renameat: {
            result = Filesystem::RenameAt2(arg1, (char*)arg2, arg3, (char*)arg4, 0);
            break;
        }
        case felix86_x86_64_poll: {
            result = poll((struct pollfd*)arg1, arg2, arg3);
            break;
        }
        case felix86_x86_64_dup2: {
            result = ::dup2(arg1, arg2);
            break;
        }
        case felix86_x86_64_lstat: {
            result = Filesystem::FStatAt(AT_FDCWD, (char*)arg1, (x86_stat*)arg2, AT_SYMLINK_NOFOLLOW);
            break;
        }
        case felix86_x86_64_chown: {
            result = Filesystem::Chown((char*)arg1, arg2, arg3);
            break;
        }
        case felix86_x86_64_lchown: {
            result = Filesystem::LChown((char*)arg1, arg2, arg3);
            break;
        }
        case felix86_x86_64_access: {
            result = Filesystem::FAccessAt(AT_FDCWD, (char*)arg1, (int)arg2, 0);
            break;
        }
        case felix86_x86_64_pipe: {
            result = ::pipe((int*)arg1);
            break;
        }
        case felix86_x86_64_mkdir: {
            result = Filesystem::MkdirAt(AT_FDCWD, (char*)arg1, arg2);
            break;
        }
        case felix86_x86_64_open: {
            result = Filesystem::OpenAt(AT_FDCWD, (char*)arg1, (int)arg2, arg3);
            break;
        }
        case felix86_x86_64_alarm: {
            result = ::alarm(arg1);
            break;
        }
        case felix86_x86_64_unlink: {
            result = Filesystem::UnlinkAt(AT_FDCWD, (char*)arg1, 0);
            break;
        }
        case felix86_x86_64_stat: {
            result = Filesystem::FStatAt(AT_FDCWD, (char*)arg1, (x86_stat*)arg2, 0);
            break;
        }
        case felix86_x86_64_rmdir: {
            result = Filesystem::Rmdir((char*)arg1);
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

    if (g_config.strace) {
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

void felix86_syscall32(ThreadState* state, u32 rip_next) {
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
        result = felix86_syscall_common(state, rv_syscall, arg1, arg2, arg3, arg4, arg5, arg6);
    } else {
        switch (syscall_number) {
        case felix86_x86_32_alarm: {
            result = ::alarm(arg1);
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
        case felix86_x86_32_rename: {
            result = Filesystem::Rename((char*)arg1, (char*)arg2);
            break;
        }
        case felix86_x86_32_mkdir: {
            result = Filesystem::MkdirAt(AT_FDCWD, (char*)arg1, arg2);
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
        case felix86_x86_32_writev: {
            x86_iovec* iovecs32 = (x86_iovec*)arg2;
            std::vector<iovec> iovecs(iovecs32, iovecs32 + arg3);
            result = SYSCALL(writev, arg1, iovecs.data(), arg3);
            break;
        }
        case felix86_x86_32_mmap_pgoff: {
            // mmap2 is like mmap but file offset is in pages (4096 bytes) to help with the lack of big enough integers in x86-32
            u64 offset = arg6 * 4096;
            state->signals_disabled = true;
            result = (ssize_t)g_mapper->map((void*)arg1, arg2, arg3, arg4, arg5, offset);
            state->signals_disabled = false;
            break;
        }
        case felix86_x86_32_mremap: {
            state->signals_disabled = true;
            result = (ssize_t)g_mapper->remap32((void*)arg1, arg2, arg3, arg4, (void*)arg5);
            state->signals_disabled = false;
            break;
        }
        case felix86_x86_32_open: {
            result = Filesystem::OpenAt(AT_FDCWD, (char*)arg1, (int)arg2, arg3);
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

            // These are the only valid entries in x86 64-bit kernel
            if (index < 12 || index > 12 + 3) {
                result = -EINVAL;
                break;
            }

            *udesc = {};

            udesc->base_addr = state->gdt[index - 12];

            if (udesc->base_addr != 0) {
                udesc->limit = 0xFFFFF;
                udesc->seg_32bit = 1;
                udesc->limit_in_pages = 1;
                udesc->usable = 1;
            } else {
                udesc->read_exec_only = 1;
                udesc->seg_not_present = 1;
            }

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
        case felix86_x86_32_access: {
            result = Filesystem::FAccessAt(AT_FDCWD, (char*)arg1, (int)arg2, 0);
            break;
        }
        case felix86_x86_32_unlink: {
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
            case F_GETFL: {
                result = ::fcntl(arg1, arg2, arg3);
                break;
            }
            default: {
                WARN("Unknown fcntl: %d", arg2);
                result = -EINVAL;
                break;
            }
            }
            break;
        }
        case felix86_x86_32_waitpid: {
            result = ::waitpid((pid_t)arg1, (int*)arg2, (int)arg3);
            break;
        }
        case felix86_x86_32_statfs64: {
            ASSERT(arg2 == sizeof(x86_statfs64));

            struct statfs statfs;
            x86_statfs64* guest_statfs = (x86_statfs64*)arg3;
            result = Filesystem::StatFs((char*)arg1, &statfs);
            if (result >= 0) {
                *guest_statfs = statfs;
            }
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
        case felix86_x86_32_readlink: {
            if (arg1 == arg2) {
                WARN("arg1 == arg2 during readlink");
            }
            result = Filesystem::ReadlinkAt(AT_FDCWD, (char*)arg1, (char*)arg2, (int)arg3);
            break;
        }
        case felix86_x86_32_ipc: {
            result = ipc32(arg1, arg2, arg3, arg4, (void*)arg5, arg6);
            break;
        }
        case felix86_x86_32_stat64: {
            result = Filesystem::FStatAt(AT_FDCWD, (char*)arg1, (x86_stat*)arg2, 0);
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
        case felix86_x86_32_futex_time32: {
            const x86_timespec* guest_spec = (x86_timespec*)arg4;
            if (guest_spec) {
                const timespec host_spec = *guest_spec;
                result = SYSCALL(futex, arg1, arg2, arg3, &host_spec, arg4, arg5);
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
        case felix86_x86_32_dup2: {
            result = ::dup2(arg1, arg2);
            break;
        }
        case felix86_x86_32_utimensat_time32: {
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
                result = ::bind(args[0], (sockaddr*)(u64)args[1], args[2]);
                break;
            }
            case SYS_CONNECT: {
                result = ::connect(args[0], (sockaddr*)(u64)args[1], args[2]);
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

    if (g_config.strace) {
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