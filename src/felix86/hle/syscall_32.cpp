// The painful part of not allocating the address space at 0-0xFFFF'FFFF is that syscalls that use pointers
// need those pointers to be offset by the g_address_space_base.
// For any syscalls that don't have pointers we just call the other function

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
#include "felix86/common/debug.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/state.hpp"
#include "felix86/emulator.hpp"
#include "felix86/hle/filesystem.hpp"
#include "felix86/hle/stat.hpp"
#include "felix86/hle/syscall.hpp"
#include "felix86/hle/thread.hpp"

u64 to_ptr(u64 data) {
    if (data == 0) {
        // If it's null, let it be null as some syscalls
        // have special behavior when a ptr is null
        return 0;
    } else {
        return data + g_address_space_base;
    }
}

void felix86_syscall(ThreadState* state) {
    u64 syscall_number = state->GetGpr(X86_REF_RAX);
    u64 rdi = state->GetGpr(X86_REF_RDI);
    u64 rsi = state->GetGpr(X86_REF_RSI);
    u64 rdx = state->GetGpr(X86_REF_RDX);
    u64 r10 = state->GetGpr(X86_REF_R10);
    u64 r8 = state->GetGpr(X86_REF_R8);
    u64 r9 = state->GetGpr(X86_REF_R9);

    u64 rdi_ptr = to_ptr(rdi);
    u64 rsi_ptr = to_ptr(rsi);
    u64 rdx_ptr = to_ptr(rdx);
    u64 r10_ptr = to_ptr(r10);
    u64 r8_ptr = to_ptr(r8);
    u64 r9_ptr = to_ptr(r9);

    switch (syscall_number) {
    case felix86_x86_64_brk:
    case felix86_x86_64_arch_prctl:
    case felix86_x86_64_set_robust_list:
    case felix86_x86_64_rseq:
    case felix86_x86_64_prlimit64:
    case felix86_x86_64_close:
    case felix86_x86_64_shutdown:
    case felix86_x86_64_shmget:
    case felix86_x86_64_setpgid:
    case felix86_x86_64_setpriority:
    case felix86_x86_64_getpriority:
    case felix86_x86_64_getpgrp:
    case felix86_x86_64_epoll_create1:
    case felix86_x86_64_sched_getscheduler:
    case felix86_x86_64_dup:
    case felix86_x86_64_dup2:
    case felix86_x86_64_dup3:
    case felix86_x86_64_fsync:
    case felix86_x86_64_sync:
    case felix86_x86_64_syncfs:
    case felix86_x86_64_fcntl:
    case felix86_x86_64_fchown:
    case felix86_x86_64_fchdir:
    case felix86_x86_64_exit_group:
    case felix86_x86_64_tgkill:
    case felix86_x86_64_kill:
    case felix86_x86_64_getuid:
    case felix86_x86_64_fdatasync:
    case felix86_x86_64_getegid:
    case felix86_x86_64_getgid:
    case felix86_x86_64_setfsgid:
    case felix86_x86_64_getppid:
    case felix86_x86_64_getpid:
    case felix86_x86_64_gettid:
    case felix86_x86_64_socket:
    case felix86_x86_64_alarm:
    case felix86_x86_64_lseek:
    case felix86_x86_64_timerfd_create:
    case felix86_x86_64_exit:
    case felix86_x86_64_vfork:
    case felix86_x86_64_eventfd2:
    case felix86_x86_64_fchmod:
    case felix86_x86_64_flock:
    case felix86_x86_64_sched_yield:
    case felix86_x86_64_inotify_init1:
    case felix86_x86_64_inotify_rm_watch:
    case felix86_x86_64_fallocate:
    case felix86_x86_64_sched_get_priority_min:
    case felix86_x86_64_sched_get_priority_max:
    case felix86_x86_64_clone3: /* just returns -ENOSYS */
    case felix86_x86_64_umask: {
        return felix86_syscall(state);
    }
    }
}