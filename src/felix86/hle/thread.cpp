#include <linux/futex.h>
#include <sys/mman.h>
#include <sys/personality.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include "felix86/common/log.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/emulator.hpp"
#include "felix86/hle/thread.hpp"

struct CloneArgs {
    ThreadState* new_state = nullptr;
    void* stack = nullptr;
    u64 flags = 0;
    pid_t* parent_tid = nullptr;
    pid_t* child_tid = nullptr;
    bool finished = false; // to signal that clone_handler has finished using the pointer
};

void* pthread_handler(void* args) {
    bool* finished;
    CloneArgs clone_args;
    {
        // Since this handler needs a pointer, and we pass a pointer to a stack variable,
        // we need to copy it and only allow the parent discard it when we're done.
        CloneArgs* copy_me = (CloneArgs*)args;
        clone_args = *copy_me;
        finished = &copy_me->finished;
    }

    ThreadState* state = clone_args.new_state;
    state->tid = gettid();

    sigset_t mask;
    sigemptyset(&mask);
    sigprocmask(SIG_SETMASK, &mask, nullptr);

    int res = prctl(PR_SET_NAME, (unsigned long)"ChildProcess", 0, 0, 0);
    if (res < 0) {
        ERROR("prctl failed with %d", errno);
    }

    if (clone_args.flags & CLONE_CHILD_SETTID && clone_args.child_tid) {
        *clone_args.child_tid = state->tid;
    }

    if (clone_args.flags & CLONE_PARENT_SETTID && clone_args.parent_tid) {
        *clone_args.parent_tid = state->tid;
    }

    if (clone_args.flags & CLONE_CHILD_CLEARTID) {
        state->clear_tid_address = clone_args.child_tid;
    }

    // Once we are finished with initialization we can signal to the parent thread that we are done
    std::atomic_signal_fence(std::memory_order_seq_cst); // Don't let the compiler reorder the copy after this fence
    __atomic_thread_fence(__ATOMIC_SEQ_CST);             // Don't reorder the store at runtime (probably unnecessary since store is seq_cst)
    __atomic_store_n(finished, true, __ATOMIC_SEQ_CST);

    LOG("Thread %ld started", state->tid);
    pthread_setname_np(state->thread, "ChildProcess");
    g_emulator->StartThread(state);
    LOG("Thread %ld exited", state->tid);

    __atomic_store_n(state->clear_tid_address, 0, __ATOMIC_SEQ_CST);
    syscall(SYS_futex, state->clear_tid_address, FUTEX_WAKE, 1, 0, 0, 0);
    g_emulator->RemoveState(state);

    return nullptr;
}

int clone_handler(void* args) {
    CloneArgs* clone_args = (CloneArgs*)args;
    ThreadState* state = clone_args->new_state;
    int res = prctl(PR_SET_NAME, (unsigned long)"CloneHandler", 0, 0, 0);
    if (res < 0) {
        ERROR("prctl failed with %d", errno);
    }

    // We can't use this cloned process, because when the guest created it, it passed a guest TLS which we can't use,
    // both due to differences in TLS and because the guest needs it, and creating a host TLS is not possible sans some hacky ways.
    // So we need to create a pthread (which will create a proper TLS) as the actual child process.
    pthread_create(&state->thread, nullptr, pthread_handler, args);
    pthread_detach(state->thread);

    return 0;
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
    STRACE("clone({%s}, stack: %llx, parid: %llx, ctid: %llx, tls: %llx)", sflags.c_str(), args->stack, args->parent_tid, args->child_tid, args->tls);

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

    u64 host_flags = args->flags;
    host_flags &= ~CLONE_SETTLS;

    long result;

    CloneArgs host_clone_args{
        .new_state = new_state,
        .stack = nullptr,
        .flags = host_flags,
        .parent_tid = (pid_t*)args->parent_tid,
        .child_tid = (pid_t*)args->child_tid,
        .finished = false,
    };

    if (args->stack == 0) {
        ERROR("FIXME");
        // If the child_stack argument is NULL, we need to handle it specially. The `clone` function can't take a null child_stack, we have to use
        // the syscall. Per the clone man page: Another difference for sys_clone is that the child_stack argument may be zero, in which case
        // copy-on-write semantics ensure that the child gets separate copies of stack pages when either process modifies the stack. In this case,
        // for correct operation, the CLONE_VM option should not be specified.
        new_state->gprs[X86_REF_RSP] = current_state->gprs[X86_REF_RSP];
        long ret = syscall(SYS_clone, host_flags, nullptr, args->parent_tid, args->child_tid, nullptr); // args are flipped in syscall

        if (ret == 0) {
            result = 0;
            clone_handler(&host_clone_args); // TODO: probably wrong. can just start normally as its a different process, malloc should be fine we
                                             // dont need a separate tls i think
            UNREACHABLE();
        } else {
            result = ret;
        }
    } else {
        host_clone_args.stack = malloc(1024 * 1024);
        result = clone(clone_handler, (u8*)host_clone_args.stack + 1024 * 1024, host_flags, &host_clone_args, 0, 0, 0);
    }

    // Wait for the pthread to finish initialization
    while (!__atomic_load_n(&host_clone_args.finished, __ATOMIC_SEQ_CST))
        ;

    free(host_clone_args.stack);

    if (result < 0) {
        ERROR("clone failed with %ld", errno);
    }

    // Return the tid of the new thread that was started inside the clone_handler
    result = new_state->tid;

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