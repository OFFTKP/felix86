#include "felix86/common/log.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/emulator.hpp"
#include "felix86/hle/thread.hpp"

#ifndef CLONE_CLEAR_SIGHAND
#define CLONE_CLEAR_SIGHAND 0x100000000ULL
#endif

#ifndef CLONE_INTO_CGROUP
#define CLONE_INTO_CGROUP 0x200000000ULL
#endif

static std::string flags_to_string(u64 f) {
#define add(x)                                                                                                                                       \
    if (f & x) {                                                                                                                                     \
        flags += #x ", ";                                                                                                                            \
    }

    std::string flags;
    add(CLONE_VM);
    add(CLONE_FS);
    add(CLONE_FILES);
    add(CLONE_SIGHAND);
    add(CLONE_PIDFD);
    add(CLONE_PTRACE);
    add(CLONE_VFORK);
    add(CLONE_PARENT);
    add(CLONE_THREAD);
    add(CLONE_NEWNS);
    add(CLONE_SYSVSEM);
    add(CLONE_SETTLS);
    add(CLONE_PARENT_SETTID);
    add(CLONE_CHILD_CLEARTID);
    add(CLONE_DETACHED);
    add(CLONE_UNTRACED);
    add(CLONE_CHILD_SETTID);
    add(CLONE_NEWCGROUP);
    add(CLONE_NEWUTS);
    add(CLONE_NEWIPC);
    add(CLONE_NEWUSER);
    add(CLONE_NEWPID);
    add(CLONE_NEWNET);
    add(CLONE_IO);
    add(CLONE_CLEAR_SIGHAND);
    add(CLONE_INTO_CGROUP);

    // Make sure we didn't miss any flags that are added in the future
    u64 mask = (0x200000000ULL << 1) - 1;
    ASSERT((f & ~mask) == 0);

    if (!flags.empty()) {
        // Remove the last ", "
        flags.pop_back();
        flags.pop_back();
    }

    return flags;
}

long Threads::Clone3(ThreadState* current_state, clone_args* args) {
    exit(1);
}

long Threads::Clone(ThreadState* current_state, clone_args* args) {
    std::string sflags = flags_to_string(args->flags);
    STRACE("clone({%s}, %llx, %llx, %llx, %llx)", sflags.c_str(), args->stack, args->parent_tid, args->child_tid, args->tls);

    u64 flags = args->flags & ~CSIGNAL;
    u64 allowed_flags = CLONE_VM | CLONE_VFORK | CLONE_CHILD_CLEARTID | CLONE_CHILD_SETTID;
    if (flags & ~allowed_flags) {
        ERROR("Unsupported flags %016lx", flags & ~allowed_flags);
        return -EINVAL;
    }

    ThreadState* new_state = g_emulator->CreateThreadState();
    new_state->gprs[X86_REF_RSP] = args->stack;
    new_state->rip = current_state->gprs[X86_REF_RCX]; // rip after syscall is stored to rcx when syscall is called
    new_state->fsbase = args->tls;

    long result = syscall(SYS_clone, args->flags, args->stack, args->parent_tid, args->child_tid, args->tls);

    if (result == 0) {
        // Start the child at the instruction after the syscall
        g_emulator->StartThread(new_state);
        UNREACHABLE();
    }

    return result;
}