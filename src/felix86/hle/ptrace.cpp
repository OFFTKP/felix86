#include <dirent.h>
#include <linux/audit.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <sys/wait.h>
#include "felix86/common/feature.hpp"
#include "felix86/common/log.hpp"
#include "felix86/hle/ptrace.hpp"
#include "felix86/hle/signals.hpp"
#include "felix86/v2/recompiler.hpp"

// This struct allows us to access a ThreadState on a different pid by reading the whole struct to a local
// buffer and writing it back on the destructor. This is safe to do because this only happens when the tracer is stopped
// Note: we need to manually call commit before calling PTRACE_CONT or similar restarting operations
// Design note: Originally we used a separate page allocated from a memfd so we didn't have to writeback after allocating it
// in our address space. However this would mean that each ThreadState would need a unique FD or a global fd + arena allocator which would
// be harder to manage. Also occassionally we needed to access stuff outside of the ptrace page.
// TODO: this assumes the emulator of the tracer and the tracee is the same version and doesn't change the offsets
struct RemoteState {
    RemoteState() : remote_state(nullptr), pid(0) {}

    explicit RemoteState(void* remote_state, pid_t pid) : remote_state(remote_state), pid(pid) {
        HOSTPTRACELOG("Getting remote state (%lx) from %d", remote_state, pid);
        ASSERT(remote_state);
        iovec local, remote;
        local.iov_base = &data;
        local.iov_len = sizeof(ThreadState);
        remote.iov_base = remote_state;
        remote.iov_len = sizeof(ThreadState);
        int result = syscall(SYS_process_vm_readv, pid, &local, 1, &remote, 1, 0);
        ASSERT(result == sizeof(ThreadState));
    }

    ~RemoteState() {
        commit();
    }

    RemoteState(const RemoteState&) = delete;
    RemoteState& operator=(const RemoteState&) = delete;
    RemoteState(RemoteState&& other) = delete;
    RemoteState& operator=(RemoteState&& other) {
        if (this != &other) {
            commit();
            data = other.data;
            remote_state = other.remote_state;
            pid = other.pid;
            other.remote_state = nullptr;
        }
        return *this;
    }

    ThreadState* operator->() {
        return &data;
    }
    const ThreadState* operator->() const {
        return &data;
    }
    ThreadState& operator*() {
        return data;
    }
    const ThreadState& operator*() const {
        return data;
    }
    explicit operator bool() const {
        return remote_state != nullptr;
    }

    void commit() {
        if (!remote_state)
            return;
        HOSTPTRACELOG("Commiting remote state (%lx) to %d", remote_state, pid);
        iovec local, remote;
        local.iov_base = &data;
        local.iov_len = sizeof(ThreadState);
        remote.iov_base = remote_state;
        remote.iov_len = sizeof(ThreadState);
        int result = syscall(SYS_process_vm_writev, pid, &local, 1, &remote, 1, 0);
        ASSERT(result == sizeof(ThreadState));
        release();
    }

    void release() {
        remote_state = nullptr;
    }

    void* get() {
        return remote_state;
    }

private:
    ThreadState data;
    void* remote_state = nullptr;
    pid_t pid = 0;
};

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
constexpr static int reg_index = ref - X86_REF_RAX;

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

static RemoteState get_remote_state(pid_t pid) {
    riscv_user_regs_struct regs;
    struct iovec io;
    io.iov_base = &regs;
    io.iov_len = sizeof(regs);
    int result = __ptrace(PTRACE_GETREGSET, pid, (void*)1, &io);
    int error = errno;
    if (result == -1 && error == ESRCH) {
        HOSTPTRACELOG("Tried to get remote page of %d but it is not stopped or not traced", pid);
        return RemoteState();
    }
    ASSERT_MSG(result == 0, "PTRACE_GETREGSET returned %d, errno: %d", result, error);
    ASSERT(io.iov_len == sizeof(regs));

    void* remote_state_ptr = (void*)regs.gp;
    if (!remote_state_ptr) {
        return RemoteState();
    }

    return RemoteState(remote_state_ptr, pid);
}

