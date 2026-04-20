#include <dirent.h>
#include <linux/audit.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <sys/wait.h>
#include "felix86/common/feature.hpp"
#include "felix86/common/log.hpp"
#include "felix86/hle/fd.hpp"
#include "felix86/hle/ptrace.hpp"
#include "felix86/hle/signals.hpp"
#include "felix86/v2/recompiler.hpp"

struct x64_user_regs_struct {
    u64 r15;
    u64 r14;
    u64 r13;
    u64 r12;
    u64 rbp;
    u64 rbx;
    u64 r11;
    u64 r10;
    u64 r9;
    u64 r8;
    u64 rax;
    u64 rcx;
    u64 rdx;
    u64 rsi;
    u64 rdi;
    u64 orig_rax;
    u64 rip;
    u64 cs;
    u64 eflags;
    u64 rsp;
    u64 ss;
    u64 fs_base;
    u64 gs_base;
    u64 ds;
    u64 es;
    u64 fs;
    u64 gs;
};

struct x86_user_regs_struct {
    u32 ebx;
    u32 ecx;
    u32 edx;
    u32 esi;
    u32 edi;
    u32 ebp;
    u32 eax;
    u32 xds;
    u32 xes;
    u32 xfs;
    u32 xgs;
    u32 orig_eax;
    u32 eip;
    u32 xcs;
    u32 eflags;
    u32 esp;
    u32 xss;
};

struct riscv_user_regs_struct {
    unsigned long pc;
    unsigned long ra;
    unsigned long sp;
    unsigned long gp;
    unsigned long tp;
    unsigned long t0;
    unsigned long t1;
    unsigned long t2;
    unsigned long s0;
    unsigned long s1;
    unsigned long a0;
    unsigned long a1;
    unsigned long a2;
    unsigned long a3;
    unsigned long a4;
    unsigned long a5;
    unsigned long a6;
    unsigned long a7;
    unsigned long s2;
    unsigned long s3;
    unsigned long s4;
    unsigned long s5;
    unsigned long s6;
    unsigned long s7;
    unsigned long s8;
    unsigned long s9;
    unsigned long s10;
    unsigned long s11;
    unsigned long t3;
    unsigned long t4;
    unsigned long t5;
    unsigned long t6;
};

struct x86_ptrace_syscall_info {
    u8 op;                   /* Type of system call stop */
    u8 reserved;             /* Reserved for future use, must be zero */
    u16 flags;               /* Reserved for future use, must be zero */
    u32 arch;                /* AUDIT_ARCH_* value; see seccomp(2) */
    u64 instruction_pointer; /* CPU instruction pointer */
    u64 stack_pointer;       /* CPU stack pointer */
    union {
        struct {         /* op == PTRACE_SYSCALL_INFO_ENTRY */
            u64 nr;      /* System call number */
            u64 args[6]; /* System call arguments */
        } entry;
        struct {         /* op == PTRACE_SYSCALL_INFO_EXIT */
            i64 rval;    /* System call return value */
            u8 is_error; /* System call error flag;
                              Boolean: does rval contain
                              an error value (-ERRCODE) or
                              a nonerror return value? */
        } exit;
        struct {          /* op == PTRACE_SYSCALL_INFO_SECCOMP */
            u64 nr;       /* System call number */
            u64 args[6];  /* System call arguments */
            u32 ret_data; /* SECCOMP_RET_DATA portion
                               of SECCOMP_RET_TRACE
                               return value */
        } seccomp;
    };
};

template <x86_ref_e ref>
constexpr int reg_index = ref - X86_REF_RAX;

static const char* stop_to_string(StopType type) {
    switch (type) {
    case StopType::SignalDeliveryStop:
        return "signal-delivery-stop";
    case StopType::SyscallEnterStop:
        return "syscall-enter-stop";
    case StopType::SyscallExitStop:
        return "syscall-exit-stop";
    case StopType::EventStop:
        return "event-stop";
    default:
        return "unknown stop";
    }
}

static const char* guest_op_to_string(felix86_ptrace_request op) {
#define OPERATIONS                                                                                                                                   \
    X(PTRACE_TRACEME)                                                                                                                                \
    X(PTRACE_PEEKTEXT)                                                                                                                               \
    X(PTRACE_PEEKDATA)                                                                                                                               \
    X(PTRACE_PEEKUSER)                                                                                                                               \
    X(PTRACE_POKETEXT)                                                                                                                               \
    X(PTRACE_POKEDATA)                                                                                                                               \
    X(PTRACE_POKEUSER)                                                                                                                               \
    X(PTRACE_CONT)                                                                                                                                   \
    X(PTRACE_KILL)                                                                                                                                   \
    X(PTRACE_SINGLESTEP)                                                                                                                             \
    X(PTRACE_GETREGS)                                                                                                                                \
    X(PTRACE_SETREGS)                                                                                                                                \
    X(PTRACE_GETFPREGS)                                                                                                                              \
    X(PTRACE_SETFPREGS)                                                                                                                              \
    X(PTRACE_ATTACH)                                                                                                                                 \
    X(PTRACE_DETACH)                                                                                                                                 \
    X(PTRACE_GETFPXREGS)                                                                                                                             \
    X(PTRACE_SETFPXREGS)                                                                                                                             \
    X(PTRACE_SYSCALL)                                                                                                                                \
    X(PTRACE_GET_THREAD_AREA)                                                                                                                        \
    X(PTRACE_SET_THREAD_AREA)                                                                                                                        \
    X(PTRACE_ARCH_PRCTL)                                                                                                                             \
    X(PTRACE_SYSEMU)                                                                                                                                 \
    X(PTRACE_SYSEMU_SINGLESTEP)                                                                                                                      \
    X(PTRACE_SINGLEBLOCK)                                                                                                                            \
    X(PTRACE_SETOPTIONS)                                                                                                                             \
    X(PTRACE_GETEVENTMSG)                                                                                                                            \
    X(PTRACE_GETSIGINFO)                                                                                                                             \
    X(PTRACE_SETSIGINFO)                                                                                                                             \
    X(PTRACE_GETREGSET)                                                                                                                              \
    X(PTRACE_SETREGSET)                                                                                                                              \
    X(PTRACE_SEIZE)                                                                                                                                  \
    X(PTRACE_INTERRUPT)                                                                                                                              \
    X(PTRACE_LISTEN)                                                                                                                                 \
    X(PTRACE_PEEKSIGINFO)                                                                                                                            \
    X(PTRACE_GETSIGMASK)                                                                                                                             \
    X(PTRACE_SETSIGMASK)                                                                                                                             \
    X(PTRACE_SECCOMP_GET_FILTER)                                                                                                                     \
    X(PTRACE_SECCOMP_GET_METADATA)                                                                                                                   \
    X(PTRACE_GET_SYSCALL_INFO)                                                                                                                       \
    X(PTRACE_GET_RSEQ_CONFIGURATION)                                                                                                                 \
    X(PTRACE_SET_SYSCALL_USER_DISPATCH_CONFIG)                                                                                                       \
    X(PTRACE_GET_SYSCALL_USER_DISPATCH_CONFIG)
#define X(name)                                                                                                                                      \
    case felix86_ptrace_request::felix86_##name:                                                                                                     \
        return #name;
    switch (op) {
    default: {
        return "Unknown!";
    }
        OPERATIONS
    }
#undef X
#undef OPERATIONS
}

