#include <errno.h>
#include <unistd.h>
#include "felix86/common/log.hpp"
#include "felix86/common/x86.hpp"
#include "felix86/emulator.hpp"
#include "felix86/hle/filesystem.hpp"
#include "felix86/hle/syscall.hpp"

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
#if defined(__x86_64__)
    return syscall;
#elif defined(__riscv)
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
#else
#error "What are you trying to compile on!?"
#endif
}

#ifdef __x86_64__
static_assert(match_host(felix86_x86_64_setxattr) == felix86_x86_64_setxattr);
#elif defined(__riscv)
static_assert(match_host(felix86_x86_64_setxattr) == felix86_riscv64_setxattr);
#endif

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

void felix86_syscall(Emulator* emulator, ThreadState* state) {
    u64 syscall_number = state->GetGpr(X86_REF_RAX);
    u64 rdi = state->GetGpr(X86_REF_RDI);
    u64 rsi = state->GetGpr(X86_REF_RSI);
    u64 rdx = state->GetGpr(X86_REF_RDX);
    u64 r10 = state->GetGpr(X86_REF_R10);
    u64 r8 = state->GetGpr(X86_REF_R8);
    u64 r9 = state->GetGpr(X86_REF_R9);
    u64 result = 0;

    Filesystem& fs = emulator->GetFilesystem();

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
            break;
        }
        case felix86_x86_64_ARCH_SET_FS: {
            state->fsbase = rsi;
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
        state->robust_futex_list = rdi;

        if (rsi != sizeof(u64) * 3) {
            WARN("Struct size is wrong during set_robust_list");
            result = EINVAL;
        }
        STRACE("set_robust_list(%016lx, %016lx) = %016lx", rdi, rsi, result);
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