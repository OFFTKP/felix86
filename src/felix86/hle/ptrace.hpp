#pragma once
#include <csignal>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/types.h>
#include "felix86/common/types.hpp"

#define FELIX86_PTRACE_SIGNAL 53
#define FELIX86_PTRACE_CODE_INSTOP -0x5702
#define FELIX86_PTRACE_CODE_DEFER -0xDEF

// Copied from x86 ptrace.h file as there's differences with RISC-V
enum class felix86_ptrace_request {
    felix86_PTRACE_TRACEME = 0,
    felix86_PTRACE_PEEKTEXT = 1,
    felix86_PTRACE_PEEKDATA = 2,
    felix86_PTRACE_PEEKUSER = 3,
    felix86_PTRACE_POKETEXT = 4,
    felix86_PTRACE_POKEDATA = 5,
    felix86_PTRACE_POKEUSER = 6,
    felix86_PTRACE_CONT = 7,
    felix86_PTRACE_KILL = 8,
    felix86_PTRACE_SINGLESTEP = 9,
    felix86_PTRACE_GETREGS = 12,
    felix86_PTRACE_SETREGS = 13,
    felix86_PTRACE_GETFPREGS = 14,
    felix86_PTRACE_SETFPREGS = 15,
    felix86_PTRACE_ATTACH = 16,
    felix86_PTRACE_DETACH = 17,
    felix86_PTRACE_GETFPXREGS = 18,
    felix86_PTRACE_SETFPXREGS = 19,
    felix86_PTRACE_SYSCALL = 24,
    felix86_PTRACE_GET_THREAD_AREA = 25,
    felix86_PTRACE_SET_THREAD_AREA = 26,
    felix86_PTRACE_ARCH_PRCTL = 30,
    felix86_PTRACE_SYSEMU = 31,
    felix86_PTRACE_SYSEMU_SINGLESTEP = 32,
    felix86_PTRACE_SINGLEBLOCK = 33,
    felix86_PTRACE_SETOPTIONS = 0x4200,
    felix86_PTRACE_GETEVENTMSG = 0x4201,
    felix86_PTRACE_GETSIGINFO = 0x4202,
    felix86_PTRACE_SETSIGINFO = 0x4203,
    felix86_PTRACE_GETREGSET = 0x4204,
    felix86_PTRACE_SETREGSET = 0x4205,
    felix86_PTRACE_SEIZE = 0x4206,
    felix86_PTRACE_INTERRUPT = 0x4207,
    felix86_PTRACE_LISTEN = 0x4208,
    felix86_PTRACE_PEEKSIGINFO = 0x4209,
    felix86_PTRACE_GETSIGMASK = 0x420a,
    felix86_PTRACE_SETSIGMASK = 0x420b,
    felix86_PTRACE_SECCOMP_GET_FILTER = 0x420c,
    felix86_PTRACE_SECCOMP_GET_METADATA = 0x420d,
    felix86_PTRACE_GET_SYSCALL_INFO = 0x420e,
    felix86_PTRACE_GET_RSEQ_CONFIGURATION = 0x420f,
    felix86_PTRACE_SET_SYSCALL_USER_DISPATCH_CONFIG = 0x4210,
    felix86_PTRACE_GET_SYSCALL_USER_DISPATCH_CONFIG = 0x4211
};

enum class StopType : u64 {
    NoStop = 0,
    SignalDeliveryStop = 0x4001,
    SyscallEnterStop,
    SyscallExitStop,
    EventStop,
};

struct ThreadState;

// TODO: find which are unused or can be removed
struct PtraceData {
    // Written by tracee once during init
    struct {
        pid_t tracer_pid;
        pid_t my_tgid;
        pid_t my_tid;
        u64 flags;
    } constants;

    // Written by tracee, read by tracer
    struct {
        bool in_clone;
        bool stopped;
        bool mode32;
        StopType type;
        siginfo_t info;
        int signal;
        u8 ptrace_event;
        u64 event_msg;
        u64 sigmask;
    } stop_info;

    // See FELIX86_PTRACE_CODE_DEFER
    // Written by tracer, read by tracee, except for deferred
    struct {
        bool deferred;
        int original_sig;
        siginfo_t original_info;
        u64 original_mask;
    } force_defer;

    struct {
        u64 nr;
        u64 args[6];
        u64 ret;
        bool is_error;
    } syscall_info;

    // Written by tracer, read by tracee
    struct {
        int cont_type;
        int signal;
        siginfo_t info;
    } injected;
};

namespace Ptrace {
int wait4(pid_t pid, int* status, int flags, struct rusage* ru);
i64 sys_ptrace(felix86_ptrace_request op, pid_t pid, void* addr, void* data);
void raise_stop(StopType stop, int& sig, siginfo_t* guest_info, int event = 0, u64 event_msg = 0);
bool is_traced(ThreadState* state);
} // namespace Ptrace