static const char* op_to_string(__ptrace_request op) {
// These are the ptrace operations defined on RISC-V
// Some may not be actually available, such as PTRACE_PEEKUSER
#define OPERATIONS                                                                                                                                   \
    X(PTRACE_TRACEME)                                                                                                                                \
    X(PTRACE_PEEKTEXT)                                                                                                                               \
    X(PTRACE_PEEKDATA)                                                                                                                               \
    X(PTRACE_PEEKUSER)                                                                                                                               \
    X(PTRACE_POKETEXT)                                                                                                                               \
    X(PTRACE_POKEDATA)                                                                                                                               \
    X(PTRACE_POKEUSER)                                                                                                                               \
    X(PTRACE_CONT)                                                                                                                                   \
    X(PTRACE_KILL)                                                                                                                                   \
    X(PTRACE_SINGLESTEP)                                                                                                                             \
    X(PTRACE_ATTACH)                                                                                                                                 \
    X(PTRACE_DETACH)                                                                                                                                 \
    X(PTRACE_SYSCALL)                                                                                                                                \
    X(PTRACE_SETOPTIONS)                                                                                                                             \
    X(PTRACE_GETEVENTMSG)                                                                                                                            \
    X(PTRACE_GETSIGINFO)                                                                                                                             \
    X(PTRACE_SETSIGINFO)                                                                                                                             \
    X(PTRACE_GETREGSET)                                                                                                                              \
    X(PTRACE_SETREGSET)                                                                                                                              \
    X(PTRACE_SEIZE)                                                                                                                                  \
    X(PTRACE_INTERRUPT)                                                                                                                              \
    X(PTRACE_LISTEN)                                                                                                                                 \
    X(PTRACE_PEEKSIGINFO)                                                                                                                            \
    X(PTRACE_GETSIGMASK)                                                                                                                             \
    X(PTRACE_SETSIGMASK)                                                                                                                             \
    X(PTRACE_SECCOMP_GET_FILTER)                                                                                                                     \
    X(PTRACE_SECCOMP_GET_METADATA)                                                                                                                   \
    X(PTRACE_GET_SYSCALL_INFO)                                                                                                                       \
    X(PTRACE_GET_RSEQ_CONFIGURATION)                                                                                                                 \
    X(PTRACE_SET_SYSCALL_USER_DISPATCH_CONFIG)                                                                                                       \
    X(PTRACE_GET_SYSCALL_USER_DISPATCH_CONFIG)
#define X(num)                                                                                                                                       \
    case num:                                                                                                                                        \
        return #num;
    switch (op) {
    default: {
        return "Unknown!";
    }
        OPERATIONS
    }
#undef X
#undef OPERATIONS
}

// ptrace libc function has some very unfortunate differences with the syscall, make sure we never use it directly
#ifdef ptrace
#undef ptrace
#endif
#pragma GCC poison ptrace
static int __ptrace(__ptrace_request op, pid_t pid, void* addr, void* data) {
    HOSTPTRACELOG("Running ptrace command %s on %d with addr=%lx and data=%lx", op_to_string(op), pid, addr, data);
    return ::syscall(SYS_ptrace, op, pid, addr, data);
}

void* get_remote_state(pid_t pid) {
    riscv_user_regs_struct regs;
    struct iovec io;
    io.iov_base = &regs;
    io.iov_len = sizeof(regs);
    int result = __ptrace(PTRACE_GETREGSET, pid, (void*)1, &io);
    int error = errno;
    if (result == -1 && error == ESRCH) {
        HOSTPTRACELOG("Tried to get remote page of %d but it is not stopped", pid);
        return nullptr;
    }
    ASSERT_MSG(result == 0, "PTRACE_GETREGSET returned %d, errno: %d", result, error);
    ASSERT(io.iov_len == sizeof(regs));
    return (void*)regs.gp;
}

void get_remote_ctx(UserContext* remote_ctx, PtracePage* remote_page, pid_t pid) {
    iovec local, remote;
    local.iov_base = remote_ctx;
    local.iov_len = sizeof(UserContext);
    remote.iov_base = (u8*)remote_page->constants.state + offsetof(ThreadState, ctx);
    remote.iov_len = sizeof(UserContext);
    int result = syscall(SYS_process_vm_readv, pid, &local, 1, &remote, 1, 0);
    ASSERT(result == sizeof(UserContext));
}

void set_remote_ctx(UserContext* remote_ctx, PtracePage* remote_page, pid_t pid) {
    iovec local, remote;
    local.iov_base = remote_ctx;
    local.iov_len = sizeof(UserContext);
    remote.iov_base = (u8*)remote_page->constants.state + offsetof(ThreadState, ctx);
    remote.iov_len = sizeof(UserContext);
    int result = syscall(SYS_process_vm_writev, pid, &local, 1, &remote, 1, 0);
    ASSERT(result == sizeof(UserContext));
}

int get_regs(bool tracer_mode32, PtracePage* remote_page, pid_t pid, void* data) {
    UserContext remote_ctx;
    get_remote_ctx(&remote_ctx, remote_page, pid);
    if (tracer_mode32) {
        x86_user_regs_struct* user = (x86_user_regs_struct*)data;
        user->eax = remote_ctx.gprs[X86_REF_RAX];
        user->ecx = remote_ctx.gprs[X86_REF_RCX];
        user->edx = remote_ctx.gprs[X86_REF_RDX];
        user->ebx = remote_ctx.gprs[X86_REF_RBX];
        user->esp = remote_ctx.gprs[X86_REF_RSP];
        user->ebp = remote_ctx.gprs[X86_REF_RBP];
        user->esi = remote_ctx.gprs[X86_REF_RSI];
        user->edi = remote_ctx.gprs[X86_REF_RDI];
        user->eip = remote_ctx.rip;
        user->xcs = remote_ctx.cs;
        user->xds = remote_ctx.ds;
        user->xss = remote_ctx.ss;
        user->xes = remote_ctx.es;
        user->xfs = remote_ctx.fs;
        user->xgs = remote_ctx.gs;
        user->eflags = remote_ctx.GetFlags();
        user->orig_eax = remote_ctx.orig_rax;
        return 0;
    } else {
        x64_user_regs_struct* user = (x64_user_regs_struct*)data;
        user->rax = remote_ctx.gprs[X86_REF_RAX];
        user->rcx = remote_ctx.gprs[X86_REF_RCX];
        user->rdx = remote_ctx.gprs[X86_REF_RDX];
        user->rbx = remote_ctx.gprs[X86_REF_RBX];
        user->rsp = remote_ctx.gprs[X86_REF_RSP];
        user->rbp = remote_ctx.gprs[X86_REF_RBP];
        user->rsi = remote_ctx.gprs[X86_REF_RSI];
        user->rdi = remote_ctx.gprs[X86_REF_RDI];
        user->r8 = remote_ctx.gprs[X86_REF_R8];
        user->r9 = remote_ctx.gprs[X86_REF_R9];
        user->r10 = remote_ctx.gprs[X86_REF_R10];
        user->r11 = remote_ctx.gprs[X86_REF_R11];
        user->r12 = remote_ctx.gprs[X86_REF_R12];
        user->r13 = remote_ctx.gprs[X86_REF_R13];
        user->r14 = remote_ctx.gprs[X86_REF_R14];
        user->r15 = remote_ctx.gprs[X86_REF_R15];
        user->rip = remote_ctx.rip;
        user->cs = remote_ctx.cs;
        user->ds = remote_ctx.ds;
        user->ss = remote_ctx.ss;
        user->es = remote_ctx.es;
        user->fs = remote_ctx.fs;
        user->gs = remote_ctx.gs;
        user->fs_base = remote_ctx.fsbase;
        user->gs_base = remote_ctx.gsbase;
        user->eflags = remote_ctx.GetFlags();
        user->orig_rax = remote_ctx.orig_rax;
        return 0;
    }
}