static int get_regs(bool tracer_mode32, const RemoteState& remote_state, void* data) {
    const UserContext& remote_ctx = remote_state->ctx;
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

static int set_regs(bool tracer_mode32, RemoteState& remote_state, void* data) {
    UserContext& remote_ctx = remote_state->ctx;
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
    return 0;
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
            pid = result;
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
                RemoteState remote_state = get_remote_state(pid);
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
                        remote_state.release(); // nothing to commit
                        int result = __ptrace(PTRACE_CONT, pid, 0, 0);
                        ASSERT(result == 0);
                        continue; // go back to waiting
                    }

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

                if (!remote_state) {
                    WARN("remote_state is null, this is unexpected. Status: %lx", host_status);
                    break;
                }

                if (remote_state->ptrace_data.constants.is_terminating) {
                    // Tracee re-raised a terminating signal (see SignalBehavior::Terminate), allow it to terminate
                    int result = __ptrace(PTRACE_CONT, pid, 0, (void*)(u64)WSTOPSIG(host_status));
                    ASSERT(result == 0);
                    continue;
                }

                if (sig == SIGSTOP) {
                    // This may be the signal from forking/vforking/cloning, which we want to skip and re-raise as SIGPTRACE later
                    if (remote_state->ptrace_data.stop_info.in_clone) {
                        HOSTPTRACELOG("Skipping clone SIGSTOP on %d", pid);
                        int result = __ptrace(PTRACE_CONT, pid, 0, 0);
                        ASSERT(result == 0);
                        continue;
                    }
                }

                HOSTPTRACELOG("%d is stopped on signal %d", pid, sig);

                pid_t our_pid = gettid();
                pid_t expected_pid = remote_state->ptrace_data.constants.tracer_pid;
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
                        ASSERT(remote_state->ptrace_data.stop_info.stopped);
                        GUESTPTRACELOG("%d has been observed entering stop of type %s", pid,
                                       stop_to_string(remote_state->ptrace_data.stop_info.type));
                        host_status = 0x7f; // Stopped
                        static_assert(WIFSTOPPED(0x7f));
                        host_status |= ((u32)remote_state->ptrace_data.stop_info.signal & 0xFF) << 8;
                        host_status |= (u32)remote_state->ptrace_data.stop_info.ptrace_event << 16;
                        memset(&remote_state->ptrace_data.injected, 0, sizeof(remote_state->ptrace_data.injected));
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
                        u64 signal_table_ptr = (u64)remote_state->signal_table;
                        // We can't access the signal_table directly from remote_state
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

                                // We need to force the tracee to defer this signal to a safepoint
                                // SIG_IGN signals need to be injected as sigptrace because otherwise
                                // they would be ignored in the tracee side, SIG_DFL signals same reason plus
                                // the fact that the default behavior of stop would be bad to do, plus
                                // we get to change SIGSTOP. So 3 birds with 1 stone.
                                remote_state->ptrace_data.force_defer.deferred = true;
                                remote_state->ptrace_data.force_defer.original_sig = sig;
                                remote_state->ptrace_data.force_defer.original_info = tracee_siginfo;
                                u64 original_mask;
                                r = __ptrace(PTRACE_GETSIGMASK, pid, (void*)sizeof(u64), &original_mask);
                                remote_state->ptrace_data.force_defer.original_mask = original_mask;
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

                // TODO: can probably be remote_state.release() if nothing changed on all paths to this point
                remote_state.commit();
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
    RemoteState remote_state;
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
        remote_state = get_remote_state(pid);
        if (!remote_state) {
            HOSTPTRACELOG("Tried to run ptrace operation %s on %d, but it is not stopped or not traced by us", guest_op_to_string(op), pid);
            return -ESRCH;
        }

        if (!remote_state->ptrace_data.stop_info.stopped) {
            // Not in an official stop, so return ESRCH and warn
            // This may happen if a different thread than the tracer was already run a waitpid on the tracee
            // We want to try to run the operation anyway, but this may be a host-side stop
            WARN("Tried to run ptrace operation %s on %d, but it is not guest stopped", guest_op_to_string(op), pid);
        }

        if (remote_state->ptrace_data.constants.tracer_pid != gettid()) {
            WARN("Tracer pid (%d) and our pid (%d) mismatch?", gettid(), remote_state->ptrace_data.constants.tracer_pid);
            return -EPERM;
        }

        bool differing_mode = tracer_mode32 != remote_state->ptrace_data.stop_info.mode32;
        if (differing_mode) {
            WARN_ONCE("Tracer %d is %s but tracee %d is %s", gettid(), tracer_mode32 ? "32-bit" : "64-bit", pid,
                      remote_state->ptrace_data.stop_info.mode32 ? "32-bit" : "64-bit");
        }
        break;
    }
    }

    switch (op) {
    case felix86_ptrace_request::felix86_PTRACE_TRACEME: {
        ThreadState* local_state = ThreadState::Get();
        if (Ptrace::is_traced(local_state)) {
            HOSTPTRACELOG("TRACEME run when already being traced by %d", local_state->ptrace_data.constants.tracer_pid);
            return -EPERM;
        }

        pid_t tracer = getppid();
        local_state->ptrace_data.constants.tracer_pid = tracer;
        int result = __ptrace(PTRACE_TRACEME, 0, 0, 0);
        if (result != 0) {
            int error = -errno;
            GUESTPTRACELOG("TRACEME failed with %d", errno);
            local_state->ptrace_data.constants.tracer_pid = 0;
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
                remote_state = get_remote_state(pid);
                ASSERT(remote_state);
                ASSERT(remote_state->ptrace_data.constants.tracer_pid == 0);
                remote_state->ptrace_data.constants.tracer_pid = gettid();
                // Re-raise the SIGSTOP so the guest can observe it and continue
                ASSERT(tgkill(remote_state->ptrace_data.constants.my_tgid, remote_state->ptrace_data.constants.my_tid, SIGSTOP) == 0);
                remote_state.commit();
                ASSERT(__ptrace(PTRACE_CONT, pid, 0, 0) == 0);
                break;
            }
        };
        return 0;
    }
    case felix86_ptrace_request::felix86_PTRACE_SEIZE: {
        return -EINVAL;
    }
    case felix86_ptrace_request::felix86_PTRACE_DETACH: {
        int sig = (i32)(u64)data;
        if (sig < 0) {
            WARN("PTRACE_DETACH with negative signal: %d", sig);
            return -EIO;
        }

        // TODO: merge all the conts into one function so it also sets the siginfo when not setsiginfo'd
        if (data != 0 || addr != 0) {
            WARN("Data or addr not 0 during PTRACE_DETACH?");
        }

        remote_state->ptrace_data.stop_info.signal = sig;
        remote_state->ptrace_data.injected.cont_type = PTRACE_DETACH;
        // Skip the host signal from raise_stop
        remote_state->ptrace_data.constants.tracer_pid = 0;
        remote_state->ptrace_data.constants.flags = 0;
        remote_state.commit();
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
        info.arch = remote_state->ptrace_data.stop_info.mode32 ? AUDIT_ARCH_I386 : AUDIT_ARCH_X86_64;
        info.instruction_pointer = remote_state->ctx.rip;
        info.stack_pointer = remote_state->ctx.gprs[X86_REF_RSP - X86_REF_RAX];

        // TODO: seccomp stop
        switch (remote_state->ptrace_data.stop_info.type) {
        case StopType::SyscallEnterStop: {
            info.op = PTRACE_SYSCALL_INFO_ENTRY;
            memcpy(info.entry.args, remote_state->ptrace_data.syscall_info.args, sizeof(info.entry.args));
            info.entry.nr = remote_state->ptrace_data.syscall_info.nr;
            ret_size = 80; // TODO: verify this size
            break;
        }
        case StopType::SyscallExitStop: {
            info.op = PTRACE_SYSCALL_INFO_EXIT;
            info.exit.is_error = remote_state->ptrace_data.syscall_info.is_error;
            info.exit.rval = remote_state->ptrace_data.syscall_info.ret;
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
        memcpy(data, &remote_state->ptrace_data.stop_info.info, tracer_mode32 ? sizeof(x86_siginfo_t) : sizeof(siginfo_t));
        return 0;
    }
    case felix86_ptrace_request::felix86_PTRACE_SETSIGINFO: {
        remote_state->ptrace_data.injected.siginfo_changed = true;
        memcpy(&remote_state->ptrace_data.stop_info.info, data, tracer_mode32 ? sizeof(x86_siginfo_t) : sizeof(siginfo_t));
        return 0;
    }
    case felix86_ptrace_request::felix86_PTRACE_GETEVENTMSG: {
        memcpy(data, &remote_state->ptrace_data.stop_info.event_msg, tracer_mode32 ? sizeof(u32) : sizeof(u64));
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
            temp >>= shift * 8;
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
    case felix86_ptrace_request::felix86_PTRACE_PEEKUSER:
    case felix86_ptrace_request::felix86_PTRACE_POKEUSER: {
        u64 offset = (u64)addr;
        u64 oob = tracer_mode32 ? 284 : 912; // sizes of struct user in x86 32-bit and 64-bit
        if (offset >= oob) {
            WARN("Tried to PTRACE_PEEKUSER out of bounds");
            return -EIO;
        }

        u64* target = nullptr;
        bool is_flags = false;
        bool is_poke = op == felix86_ptrace_request::felix86_PTRACE_POKEUSER;
        if (tracer_mode32) {
            switch (offset) {
            case 252 + 0 * sizeof(u32):
                target = &remote_state->ctx.debug_register[0];
                break;
            case 252 + 1 * sizeof(u32):
                target = &remote_state->ctx.debug_register[1];
                break;
            case 252 + 2 * sizeof(u32):
                target = &remote_state->ctx.debug_register[2];
                break;
            case 252 + 3 * sizeof(u32):
                target = &remote_state->ctx.debug_register[3];
                break;
            case 252 + 4 * sizeof(u32):
                WARN("Accessing debug register 4, this shouldn't happen");
                [[fallthrough]];
            case 252 + 6 * sizeof(u32):
                target = &remote_state->ctx.debug_status;
                break;
            case 252 + 5 * sizeof(u32):
                WARN("Accessing debug register 5, this shouldn't happen");
                [[fallthrough]];
            case 252 + 7 * sizeof(u32):
                target = &remote_state->ctx.debug_control;
                break;
            case offsetof(x86_user_regs_struct, ebx):
                target = &remote_state->ctx.gprs[reg_index<X86_REF_RBX>];
                break;
            case offsetof(x86_user_regs_struct, ecx):
                target = &remote_state->ctx.gprs[reg_index<X86_REF_RCX>];
                break;
            case offsetof(x86_user_regs_struct, edx):
                target = &remote_state->ctx.gprs[reg_index<X86_REF_RDX>];
                break;
            case offsetof(x86_user_regs_struct, esi):
                target = &remote_state->ctx.gprs[reg_index<X86_REF_RSI>];
                break;
            case offsetof(x86_user_regs_struct, edi):
                target = &remote_state->ctx.gprs[reg_index<X86_REF_RDI>];
                break;
            case offsetof(x86_user_regs_struct, ebp):
                target = &remote_state->ctx.gprs[reg_index<X86_REF_RBP>];
                break;
            case offsetof(x86_user_regs_struct, esp):
                target = &remote_state->ctx.gprs[reg_index<X86_REF_RSP>];
                break;
            case offsetof(x86_user_regs_struct, orig_eax):
                target = &remote_state->ctx.orig_rax;
                break;
            case offsetof(x86_user_regs_struct, eax):
                target = &remote_state->ctx.gprs[reg_index<X86_REF_RAX>];
                break;
            case offsetof(x86_user_regs_struct, eip):
                target = &remote_state->ctx.rip;
                break;
            // TODO: test these
            case offsetof(x86_user_regs_struct, xds):
                target = &remote_state->ctx.ds;
                break;
            case offsetof(x86_user_regs_struct, xss):
                target = &remote_state->ctx.ss;
                break;
            case offsetof(x86_user_regs_struct, xcs):
                target = &remote_state->ctx.cs;
                break;
            case offsetof(x86_user_regs_struct, xgs):
                target = &remote_state->ctx.gs;
                break;
            case offsetof(x86_user_regs_struct, xfs):
                target = &remote_state->ctx.fs;
                break;
            case offsetof(x86_user_regs_struct, xes):
                target = &remote_state->ctx.es;
                break;
            case offsetof(x86_user_regs_struct, eflags):
                is_flags = true;
                break;
            default:
                UNREACHABLE();
                break;
            }
        } else {
            switch (offset) {
            case 848 + 0 * sizeof(u64):
                target = &remote_state->ctx.debug_register[0];
                break;
            case 848 + 1 * sizeof(u64):
                target = &remote_state->ctx.debug_register[1];
                break;
            case 848 + 2 * sizeof(u64):
                target = &remote_state->ctx.debug_register[2];
                break;
            case 848 + 3 * sizeof(u64):
                target = &remote_state->ctx.debug_register[3];
                break;
            case 848 + 4 * sizeof(u64):
                WARN("Accessing debug register 4, this shouldn't happen");
                [[fallthrough]];
            case 848 + 6 * sizeof(u64):
                target = &remote_state->ctx.debug_status;
                break;
            case 848 + 5 * sizeof(u64):
                WARN("Accessing debug register 5, this shouldn't happen");
                [[fallthrough]];
            case 848 + 7 * sizeof(u64):
                target = &remote_state->ctx.debug_control;
                break;
            case offsetof(x64_user_regs_struct, rax):
                target = &remote_state->ctx.gprs[reg_index<X86_REF_RAX>];
                break;
            case offsetof(x64_user_regs_struct, orig_rax):
                target = &remote_state->ctx.orig_rax;
                break;
            case offsetof(x64_user_regs_struct, rcx):
                target = &remote_state->ctx.gprs[reg_index<X86_REF_RCX>];
                break;
            case offsetof(x64_user_regs_struct, rdx):
                target = &remote_state->ctx.gprs[reg_index<X86_REF_RDX>];
                break;
            case offsetof(x64_user_regs_struct, rbx):
                target = &remote_state->ctx.gprs[reg_index<X86_REF_RBX>];
                break;
            case offsetof(x64_user_regs_struct, rsp):
                target = &remote_state->ctx.gprs[reg_index<X86_REF_RSP>];
                break;
            case offsetof(x64_user_regs_struct, rbp):
                target = &remote_state->ctx.gprs[reg_index<X86_REF_RBP>];
                break;
            case offsetof(x64_user_regs_struct, rsi):
                target = &remote_state->ctx.gprs[reg_index<X86_REF_RSI>];
                break;
            case offsetof(x64_user_regs_struct, rdi):
                target = &remote_state->ctx.gprs[reg_index<X86_REF_RDI>];
                break;
            case offsetof(x64_user_regs_struct, r8):
                target = &remote_state->ctx.gprs[reg_index<X86_REF_R8>];
                break;
            case offsetof(x64_user_regs_struct, r9):
                target = &remote_state->ctx.gprs[reg_index<X86_REF_R9>];
                break;
            case offsetof(x64_user_regs_struct, r10):
                target = &remote_state->ctx.gprs[reg_index<X86_REF_R10>];
                break;
            case offsetof(x64_user_regs_struct, r11):
                target = &remote_state->ctx.gprs[reg_index<X86_REF_R11>];
                break;
            case offsetof(x64_user_regs_struct, r12):
                target = &remote_state->ctx.gprs[reg_index<X86_REF_R12>];
                break;
            case offsetof(x64_user_regs_struct, r13):
                target = &remote_state->ctx.gprs[reg_index<X86_REF_R13>];
                break;
            case offsetof(x64_user_regs_struct, r14):
                target = &remote_state->ctx.gprs[reg_index<X86_REF_R14>];
                break;
            case offsetof(x64_user_regs_struct, r15):
                target = &remote_state->ctx.gprs[reg_index<X86_REF_R15>];
                break;
            case offsetof(x64_user_regs_struct, rip):
                target = &remote_state->ctx.rip;
                break;
            case offsetof(x64_user_regs_struct, es):
                target = &remote_state->ctx.es;
                break;
            case offsetof(x64_user_regs_struct, ds):
                target = &remote_state->ctx.ds;
                break;
            case offsetof(x64_user_regs_struct, cs):
                target = &remote_state->ctx.cs;
                break;
            case offsetof(x64_user_regs_struct, gs):
                target = &remote_state->ctx.gs;
                break;
            case offsetof(x64_user_regs_struct, fs):
                target = &remote_state->ctx.fs;
                break;
            case offsetof(x64_user_regs_struct, ss):
                target = &remote_state->ctx.ss;
                break;
            case offsetof(x64_user_regs_struct, gs_base):
                target = &remote_state->ctx.gsbase;
                break;
            case offsetof(x64_user_regs_struct, fs_base):
                target = &remote_state->ctx.fsbase;
                break;
            case offsetof(x64_user_regs_struct, eflags):
                is_flags = true;
                break;
            default:
                UNREACHABLE();
                break;
            }
        }

        const int size = tracer_mode32 ? sizeof(u32) : sizeof(u64);
        if (is_flags) {
            if (is_poke) {
                remote_state->ctx.SetFlags((u64)data);
            } else {
                u64 temp = remote_state->ctx.GetFlags();
                memcpy(data, &temp, size);
            }
        } else if (target) {
            if (is_poke) {
                memcpy(target, &data, size);
            } else {
                memcpy(data, target, size);
            }
        }
        return 0;
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
        int sig = (i32)(u64)data;
        if (sig < 0) {
            WARN("PTRACE_CONT with negative signal: %d", sig);
            return -EIO;
        }

        if (sig > 0 && !remote_state->ptrace_data.stop_info.stopped) {
            // If this is a host signal-delivery-stop then we need to not raise the next guest signal-delivery-stop...
            // TODO: handle this properly
            WARN("Tracee is not guest stopped but continued with signal, this will cause a second signal-delivery-stop");
        }

        if (sig != remote_state->ptrace_data.stop_info.signal && !remote_state->ptrace_data.injected.siginfo_changed) {
            // When the signal is changed and the siginfo isn't changed via PTRACE_SETSIGINFO
            // the siginfo value is changed as seen in signal.c SEND_SIG_NOINFO
            siginfo_t info;
            memset(&info, 0, sizeof(info));
            info.si_code = SI_USER;
            info.si_errno = 0;
            info.si_signo = sig;
            info.si_pid = gettid();
            info.si_uid = getuid();
            remote_state->ptrace_data.injected.siginfo_changed = true;
            remote_state->ptrace_data.stop_info.info = info;
        }

        remote_state->ptrace_data.stop_info.signal = sig;
        remote_state->ptrace_data.injected.cont_type = PTRACE_CONT;
        remote_state.commit();
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
            remote_state->ptrace_data.constants.flags = (u64)data;
            return 0;
        }
    }
    case felix86_ptrace_request::felix86_PTRACE_SINGLESTEP: {
        i64 sig = (i64)data;
        if (sig < 0) {
            WARN("PTRACE_SINGLESTEP with negative signal: %d", sig);
            return -EIO;
        }

        if (data != 0 || addr != 0) {
            WARN("Data or addr not 0 during PTRACE_SINGLESTEP?");
        }

        remote_state->ptrace_data.stop_info.signal = sig;
        remote_state->ptrace_data.injected.cont_type = PTRACE_SINGLESTEP;
        remote_state.commit();
        int result = __ptrace(PTRACE_CONT, pid, 0, 0);
        ASSERT(result == 0);
        return 0;
    }
    case felix86_ptrace_request::felix86_PTRACE_GETSIGMASK: {
        u64 size = (u64)addr;
        if (size > 8) {
            memset((u8*)data + 8, 0, (u64)addr - 8);
            size = 8;
        }
        memcpy(data, &remote_state->ptrace_data.stop_info.sigmask, size);
        return 0;
    }
    case felix86_ptrace_request::felix86_PTRACE_SYSCALL: {
        if (data != 0 || addr != 0) {
            WARN("Data or addr not 0 during PTRACE_SYSCALL?");
        }

        remote_state->ptrace_data.stop_info.signal = 0;
        remote_state->ptrace_data.injected.cont_type = PTRACE_SYSCALL;
        remote_state.commit();
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
        memcpy(&remote_state->ptrace_data.stop_info.sigmask, data, size);
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
            return get_regs(tracer_mode32, remote_state, (void*)dest);
        }
        case NT_X86_XSTATE: {
            if (!is_feature_enabled(x86_feature::OSXSAVE)) {
                return -ENODEV;
            }

            if (size < felix86_xsave_size()) {
                WARN("Partial NT_X86_XSTATE requested: %d", size);
            }

            constexpr size_t total_size = felix86_xsave_size();
            u8 buffer[total_size];
            felix86_xsave(remote_state->ctx, buffer, true);
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
        return get_regs(tracer_mode32, remote_state, data);
    }
    case felix86_ptrace_request::felix86_PTRACE_SETREGS: {
        return set_regs(tracer_mode32, remote_state, data);
    }
    default: {
        WARN("Unimplemented operation: %x", op);
        return -EIO;
    }
    }
}

