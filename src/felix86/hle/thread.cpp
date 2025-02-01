#include <sys/mman.h>
#include <sys/personality.h>
#include <sys/resource.h>
#include "felix86/common/log.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/emulator.hpp"
#include "felix86/hle/thread.hpp"

void start_thread_wrapper(ThreadState* new_state) {
    new_state->tid = gettid();
    VERBOSE("Created thread state with tid %ld", new_state->tid);
    pthread_setname_np(pthread_self(), "ChildProcess");
    g_emulator->StartThread(new_state);
    g_emulator->RemoveState(new_state);
    // TODO: cleanup stack
}

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

long Threads::Clone(ThreadState* current_state, clone_args* args) {
    std::string sflags = flags_to_string(args->flags);
    STRACE("clone({%s}, %llx, %llx, %llx, %llx)", sflags.c_str(), args->stack, args->parent_tid, args->child_tid, args->tls);

    u64 flags = args->flags & ~CSIGNAL;
    u64 allowed_flags = CLONE_VM | CLONE_THREAD | CLONE_SYSVSEM | CLONE_VFORK | CLONE_CHILD_CLEARTID | CLONE_CHILD_SETTID | CLONE_SIGHAND |
                        CLONE_FILES | CLONE_FS | CLONE_IO | CLONE_SETTLS | CLONE_PARENT_SETTID;
    if (flags & ~allowed_flags) {
        ERROR("Unsupported flags %016lx", flags & ~allowed_flags);
        return -EINVAL;
    }

    ThreadState* new_state = g_emulator->CreateThreadState(current_state);

    new_state->gprs[X86_REF_RSP] = args->stack;
    new_state->gprs[X86_REF_RAX] = 0;                  // return value
    new_state->rip = current_state->gprs[X86_REF_RCX]; // rip after syscall is stored to rcx when syscall is called

    if (args->flags & CLONE_SETTLS) {
        new_state->fsbase = args->tls;
    } else if (args->tls) {
        ERROR("TLS specified but CLONE_SETTLS not set");
    }

    // A child process created via fork(2) inherits a
    // copy of its parent's alternate signal stack settings.  The same
    // is also true for a child process created using clone(2), unless
    // the clone flags include CLONE_VM and do not include CLONE_VFORK,
    // in which case any alternate signal stack that was established in
    // the parent is disabled in the child process.
    if ((args->flags & CLONE_VM) && !(args->flags & CLONE_VFORK)) {
        new_state->alt_stack = {};
    }

    if (args->flags & CLONE_SIGHAND) {
        // If CLONE_SIGHAND is set, the child and the parent share the same signal handler table
        ASSERT(args->flags & CLONE_VM);
        new_state->signal_handlers = current_state->signal_handlers;
    } else {
        // otherwise it gets a copy
        new_state->signal_handlers = std::make_shared<SignalHandlerTable>(*current_state->signal_handlers);
    }

    bool has_stack = args->stack != 0;
    long result;

    u64 host_flags = args->flags;
    host_flags &= ~CLONE_SETTLS;
    if (has_stack) {
        void* my_stack = malloc(1024 * 1024);
        result = clone((int (*)(void*))start_thread_wrapper, (u8*)my_stack + 1024 * 1024, host_flags, new_state, args->parent_tid, nullptr,
                       args->child_tid);
    } else {
        result = clone((int (*)(void*))start_thread_wrapper, nullptr, host_flags, new_state, args->parent_tid, nullptr, args->child_tid);
    }

    return result;
}

std::pair<u8*, size_t> Threads::AllocateStack(size_t size) {
    struct rlimit stack_limit = {0};
    if (getrlimit(RLIMIT_STACK, &stack_limit) == -1) {
        ERROR("Failed to get stack size limit");
    }

    u64 stack_size = stack_limit.rlim_cur;
    if (stack_size == RLIM_INFINITY) {
        stack_size = 8 * 1024 * 1024;
    }

    u64 max_stack_size = size == 0 ? stack_limit.rlim_max : size;
    if (max_stack_size == RLIM_INFINITY) {
        max_stack_size = 128 * 1024 * 1024;
    }

    u64 stack_hint = 0x7FFFFFFFF000 - max_stack_size;

    u8* base =
        (u8*)mmap((void*)stack_hint, max_stack_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK | MAP_GROWSDOWN | MAP_NORESERVE, -1, 0);
    if (base == MAP_FAILED) {
        ERROR("Failed to allocate stack");
    }

    u8* stack_pointer = (u8*)mmap(base + max_stack_size - stack_size, stack_size, PROT_READ | PROT_WRITE,
                                  MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK | MAP_GROWSDOWN, -1, 0);
    if (stack_pointer == MAP_FAILED) {
        ERROR("Failed to allocate stack");
    }
    VERBOSE("Allocated stack at %p", base);
    stack_pointer += stack_size;
    VERBOSE("Stack pointer at %p", stack_pointer);

    return {stack_pointer, max_stack_size};
}