int set_regs(bool tracer_mode32, PtracePage* remote_page, pid_t pid, void* data) {
    UserContext remote_ctx;
    get_remote_ctx(&remote_ctx, remote_page, pid);
    if (tracer_mode32) {
        x86_user_regs_struct* user = (x86_user_regs_struct*)data;
        remote_ctx.gprs[X86_REF_RAX] = user->eax;
        remote_ctx.gprs[X86_REF_RCX] = user->ecx;
        remote_ctx.gprs[X86_REF_RDX] = user->edx;
        remote_ctx.gprs[X86_REF_RBX] = user->ebx;
        remote_ctx.gprs[X86_REF_RSP] = user->esp;
        remote_ctx.gprs[X86_REF_RBP] = user->ebp;
        remote_ctx.gprs[X86_REF_RSI] = user->esi;
        remote_ctx.gprs[X86_REF_RDI] = user->edi;
        remote_ctx.rip = user->eip;
        remote_ctx.cs = user->xcs;
        remote_ctx.ds = user->xds;
        remote_ctx.ss = user->xss;
        remote_ctx.es = user->xes;
        remote_ctx.fs = user->xfs;
        remote_ctx.gs = user->xgs;
        remote_ctx.orig_rax = user->orig_eax;
        remote_ctx.SetFlags(user->eflags);
    } else {
        x64_user_regs_struct* user = (x64_user_regs_struct*)data;
        remote_ctx.gprs[X86_REF_RAX] = user->rax;
        remote_ctx.gprs[X86_REF_RCX] = user->rcx;
        remote_ctx.gprs[X86_REF_RDX] = user->rdx;
        remote_ctx.gprs[X86_REF_RBX] = user->rbx;
        remote_ctx.gprs[X86_REF_RSP] = user->rsp;
        remote_ctx.gprs[X86_REF_RBP] = user->rbp;
        remote_ctx.gprs[X86_REF_RSI] = user->rsi;
        remote_ctx.gprs[X86_REF_RDI] = user->rdi;
        remote_ctx.gprs[X86_REF_R8] = user->r8;
        remote_ctx.gprs[X86_REF_R9] = user->r9;
        remote_ctx.gprs[X86_REF_R10] = user->r10;
        remote_ctx.gprs[X86_REF_R11] = user->r11;
        remote_ctx.gprs[X86_REF_R12] = user->r12;
        remote_ctx.gprs[X86_REF_R13] = user->r13;
        remote_ctx.gprs[X86_REF_R14] = user->r14;
        remote_ctx.gprs[X86_REF_R15] = user->r15;
        remote_ctx.rip = user->rip;
        remote_ctx.cs = user->cs;
        remote_ctx.ds = user->ds;
        remote_ctx.ss = user->ss;
        remote_ctx.es = user->es;
        remote_ctx.fs = user->fs;
        remote_ctx.gs = user->gs;
        remote_ctx.fsbase = user->fs_base;
        remote_ctx.gsbase = user->gs_base;
        remote_ctx.orig_rax = user->orig_rax;
        remote_ctx.SetFlags(user->eflags);
    }
    set_remote_ctx(&remote_ctx, remote_page, pid);
    return 0;
}

u64 user_to_threadstate_offset(int offset, bool mode32) {
    const int debug_register_size = mode32 ? sizeof(u32) : sizeof(u64);
    u64 debug_register_start = mode32 ? 252 : 848;
    u64 debug_register_end = debug_register_start + debug_register_size * 8;
    if (offset >= debug_register_start && offset < debug_register_end) {
        int index = (offset - debug_register_start) / debug_register_size;
        switch (index) {
        case 0 ... 3: {
            return offsetof(ThreadState, ctx.debug_register) + index * debug_register_size;
        }
        case 4: {
            WARN("Accessing debug register 4, this shouldn't happen");
            [[fallthrough]];
        }
        case 6: {
            return offsetof(ThreadState, ctx.debug_status);
        }
        case 5: {
            WARN("Accessing debug register 5, this shouldn't happen");
            [[fallthrough]];
        }
        case 7: {
            return offsetof(ThreadState, ctx.debug_control);
        }
        default: {
            UNREACHABLE();
        }
        }
    }

    if (mode32) {
        switch (offset) {
        case offsetof(x86_user_regs_struct, ebx): {
            return offsetof(ThreadState, ctx.gprs) + reg_index<X86_REF_RBX> * sizeof(u64);
        }
        case offsetof(x86_user_regs_struct, ecx): {
            return offsetof(ThreadState, ctx.gprs) + reg_index<X86_REF_RCX> * sizeof(u64);
        }
        case offsetof(x86_user_regs_struct, edx): {
            return offsetof(ThreadState, ctx.gprs) + reg_index<X86_REF_RDX> * sizeof(u64);
        }
        case offsetof(x86_user_regs_struct, esi): {
            return offsetof(ThreadState, ctx.gprs) + reg_index<X86_REF_RSI> * sizeof(u64);
        }
        case offsetof(x86_user_regs_struct, edi): {
            return offsetof(ThreadState, ctx.gprs) + reg_index<X86_REF_RDI> * sizeof(u64);
        }
        case offsetof(x86_user_regs_struct, ebp): {
            return offsetof(ThreadState, ctx.gprs) + reg_index<X86_REF_RBP> * sizeof(u64);
        }
        case offsetof(x86_user_regs_struct, esp): {
            return offsetof(ThreadState, ctx.gprs) + reg_index<X86_REF_RSP> * sizeof(u64);
        }
        case offsetof(x86_user_regs_struct, orig_eax): {
            return offsetof(ThreadState, ctx.orig_rax);
        }
        case offsetof(x86_user_regs_struct, eax): {
            return offsetof(ThreadState, ctx.gprs) + reg_index<X86_REF_RAX> * sizeof(u64);
        }
        case offsetof(x86_user_regs_struct, eip): {
            return offsetof(ThreadState, ctx.rip);
        }
        // TODO: test these
        case offsetof(x86_user_regs_struct, xds): {
            return offsetof(ThreadState, ctx.ds);
        }
        case offsetof(x86_user_regs_struct, xss): {
            return offsetof(ThreadState, ctx.ss);
        }
        case offsetof(x86_user_regs_struct, xcs): {
            return offsetof(ThreadState, ctx.cs);
        }
        case offsetof(x86_user_regs_struct, xgs): {
            return offsetof(ThreadState, ctx.gs);
        }
        case offsetof(x86_user_regs_struct, xfs): {
            return offsetof(ThreadState, ctx.fs);
        }
        case offsetof(x86_user_regs_struct, xes): {
            return offsetof(ThreadState, ctx.es);
        }
        case offsetof(x86_user_regs_struct, eflags): {
            // Handled specially
            UNREACHABLE();
            return 0;
        }
        default: {
            UNREACHABLE();
            return 0;
        }
        }
    } else {
        switch (offset) {
        case offsetof(x64_user_regs_struct, rax): {
            return offsetof(ThreadState, ctx.gprs) + reg_index<X86_REF_RAX> * sizeof(u64);
        }
        case offsetof(x64_user_regs_struct, orig_rax): {
            return offsetof(ThreadState, ctx.orig_rax);
        }
        case offsetof(x64_user_regs_struct, rcx): {
            return offsetof(ThreadState, ctx.gprs) + reg_index<X86_REF_RCX> * sizeof(u64);
        }
        case offsetof(x64_user_regs_struct, rdx): {
            return offsetof(ThreadState, ctx.gprs) + reg_index<X86_REF_RDX> * sizeof(u64);
        }
        case offsetof(x64_user_regs_struct, rbx): {
            return offsetof(ThreadState, ctx.gprs) + reg_index<X86_REF_RBX> * sizeof(u64);
        }
        case offsetof(x64_user_regs_struct, rsp): {
            return offsetof(ThreadState, ctx.gprs) + reg_index<X86_REF_RSP> * sizeof(u64);
        }
        case offsetof(x64_user_regs_struct, rbp): {
            return offsetof(ThreadState, ctx.gprs) + reg_index<X86_REF_RBP> * sizeof(u64);
        }
        case offsetof(x64_user_regs_struct, rsi): {
            return offsetof(ThreadState, ctx.gprs) + reg_index<X86_REF_RSI> * sizeof(u64);
        }
        case offsetof(x64_user_regs_struct, rdi): {
            return offsetof(ThreadState, ctx.gprs) + reg_index<X86_REF_RDI> * sizeof(u64);
        }
        case offsetof(x64_user_regs_struct, r8): {
            return offsetof(ThreadState, ctx.gprs) + reg_index<X86_REF_R8> * sizeof(u64);
        }
        case offsetof(x64_user_regs_struct, r9): {
            return offsetof(ThreadState, ctx.gprs) + reg_index<X86_REF_R9> * sizeof(u64);
        }
        case offsetof(x64_user_regs_struct, r10): {
            return offsetof(ThreadState, ctx.gprs) + reg_index<X86_REF_R10> * sizeof(u64);
        }
        case offsetof(x64_user_regs_struct, r11): {
            return offsetof(ThreadState, ctx.gprs) + reg_index<X86_REF_R11> * sizeof(u64);
        }
        case offsetof(x64_user_regs_struct, r12): {
            return offsetof(ThreadState, ctx.gprs) + reg_index<X86_REF_R12> * sizeof(u64);
        }
        case offsetof(x64_user_regs_struct, r13): {
            return offsetof(ThreadState, ctx.gprs) + reg_index<X86_REF_R13> * sizeof(u64);
        }
        case offsetof(x64_user_regs_struct, r14): {
            return offsetof(ThreadState, ctx.gprs) + reg_index<X86_REF_R14> * sizeof(u64);
        }
        case offsetof(x64_user_regs_struct, r15): {
            return offsetof(ThreadState, ctx.gprs) + reg_index<X86_REF_R15> * sizeof(u64);
        }
        case offsetof(x64_user_regs_struct, rip): {
            return offsetof(ThreadState, ctx.rip);
        }
        case offsetof(x64_user_regs_struct, es): {
            return offsetof(ThreadState, ctx.es);
        }
        case offsetof(x64_user_regs_struct, ds): {
            return offsetof(ThreadState, ctx.ds);
        }
        case offsetof(x64_user_regs_struct, cs): {
            return offsetof(ThreadState, ctx.cs);
        }
        case offsetof(x64_user_regs_struct, gs): {
            return offsetof(ThreadState, ctx.gs);
        }
        case offsetof(x64_user_regs_struct, fs): {
            return offsetof(ThreadState, ctx.fs);
        }
        case offsetof(x64_user_regs_struct, ss): {
            return offsetof(ThreadState, ctx.ss);
        }
        case offsetof(x64_user_regs_struct, gs_base): {
            return offsetof(ThreadState, ctx.gsbase);
        }
        case offsetof(x64_user_regs_struct, fs_base): {
            return offsetof(ThreadState, ctx.fsbase);
        }
        case offsetof(x64_user_regs_struct, eflags): {
            // Handled specially
            UNREACHABLE();
            return 0;
        }
        default: {
            UNREACHABLE();
            return 0;
        }
        }
    }
}