void raise_stop(StopType stop, int& sig, siginfo_t* guest_info, int event, u64 event_msg) {
    ThreadState* local_state = ThreadState::Get();
    PtraceData& data = local_state->ptrace_data;
    bool old_tf = local_state->ctx.tf;
    data.stop_info.mode32 = local_state->ctx.Mode32();
    data.stop_info.type = stop;
    data.stop_info.info = *guest_info;
    data.stop_info.signal = sig;
    if ((data.constants.flags & PTRACE_O_TRACESYSGOOD) && (stop == StopType::SyscallEnterStop || stop == StopType::SyscallExitStop)) {
        data.stop_info.signal |= 0x80;
    }
    data.stop_info.ptrace_event = event;
    data.stop_info.event_msg = event_msg;
    data.stop_info.sigmask = local_state->signal_mask.__val[0];
    data.stop_info.stopped = true;
    memset(&data.injected, 0, sizeof(data.injected));
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

    switch (data.injected.cont_type) {
    case PTRACE_CONT:
    case PTRACE_SYSCALL:
    case PTRACE_DETACH: {
        break;
    }
    case PTRACE_SINGLESTEP: {
        local_state->recompiler->setSingleStepMode(SingleStepMode::PtraceSinglestep);
        local_state->recompiler->clearCodeCache(local_state);
        break;
    }
    default: {
        WARN("Unknown cont_type: %d", data.injected.cont_type);
        break;
    }
    }

    // Resumed by tracer...
    *guest_info = data.stop_info.info;
    ASSERT(data.stop_info.signal >= 0);
    sig = data.stop_info.signal;
    memset(&data.stop_info, 0, sizeof(data.stop_info));
    Signals::sigprocmask(local_state, SIG_SETMASK, &local_state->signal_mask, nullptr);

    // Address space could've been changed in many ways, not only via POKEDATA but also process_vm_writev
    // and even /proc/<pid>/mem or /proc/<pid>/task/<pid>/mem + write syscalls
    // Since we don't currently track any of these, invalidate the entire code cache after restart
    Recompiler::invalidateRangeGlobal(0, UINT64_MAX & ~0xFFFull, "ptrace stop resumed");

    local_state->recompiler->clearBreakpoints();
    u64 debug_control = local_state->ctx.debug_control;
    for (int i = 0; i < 4; i++) {
        u8 local_enabled = (debug_control >> (i * 2)) & 0b11;
        if (local_enabled == 0b00) {
            // Disabled...
        } else {
            u64 address = local_state->ctx.debug_register[i];
            u8 type = (debug_control >> (16 + 4 * i)) & 0b11;
            u8 length = (debug_control >> (18 + 4 * i)) & 0b11;
            if (type == 0b00) {
                if (length != 0) {
                    WARN("Breakpoint with length != 0?");
                }

                GUESTPTRACELOG("Added breakpoint at %lx", address);
                local_state->recompiler->addBreakpoint(i, address);
            } else {
                WARN("Ptrace watchpoint set at %lx but watchpoints are not currently supported", address);
            }
        }
    }

    if (local_state->ctx.tf != old_tf) {
        WARN("Trap flag changed during stop");
        felix86_tf_changed(local_state, local_state->ctx.tf);
    }
}

bool is_traced(ThreadState* state) {
    return state->ptrace_data.constants.tracer_pid != 0;
}
} // namespace Ptrace