namespace Ptrace {
int wait4(pid_t pid, int* status, int flags, struct rusage* ru) {
    HOSTPTRACELOG("%d is waiting on %d", gettid(), pid);
    int host_status;
    int result;
    bool first_synchronous = false;
    while (true) {
        result = syscall(SYS_wait4, pid, &host_status, flags, ru);
        if (result == -1) {
            // If something like -EINTR is returned, the syscall will be restarted guest side, if necessary, so we don't restart it here
            break; // NOTE: felix86_syscall will get the actual value through errno
        } else if (result > 0) {
            if (pid == -1) {
                pid = result;
            } else {
                ASSERT(pid == result);
            }
            if (WIFEXITED(host_status)) {
                HOSTPTRACELOG("%d exited", pid);
                break;
            } else if (WIFSIGNALED(host_status)) {
                HOSTPTRACELOG("%d signalled with termination signal: %d", pid, WTERMSIG(host_status));
                break;
            } else if (WIFCONTINUED(host_status)) {
                HOSTPTRACELOG("%d continued", pid);
                break;
            } else if (WIFSTOPPED(host_status)) {
                int sig = WSTOPSIG(host_status);
                HOSTPTRACELOG("Host stop at pid %d, signal: %d, event: %d", pid, sig, (u8)(host_status >> 16));
                if (sig == SIGTRAP) {
                    // This may be a host execve-stop, or a clone/fork event, which we need to skip
                    // As described in syscall.cpp execve handler, right before an execve happens
                    // we set the gp register to null, so the remote state should be null
                    // When felix86 is free to initialize itself, it will raise its own execve-stop
                    // using sigptrace, so we can skip this one. Same deal for fork/clone.
                    u8 event = (host_status >> 16) & 0xFF;
                    if (event == PTRACE_EVENT_FORK || event == PTRACE_EVENT_CLONE) {
                        HOSTPTRACELOG("Skipping fork/clone event from %d", pid);
                        int result = __ptrace(PTRACE_CONT, pid, 0, 0);
                        ASSERT(result == 0);
                        continue; // go back to waiting
                    }

                    void* remote_state = get_remote_state(pid);
                    if (!remote_state) {
                        siginfo_t tracee_siginfo;
                        result = __ptrace(PTRACE_GETSIGINFO, pid, 0, &tracee_siginfo);
                        ASSERT(result == 0);
                        if (tracee_siginfo.si_code != SI_USER) {
                            ERROR("Unexpected si_code: %d, full status: %x", tracee_siginfo.si_code, host_status);
                        } else {
                            HOSTPTRACELOG("Skipping execve-stop from %d", pid);
                            int result = __ptrace(PTRACE_CONT, pid, 0, 0);
                            ASSERT(result == 0);
                            continue; // go back to waiting
                        }
                    }
                }

                if (sig == SIGSTOP) {
                    // This may be the signal from forking/vforking/cloning, which we want to skip and re-raise as SIGPTRACE later
                    void* remote_state = get_remote_state(pid);
                    if (!remote_state) {
                        HOSTPTRACELOG("Skipping clone SIGSTOP on %d", pid);
                        int result = __ptrace(PTRACE_CONT, pid, 0, 0);
                        ASSERT(result == 0);
                        continue;
                    }
                }

                HOSTPTRACELOG("%d is stopped on signal %d", pid, sig);
                PtracePage* remote_page = get_remote_page(pid);
                if (!remote_page) {
                    HOSTPTRACELOG("Process %d is stopped but has no remote page", pid);
                    ASSERT(flags & WUNTRACED);
                    break;
                }

                struct UnmapPage {
                    explicit UnmapPage(PtracePage* page) : page(page) {}
                    ~UnmapPage() {
                        if (page) {
                            close_remote_page(page);
                        }
                    }
                    PtracePage* page;
                } unmapper(remote_page);

                pid_t our_pid = gettid();
                pid_t expected_pid = remote_page->constants.tracer_pid;
                if (expected_pid != our_pid) {
                    HOSTPTRACELOG("Process %d is stopped but tracer_pid (%d) is not us (%d)", pid, expected_pid, our_pid);
                    break;
                }

                bool is_signal_delivery_stop = false;
                bool is_group_stop = false;
                siginfo_t tracee_siginfo;
                int r = __ptrace(PTRACE_GETSIGINFO, pid, 0, &tracee_siginfo);
                int error = errno;
                if (sig == SIGSTOP || sig == SIGTTOU || sig == SIGTTIN || sig == SIGTSTP) {
                    if (r == 0) {
                        is_signal_delivery_stop = true;
                    } else if (r == -1) {
                        if (error == EINVAL) {
                            is_group_stop = true;
                        } else {
                            UNREACHABLE();
                        }
                    } else {
                        UNREACHABLE();
                    }
                } else {
                    is_signal_delivery_stop = true;
                    ASSERT(r == 0);
                }

                if (is_signal_delivery_stop) {
                    HOSTPTRACELOG("Tracee %d in host signal-delivery-stop from signal %d", pid, sig);
                    if (sig == FELIX86_PTRACE_SIGNAL && tracee_siginfo.si_code == FELIX86_PTRACE_CODE_INSTOP) {
                        first_synchronous = false;
                        ASSERT(remote_page->stop_info.stopped);
                        GUESTPTRACELOG("%d has been observed entering stop of type %s", pid, stop_to_string(remote_page->stop_info.type));
                        host_status = 0x7f; // Stopped
                        static_assert(WIFSTOPPED(0x7f));
                        host_status |= ((u32)remote_page->stop_info.signal & 0xFF) << 8;
                        host_status |= (u32)remote_page->stop_info.ptrace_event << 16;
                        memset(&remote_page->injected, 0, sizeof(remote_page->injected));
                        // Break out so the signal is not continued and the wait4 can return
                        break;
                    } else {
                        if (sig == FELIX86_PTRACE_SIGNAL) {
                            WARN("Tracer has observed sigptrace, this shouldn't happen unless if tracee is using sigptrace");
                        }

                        // We need to peek into ThreadState to see if the signal handler is SIG_IGN or SIG_DFL
                        // in those cases we want to inject sigptrace so that the signal will be deferred
                        // We don't capture SIG_IGN or SIG_DFL signals, we pass them to the host, it would cause wrong
                        // behavior to capture SIG_IGN if an ignored signal happens during a syscall
                        ThreadState* remote_state_ptr = remote_page->constants.state;
                        // TODO: this assumes the emulator of the tracer and the tracee is the same version and doesn't change the offset
                        u64 signal_table_ptr;
                        u64 offset = (u64)remote_state_ptr + offsetof(ThreadState, signal_table);
                        r = __ptrace(PTRACE_PEEKDATA, pid, (void*)offset, &signal_table_ptr);
                        ASSERT(r == 0);
                        u64 handler;
                        u64 handler_offset = signal_table_ptr + sizeof(RegisteredSignal) * (sig - 1) + offsetof(RegisteredSignal, func);
                        r = __ptrace(PTRACE_PEEKDATA, pid, (void*)handler_offset, &handler);
                        ASSERT(r == 0);
                        // Because we passthrough SIG_IGN and SIG_DFL to the host, we need to capture them on the tracer
                        // turn them into a different signal and defer them. The deferred signal will be handled in a safepoint
                        // and raise a signal-delivery-stop
                        // If it's synchronous we can't defer it however
                        bool is_sigign_or_sigdfl = handler == (u64)SIG_IGN || handler == (u64)SIG_DFL;
                        bool is_synchronous = tracee_siginfo.si_code > 0 && (sig == SIGSEGV || sig == SIGBUS || sig == SIGILL || sig == SIGTRAP);
                        if (!is_synchronous) {
                            first_synchronous = false;
                        }
                        if (is_sigign_or_sigdfl) {
                            if (!is_synchronous) {
                                HOSTPTRACELOG("Forcing signal %d to be deferred", sig);
                                bool already_deferred = __atomic_test_and_set(&remote_page->force_defer.deferred, __ATOMIC_SEQ_CST);
                                if (already_deferred) {
                                    // Okay, this is a possible rare occurence. If we already deferred a signal to the tracee previously
                                    // but it didn't finish its sigptrace signal handler yet, and it got another signal then we
                                    // would be overwriting original_info/original_sig here. Since we mask all signals before deferring
                                    // the only way this can happen is if it receives a SIGSTOP, which can't be masked
                                    // We currently error, but if this becomes a real problem a solution would likely to be to
                                    // discard the sigstop and force the tracee to wait after finishing (meaning wait after turning deferred to false)
                                    // then while it's waiting there send a SIGSTOP, turn off the wait condition for the tracee,
                                    // this way it will be forced to raise a signal-delivery-stop on SIGSTOP, which we will observe here
                                    // and continue as normal to defer it
                                    ASSERT(sig == SIGSTOP);
                                    ERROR("Tracer is waiting for tracee %d to finish deferring its previous signal to raise SIGSTOP", pid);
                                }

                                // We need to force the tracee to defer this signal to a safepoint
                                // SIG_IGN signals need to be injected as sigptrace because otherwise
                                // they would be ignored in the tracee side, SIG_DFL signals same reason plus
                                // the fact that the default behavior of stop would be bad to do, plus
                                // we get to change SIGSTOP. So 3 birds with 1 stone.
                                remote_page->force_defer.original_sig = sig;
                                remote_page->force_defer.original_info = tracee_siginfo;
                                u64 original_mask;
                                r = __ptrace(PTRACE_GETSIGMASK, pid, (void*)sizeof(u64), &original_mask);
                                remote_page->force_defer.original_mask = original_mask;
                                sig = FELIX86_PTRACE_SIGNAL;
                                tracee_siginfo.si_code = FELIX86_PTRACE_CODE_DEFER;
                                tracee_siginfo.si_signo = sig;
                                r = __ptrace(PTRACE_SETSIGINFO, pid, 0, &tracee_siginfo);
                                ASSERT(r == 0);
                                u64 full_mask = -1ull & ~(1ull << (FELIX86_PTRACE_SIGNAL - 1));
                                // Block signals for reasons specified in handle_sigptrace
                                // They will become unblocked at the end of the signal handler as it restores the mask from context
                                r = __ptrace(PTRACE_SETSIGMASK, pid, (void*)sizeof(u64), &full_mask);
                                ASSERT(r == 0);
                            } else {
                                // If the signal is synchronous and guest signal handler is null, it may be a safepoint
                                // or other emulator sided signal. If not, we need to break of the wait loop
                                // and let the tracer observe the signal, because we can't defer synchronous signals
                                if (first_synchronous) {
                                    HOSTPTRACELOG("Observed the same synchronous signal twice, passing through to host");
                                    break;
                                } else {
                                    // Passthrough the signal, and if it's not handled it will trigger again and we will break out
                                    first_synchronous = true;
                                }
                            }
                        } else {
                            // The signal will naturally be deferred, if necessary, on the guest side
                            // When it is deferred it will raise a sigptrace (via Ptrace::raise_stop)
                            // If synchronous it will not be deferred but it will raise the stop immediately
                        }
                    }
                } else if (is_group_stop) {
                    HOSTPTRACELOG("Tracee %d in host group-stop from signal %d", pid, sig);
                    first_synchronous = false;
                    UNIMPLEMENTED();
                } else {
                    UNREACHABLE();
                }

                // Any other synchronous or asynchronous signal we just skip. Ptrace related stops are only observed on the guest on SIG53, which
                // the tracee will raise itself when all the relevant data is prepared
                r = __ptrace(PTRACE_CONT, pid, (void*)0, (void*)(u64)sig); // inject the signal
                if (r == 0) {
                    // Go back to waiting, this stop isn't observed by guest
                    continue;
                } else if (errno == ESRCH) {
                    ERROR("Got wait status from pid %d but we aren't tracing it?", pid);
                } else {
                    ERROR("Unexpected error %d while trying to skip signal %d in %d", errno, WSTOPSIG(host_status), pid);
                }
            } else {
                UNREACHABLE();
            }
        } else if (result == 0) {
            ASSERT(flags & WNOHANG);
            break;
        } else {
            UNREACHABLE();
        }
    }

    if (status) {
        *status = host_status;
    }

    return result;
}

i64 sys_ptrace(felix86_ptrace_request op, pid_t pid, void* addr, void* data) {
    bool tracer_mode32 = ThreadState::Get()->ctx.Mode32();
    GUESTPTRACELOG("Operation %s on %d with addr=%lx and data=%lx", guest_op_to_string(op), pid, addr, data);
    PtracePage* remote_page = nullptr;
    switch (op) {
    // These are the only operations that don't need a stopped tracee
    case felix86_ptrace_request::felix86_PTRACE_TRACEME:
    case felix86_ptrace_request::felix86_PTRACE_ATTACH:
    case felix86_ptrace_request::felix86_PTRACE_SEIZE:
    case felix86_ptrace_request::felix86_PTRACE_INTERRUPT:
    case felix86_ptrace_request::felix86_PTRACE_KILL: {
        break;
    }
    default: {
        remote_page = get_remote_page(pid);
        if (!remote_page) {
            HOSTPTRACELOG("Tried to run ptrace operation %d on %d, but it doesn't have a PtracePage", op, pid);
            return -ESRCH;
        }

        if (!remote_page->stop_info.stopped) {
            // Not in an official stop, so return ESRCH and warn
            // This may be the case if the tracer doesn't properly waitpid, which should not happen in legit
            // applications. It may be the case we are actually in a "stop", but only due to host effects
            // and we don't want to run ptrace operations that require stopping at that time
            HOSTPTRACELOG("Tried to run ptrace operation %d on %d, but it is not guest stopped", op, pid);
            return -ESRCH;
        }

        if (remote_page->constants.tracer_pid != gettid()) {
            WARN("Tracer pid (%d) and our pid (%d) mismatch?", gettid(), remote_page->constants.tracer_pid);
            return -EPERM;
        }

        bool differing_mode = tracer_mode32 != remote_page->stop_info.mode32;
        if (differing_mode) {
            WARN_ONCE("Tracer %d is %s but tracee %d is %s", gettid(), tracer_mode32 ? "32-bit" : "64-bit", pid,
                      remote_page->stop_info.mode32 ? "32-bit" : "64-bit");
        }
        break;
    }
    }

    struct UnmapPage {
        explicit UnmapPage(PtracePage* page) : page(page) {}
        ~UnmapPage() {
            if (page) {
                close_remote_page(page);
            }
        }
        PtracePage* page;
    } unmapper(remote_page);

    switch (op) {
    case felix86_ptrace_request::felix86_PTRACE_TRACEME: {
        ThreadState* state = ThreadState::Get();
        if (Ptrace::is_traced(state)) {
            HOSTPTRACELOG("TRACEME run when already being traced by %d", state->ptrace_page->constants.tracer_pid);
            return -EPERM;
        }

        pid_t tracer = getppid();
        state->ptrace_page->constants.tracer_pid = tracer;
        int result = __ptrace(PTRACE_TRACEME, 0, 0, 0);
        if (result != 0) {
            int error = -errno;
            GUESTPTRACELOG("TRACEME failed with %d", errno);
            state->ptrace_page->constants.tracer_pid = 0;
            return error;
        } else {
            GUESTPTRACELOG("Process %d is now being traced by %d", gettid(), tracer);
            return result;
        }
    }
    case felix86_ptrace_request::felix86_PTRACE_ATTACH: {
        int result = __ptrace(PTRACE_ATTACH, pid, addr, data);
        if (result != 0) {
            return -errno;
        }

        // Now wait for the PTRACE_ATTACH's SIGSTOP to set the tracer_pid
        // Since we waited on it we need to re-raise it afterwards
        while (true) {
            int status;
            int r = waitpid(pid, &status, 0);
            if (r != pid) {
                WARN("Couldn't wait on %d after PTRACE_ATTACH", pid);
                return -ESRCH;
            }

            if (!WIFSTOPPED(status)) {
                WARN("Couldn't wait on %d after PTRACE_ATTACH, status: %x", pid, status);
                return -ESRCH;
            }

            if (WSTOPSIG(status) != SIGSTOP) {
                // A different signal raced, reinject it until we hit our SIGSTOP
                // This is a known limitation of PTRACE_ATTACH
                WARN("Signal %d raced us to SIGSTOP", WSTOPSIG(status));
                ASSERT(__ptrace(PTRACE_CONT, pid, 0, (void*)(u64)WSTOPSIG(status)) == 0);
                continue;
            } else {
                remote_page = get_remote_page(pid);
                ASSERT(remote_page);
                ASSERT(remote_page->constants.tracer_pid == 0);
                remote_page->constants.tracer_pid = gettid();
                // Re-raise the SIGSTOP so the guest can observe it and continue
                ASSERT(tgkill(remote_page->constants.my_tgid, remote_page->constants.my_tid, SIGSTOP) == 0);
                close_remote_page(remote_page);
                ASSERT(__ptrace(PTRACE_CONT, pid, 0, 0) == 0);
                break;
            }
        };
        return 0;
    }
    case felix86_ptrace_request::felix86_PTRACE_SEIZE: {
        // // const u64 flags = (u64)data;
        // // int result = __ptrace(PTRACE_SEIZE, pid, addr, data);
        // // if (result != 0) {
        // //     return result;
        // // }

        // // // Some programs send a signal like SIGSTOP first then attach with PTRACE_SEIZE
        // // // These will be already stopped. So here we attempt to get the remote page without
        // // // stopping the program ourselves. If we succeed, we can set the tracer_pid value without
        // // // needing to PTRACE_INTERRUPT. We don't want to waitpid here because that would observe
        // // // the stop when in reality we want the guest to observe it.
        // // remote_page = get_remote_page(pid);
        // // if (remote_page) {
        // //     ASSERT(remote_page);
        // //     ASSERT(remote_page->constants.tracer_pid == 0);
        // //     remote_page->constants.tracer_pid = gettid();
        // //     remote_page->constants.flags = flags;
        // //     close_remote_page(remote_page);
        // //     return 0;
        // // }

        // // // We send a PTRACE_INTERRUPT to set some remote_page values on the guest side
        // // int r = __ptrace(PTRACE_INTERRUPT, pid, 0, 0);
        // // if (r != 0) {
        // //     WARN("Failed to send PTRACE_INTERRUPT on seized process %d", pid);
        // //     return -ESRCH;
        // // }

        // // while (true) {
        // //     int status;
        // //     int r = waitpid(pid, &status, 0);
        // //     if (r != pid) {
        // //         WARN("Couldn't wait on %d after PTRACE_SEIZE", pid);
        // //         return -ESRCH;
        // //     }

        // //     if (!WIFSTOPPED(status)) {
        // //         WARN("Couldn't interrupt %d after PTRACE_SEIZE, status: %x", pid, status);
        // //         return -ESRCH;
        // //     }

        // //     if (status >> 8 != (SIGTRAP | (PTRACE_EVENT_STOP << 8))) {
        // //         // A different signal raced, reinject it until we hit our SIGSTOP
        // //         // This is a known limitation of PTRACE_ATTACH
        // //         WARN("Signal %d raced us to PTRACE_INTERRUPT", WSTOPSIG(status));
        // //         ASSERT(__ptrace(PTRACE_CONT, pid, 0, (void*)(u64)WSTOPSIG(status)) == 0);
        // //         continue;
        // //     } else {
        // //         remote_page = get_remote_page(pid);
        // //         ASSERT(remote_page);
        // //         ASSERT(remote_page->constants.tracer_pid == 0);
        // //         remote_page->constants.tracer_pid = gettid();
        // //         remote_page->constants.flags = flags;
        // //         close_remote_page(remote_page);
        // //         // Skip our PTRACE_INTERRUPT as the guest isn't supposed to see it
        // //         ASSERT(__ptrace(PTRACE_CONT, pid, 0, 0) == 0);
        // //         break;
        // //     }
        // }
        return -EINVAL;
    }
    case felix86_ptrace_request::felix86_PTRACE_DETACH: {
        i64 sig = (i64)data;
        if (sig < 0) {
            WARN("PTRACE_DETACH with negative signal: %d", sig);
            return -EIO;
        }

        remote_page->injected.signal = sig;
        remote_page->injected.cont_type = PTRACE_DETACH;
        // Skip the host signal from raise_stop
        remote_page->constants.tracer_pid = 0;
        remote_page->constants.flags = 0;
        int result = __ptrace(PTRACE_DETACH, pid, 0, 0);
        ASSERT(result == 0);
        return 0;
    }
    case felix86_ptrace_request::felix86_PTRACE_GET_SYSCALL_INFO: {
        if (!data) {
            return -EINVAL;
        }

        size_t ret_size = 0;
        x86_ptrace_syscall_info info;
        info.reserved = 0;
        info.flags = 0;
        info.arch = remote_page->stop_info.mode32 ? AUDIT_ARCH_I386 : AUDIT_ARCH_X86_64;
        UserContext ctx;
        get_remote_ctx(&ctx, remote_page, pid);
        info.instruction_pointer = ctx.rip;
        info.stack_pointer = ctx.gprs[X86_REF_RSP - X86_REF_RAX];

        // TODO: seccomp stop
        switch (remote_page->stop_info.type) {
        case StopType::SyscallEnterStop: {
            info.op = PTRACE_SYSCALL_INFO_ENTRY;
            memcpy(info.entry.args, remote_page->syscall_info.args, sizeof(info.entry.args));
            info.entry.nr = remote_page->syscall_info.nr;
            ret_size = 80; // TODO: verify this size
            break;
        }
        case StopType::SyscallExitStop: {
            info.op = PTRACE_SYSCALL_INFO_EXIT;
            info.exit.is_error = remote_page->syscall_info.is_error;
            info.exit.rval = remote_page->syscall_info.ret;
            ret_size = 40; // TODO: verify this size
            break;
        }
        default: {
            info.op = PTRACE_SYSCALL_INFO_NONE;
            ret_size = 24; // size of x86_ptrace_syscall_info before the union
            break;
        }
        }

        size_t copy_size = std::min(ret_size, sizeof(x86_ptrace_syscall_info));
        memcpy(data, &info, copy_size);
        return ret_size;
    }
    case felix86_ptrace_request::felix86_PTRACE_GETSIGINFO: {
        memcpy(data, &remote_page->stop_info.info, tracer_mode32 ? sizeof(siginfo_t) : sizeof(x86_siginfo_t));
        return 0;
    }
    case felix86_ptrace_request::felix86_PTRACE_SETSIGINFO: {
        memcpy(&remote_page->stop_info.info, data, tracer_mode32 ? sizeof(siginfo_t) : sizeof(x86_siginfo_t));
        return 0;
    }
    case felix86_ptrace_request::felix86_PTRACE_GETEVENTMSG: {
        memcpy(data, &remote_page->stop_info.event_msg, tracer_mode32 ? sizeof(u32) : sizeof(u64));
        return 0;
    }
    case felix86_ptrace_request::felix86_PTRACE_PEEKDATA:
    case felix86_ptrace_request::felix86_PTRACE_PEEKTEXT: {
        if (!data) {
            WARN("PTRACE_PEEKDATA with data==nullptr");
            return -EFAULT;
        }
        // Edge case: Host PEEKDATA does a 64-bit read, but a 32-bit process needs to do a 32-bit read
        // In most cases we can do a 64-bit read fine, but if near the end of a page this
        // can cause EFAULT. So if the 64-bit read would overflow but a 32-bit read wouldn't on a
        // 32-bit process, adjust the address. If both a 32-bit read and a 64-bit overflows to the next
        // page we don't need to adjust because if the next page would fault it would fault on 32-bit anyway
        // Cases where this happens:
        // Read from 0xFF9 -> 0xFFC, an 8-byte read overflows. In this case, do a read from an aligned address
        // and shift the result into place
        // Read from 0xFFD -> 0xFFF, an 8-byte read overflows, but a 4-byte read overflows too, so pass it through
        u64 temp;
        u64 offset = (u64)addr & 0xFFF;
        bool overflows = offset >= 0xFF9 && offset <= 0xFFC;
        int result;
        if (tracer_mode32 && overflows) {
            u64 aligned_addr = (u64)addr & ~0b111;
            u64 shift = (u64)addr & 0b111;
            result = __ptrace(PTRACE_PEEKDATA, pid, (void*)aligned_addr, &temp);
            temp >>= shift;
        } else {
            result = __ptrace(PTRACE_PEEKDATA, pid, addr, &temp);
        }
        if (result == 0) {
            // TODO: this should set a temporary signal handler to catch faults and return EFAULT
            if (tracer_mode32) {
                memcpy(data, &temp, sizeof(u32));
            } else {
                memcpy(data, &temp, sizeof(u64));
            }
        } else {
            ASSERT(result == -1);
        }
        return result;
    }
    case felix86_ptrace_request::felix86_PTRACE_PEEKUSER: {
        ASSERT(remote_page);
        u64 offset = (u64)addr;
        u64 oob = tracer_mode32 ? 284 : 912; // sizes of struct user in x86 32-bit and 64-bit
        if (offset >= oob) {
            WARN("Tried to PTRACE_PEEKUSER out of bounds");
            return -EIO;
        }

        u64 temp = 0;
        int result = -EIO;
        bool flags_offset = tracer_mode32 ? (offset == offsetof(x86_user_regs_struct, eflags)) : (offset == offsetof(x64_user_regs_struct, eflags));
        if (flags_offset) {
            UserContext remote_ctx;
            get_remote_ctx(&remote_ctx, remote_page, pid);
            temp = remote_ctx.GetFlags();
            result = 0;
        } else {
            int threadstate_offset = user_to_threadstate_offset(offset, tracer_mode32);
            u8* address = (u8*)remote_page->constants.state + threadstate_offset;
            result = __ptrace(PTRACE_PEEKDATA, pid, address, &temp);
            if (result == -1) {
                result = -errno;
                WARN("Failed to PEEKDATA on ThreadState: %d", result);
            }
        }

        if (result == 0) {
            if (tracer_mode32) {
                memcpy(data, &temp, sizeof(u32));
            } else {
                memcpy(data, &temp, sizeof(u64));
            }
        }
        return result;
    }
    case felix86_ptrace_request::felix86_PTRACE_POKEUSER: {
        ASSERT(remote_page);
        u64 offset = (u64)addr;
        u64 oob = tracer_mode32 ? 284 : 912; // sizes of struct user in x86 32-bit and 64-bit
        if (offset >= oob) {
            WARN("Tried to PTRACE_POKEUSER out of bounds");
            return -EIO;
        }

        int result = -EIO;
        bool flags_offset = tracer_mode32 ? (offset == offsetof(x86_user_regs_struct, eflags)) : (offset == offsetof(x64_user_regs_struct, eflags));
        if (flags_offset) {
            UserContext remote_ctx;
            get_remote_ctx(&remote_ctx, remote_page, pid);
            remote_ctx.SetFlags((u64)data);
            set_remote_ctx(&remote_ctx, remote_page, pid);
            result = 0;
        } else {
            int threadstate_offset = user_to_threadstate_offset(offset, tracer_mode32);
            u8* address = (u8*)remote_page->constants.state + threadstate_offset;
            result = __ptrace(PTRACE_POKEDATA, pid, address, (void*)data);
            if (result == -1) {
                result = -errno;
                WARN("Failed to POKEDATA on ThreadState: %d", result);
            }
        }
        return result;
    }
    case felix86_ptrace_request::felix86_PTRACE_POKEDATA:
    case felix86_ptrace_request::felix86_PTRACE_POKETEXT: {
        int result;
        if (tracer_mode32) {
            // TODO: Needs using process_vm_writev, needs caution around crossing pages and splitting the write into two iovecs
            // NOTE: peek+poke is probably not fine. A poke on aligned memory is likely atomic, while a peek+poke isn't.
            result = -1;
            UNIMPLEMENTED();
        } else {
            result = __ptrace(PTRACE_POKEDATA, pid, addr, data);
        }
        ASSERT(result == 0 || result == -1);
        return result;
    }
    case felix86_ptrace_request::felix86_PTRACE_CONT: {
        i64 sig = (i64)data;
        if (sig < 0) {
            WARN("PTRACE_CONT with negative signal: %d", sig);
            return -EIO;
        }

        remote_page->injected.signal = sig;
        remote_page->injected.cont_type = PTRACE_CONT;
        // Skip the host signal from raise_stop
        int result = __ptrace(PTRACE_CONT, pid, 0, 0);
        ASSERT(result == 0);
        return 0;
    }
    case felix86_ptrace_request::felix86_PTRACE_KILL: {
        return __ptrace(PTRACE_KILL, pid, addr, data);
    }
    case felix86_ptrace_request::felix86_PTRACE_SETOPTIONS: {
        u64 handled_flags = PTRACE_O_EXITKILL | PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACEFORK | PTRACE_O_TRACECLONE | PTRACE_O_TRACEVFORK |
                            PTRACE_O_TRACEVFORKDONE | PTRACE_O_TRACEEXEC | PTRACE_O_TRACEEXIT | PTRACE_O_TRACESECCOMP;
        if ((u64)data & ~handled_flags) {
            ERROR("Unhandled ptrace flags: %lx", (u64)data & ~handled_flags);
        }

        // We need to passthrough PTRACE_O_TRACECLONE and PTRACE_O_TRACEFORK to the host as we want the children to automatically
        // be traced by the tracer, if the guest ones are
        u64 host_flags = (u64)data & (PTRACE_O_EXITKILL | PTRACE_O_TRACECLONE | PTRACE_O_TRACEFORK);
        int result = __ptrace(PTRACE_SETOPTIONS, pid, addr, (void*)host_flags);
        if (result != 0) {
            int error = errno;
            GUESTPTRACELOG("PTRACE_SETOPTIONS failed with %d", error);
            return -error;
        } else {
            remote_page->constants.flags = (u64)data;
            return 0;
        }
    }
    case felix86_ptrace_request::felix86_PTRACE_SINGLESTEP: {
        i64 sig = (i64)data;
        if (sig < 0) {
            WARN("PTRACE_SINGLESTEP with negative signal: %d", sig);
            return -EIO;
        }

        remote_page->injected.signal = sig;
        remote_page->injected.cont_type = PTRACE_SINGLESTEP;
        int result = __ptrace(PTRACE_CONT, pid, nullptr, data);
        ASSERT(result == 0);
        return 0;
    }
    case felix86_ptrace_request::felix86_PTRACE_GETSIGMASK: {
        u64 size = (u64)addr;
        if (size > 8) {
            memset((u8*)data + 8, 0, (u64)addr - 8);
            size = 8;
        }
        memcpy(data, &remote_page->stop_info.sigmask, size);
        return 0;
    }
    case felix86_ptrace_request::felix86_PTRACE_SYSCALL: {
        if (data != 0 || addr != 0) {
            WARN("Data or addr not 0 during PTRACE_SYSCALL?");
        }

        remote_page->injected.signal = 0;
        remote_page->injected.cont_type = PTRACE_SYSCALL;
        // Skip the host signal from raise_stop
        int result = __ptrace(PTRACE_CONT, pid, 0, 0);
        ASSERT(result == 0);
        return 0;
    }
    case felix86_ptrace_request::felix86_PTRACE_SETSIGMASK: {
        u64 size = (u64)addr;
        if (size > 8) {
            size = 8;
        }
        memcpy(&remote_page->stop_info.sigmask, data, size);
        return 0;
    }
    case felix86_ptrace_request::felix86_PTRACE_GETREGSET: {
        u64 dest, size;
        if (tracer_mode32) {
            x86_iovec* io = (x86_iovec*)data;
            dest = io->iov_base;
            size = io->iov_len;
        } else {
            iovec* io = (iovec*)data;
            dest = (u64)io->iov_base;
            size = io->iov_len;
        }
#define NT_PRSTATUS 1
#define NT_386_TLS 0x200
#define NT_386_IOPERM 0x201
#define NT_X86_XSTATE 0x202
#define NT_X86_SHSTK 0x204
#define NT_X86_XSAVE_LAYOUT 0x205
        switch ((u64)addr) {
        case NT_PRSTATUS: {
            return get_regs(tracer_mode32, remote_page, pid, data);
        }
        case NT_X86_XSTATE: {
            if (!is_feature_enabled(x86_feature::OSXSAVE)) {
                return -ENODEV;
            }

            if (size < felix86_xsave_size()) {
                WARN("Partial NT_X86_XSTATE requested: %d", size);
            }

            UserContext remote_ctx;
            get_remote_ctx(&remote_ctx, remote_page, pid);
            constexpr size_t total_size = felix86_xsave_size();
            u8 buffer[total_size];
            felix86_xsave(remote_ctx, buffer, true);
            // See update_regset_xstate_info
            *(u64*)(buffer + 464) = get_xfeature_enabled_mask();
            memcpy((void*)dest, buffer, size);
            return 0;
        }
        default: {
            WARN("PTRACE_GETREGSET with unknown addr: %lx", addr);
            return -EINVAL;
        }
        }
    }
    case felix86_ptrace_request::felix86_PTRACE_GETREGS: {
        return get_regs(tracer_mode32, remote_page, pid, data);
    }
    case felix86_ptrace_request::felix86_PTRACE_SETREGS: {
        return set_regs(tracer_mode32, remote_page, pid, data);
    }
    default: {
        ERROR("Unimplemented operation: %x", op);
        return -EINVAL;
    }
    }
}

void raise_stop(StopType stop, int& sig, siginfo_t* guest_info, int event, u64 event_msg) {
    ThreadState* state = ThreadState::Get();
    PtracePage* page = state->ptrace_page;
    bool old_tf = state->ctx.tf;
    page->stop_info.mode32 = state->ctx.Mode32();
    page->stop_info.type = stop;
    page->stop_info.info = *guest_info;
    page->stop_info.signal = sig;
    if ((page->constants.flags & PTRACE_O_TRACESYSGOOD) && (stop == StopType::SyscallEnterStop || stop == StopType::SyscallExitStop)) {
        page->stop_info.signal |= 0x80;
    }
    page->stop_info.ptrace_event = event;
    page->stop_info.event_msg = event_msg;
    page->stop_info.sigmask = state->signal_mask.__val[0];
    page->stop_info.stopped = true;
    memset(&page->injected, 0, sizeof(page->injected));
    auto tid = gettid();
    GUESTPTRACELOG("--- %d is raising %s with signal %s ---", tid, stop_to_string(stop), sigdescr_np(sig));
    siginfo_t info;
    memset(&info, 0, sizeof(siginfo_t)); // kernel requirement
    info.si_signo = FELIX86_PTRACE_SIGNAL;
    info.si_code = FELIX86_PTRACE_CODE_INSTOP;
    info.si_pid = tid;
    info.si_uid = getuid();
    info.si_value.sival_ptr = nullptr;
    int r = syscall(SYS_rt_tgsigqueueinfo, getpid(), tid, FELIX86_PTRACE_SIGNAL, &info);
    ASSERT_MSG(r == 0, "SYS_rt_tgsigqueueinfo failed with %d during raise_stop", errno);

    switch (page->injected.cont_type) {
    case PTRACE_CONT:
    case PTRACE_SYSCALL:
    case PTRACE_DETACH: {
        break;
    }
    case PTRACE_SINGLESTEP: {
        state->recompiler->setSingleStepMode(SingleStepMode::PtraceSinglestep);
        state->recompiler->clearCodeCache(state);
        break;
    }
    default: {
        WARN("Unknown cont_type: %d", page->injected.cont_type);
        break;
    }
    }

    // Resumed by tracer...
    memset(&page->stop_info, 0, sizeof(page->stop_info));
    *guest_info = page->injected.info;
    Signals::sigprocmask(state, SIG_SETMASK, &state->signal_mask, nullptr);
    ASSERT(page->injected.signal >= 0);
    sig = page->injected.signal;

    // Address space could've been changed in many ways, not only via POKEDATA but also process_vm_writev
    // and even /proc/<pid>/mem or /proc/<pid>/task/<pid>/mem + write syscalls
    // Since we don't currently track any of these, invalidate the entire code cache after restart
    Recompiler::invalidateRangeGlobal(0, UINT64_MAX & ~0xFFFull, "ptrace stop resumed");

    state->recompiler->clearBreakpoints();
    u64 debug_control = state->ctx.debug_control;
    for (int i = 0; i < 4; i++) {
        u8 local_enabled = (debug_control >> (i * 2)) & 0b11;
        if (local_enabled == 0b00) {
            // Disabled...
        } else {
            u64 address = state->ctx.debug_register[i];
            u8 type = (debug_control >> (16 + 4 * i)) & 0b11;
            u8 length = (debug_control >> (18 + 4 * i)) & 0b11;
            if (type == 0b00) {
                if (length != 0) {
                    WARN("Breakpoint with length != 0?");
                }

                GUESTPTRACELOG("Added breakpoint at %lx", address);
                state->recompiler->addBreakpoint(i, address);
            } else {
                WARN("Ptrace watchpoint set at %lx but watchpoints are not currently supported", address);
            }
        }
    }

    if (state->ctx.tf != old_tf) {
        WARN("Trap flag changed during stop");
        felix86_tf_changed(state, state->ctx.tf);
    }
}

bool is_traced(ThreadState* state) {
    return state->ptrace_page->constants.tracer_pid != 0;
}

PtracePage* get_remote_page(pid_t pid) {
    // ThreadState is always in the gp register, whether the tracee is stopped in C++ or JIT code
    // This means we can access it using PTRACE_PEEKUSER
    // TODO: validate same version of felix86 is used across tracer/tracee first, using /proc/self/exe
    // Get the remote memfd number, then open it here and map its contents in our address space
    void* remote_state = get_remote_state(pid);
    if (!remote_state) {
        HOSTPTRACELOG("Remote ThreadState is nullptr, this shouldn't happen");
        return nullptr;
    }

    i64 remote_fd;
    int result = __ptrace(PTRACE_PEEKDATA, pid, (void*)((u64)remote_state + offsetof(ThreadState, ptrace_fd)), &remote_fd);
    static_assert(Recompiler::threadStatePointer() == biscuit::gp);
    ASSERT_MSG(result == 0, "PTRACE_PEEKDATA returned %d, errno: %d", result, errno);
    ASSERT_MSG(remote_fd >= FD::min() && remote_fd <= FD::max(), "remote_fd is wrong");

    // TODO: we should prefer a pidfd_getfd path, e.g. for no rootfs chroots that dont mount proc, needs >=6.9 kernel
    char buffer[256];
    int written = snprintf(buffer, sizeof(buffer), "/proc/%d/fd/%d", pid, (int)remote_fd);
    ASSERT(written > 0 && written < sizeof(buffer));

    int memfd = open(buffer, O_RDWR);
    ASSERT(memfd >= 0);
    void* remote_page = mmap(nullptr, sizeof(PtracePage), PROT_READ | PROT_WRITE, MAP_SHARED, memfd, 0);
    ASSERT(remote_page != MAP_FAILED);
    ASSERT(close(memfd) == 0);

    return (PtracePage*)remote_page;
}

void close_remote_page(PtracePage* page) {
    ASSERT(munmap(page, 4096) == 0);
}
} // namespace Ptrace
