#include <atomic>
#include <fcntl.h>
#include <linux/futex.h>
#include <sys/mman.h>
#include <sys/personality.h>
#include <sys/poll.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include "felix86/common/log.hpp"
#include "felix86/common/state.hpp"
#include "felix86/common/types.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/hle/mmap.hpp"
#include "felix86/hle/signals.hpp"
#include "felix86/hle/thread.hpp"
#include "felix86/v2/recompiler.hpp"

void* pthread_handler(void* args) {
    u32* finished;
    CloneArgs clone_args;
    {
        // Since this handler needs a pointer, and we pass a pointer to a stack variable,
        // we need to copy it and only allow the parent discard it when we're done.
        CloneArgs* copy_me = (CloneArgs*)args;
        clone_args = *copy_me;
        finished = &copy_me->new_tid;
    }

    ThreadState* state = ThreadState::Create(clone_args.parent_state);

    if (clone_args.guest_flags & CLONE_SIGHAND) {
        // If CLONE_SIGHAND is set, the child and the parent share the same signal handler table
        ASSERT(clone_args.guest_flags & CLONE_VM);
        state->signal_table = clone_args.parent_state->signal_table;
    } else {
        // otherwise it gets a copy
        state->signal_table = SignalHandlerTable::Create(clone_args.parent_state->signal_table);
    }

    state->tid = gettid();

    sigset_t mask;
    sigfillset(&mask);
    pthread_sigmask(SIG_UNBLOCK, &mask, nullptr);

    int res = prctl(PR_SET_NAME, (unsigned long)"ChildProcess", 0, 0, 0);
    if (res < 0) {
        ERROR("prctl failed with %d", errno);
    }

    if (clone_args.guest_flags & CLONE_CHILD_SETTID && clone_args.child_tid) {
        *clone_args.child_tid = state->tid;
    }

    if (clone_args.guest_flags & CLONE_PARENT_SETTID && clone_args.parent_tid) {
        *clone_args.parent_tid = state->tid;
    }

    if (clone_args.guest_flags & CLONE_CHILD_CLEARTID) {
        state->clear_tid_address = clone_args.child_tid;
    }

    state->gprs[X86_REF_RAX] = 0; // return value
    state->rip = clone_args.new_rip;
    state->gprs[X86_REF_RSP] = clone_args.new_rsp;
    state->thread = clone_args.new_thread;

    if (clone_args.guest_flags & CLONE_SETTLS) {
        state->SetTLS(clone_args.new_tls);
    } else if (clone_args.new_tls) {
        ERROR("TLS specified but CLONE_SETTLS not set");
    }

    // A child process created via fork(2) inherits a
    // copy of its parent's alternate signal stack settings.  The same
    // is also true for a child process created using clone(2), unless
    // the clone flags include CLONE_VM and do not include CLONE_VFORK,
    // in which case any alternate signal stack that was established in
    // the parent is disabled in the child process.
    if ((clone_args.guest_flags & CLONE_VM) && !(clone_args.guest_flags & CLONE_VFORK)) {
        state->alt_stack = {};
    }

    // Once we are finished with initialization we can signal to the parent thread that we are done
    std::atomic_signal_fence(std::memory_order_seq_cst); // Don't let the compiler reorder the copy after this fence
    __atomic_store_n(finished, state->tid, __ATOMIC_SEQ_CST);

    LOG("Thread %ld started", state->tid);
    Threads::StartThread(state);
    LOG("Thread %ld exited with reason: %s", state->tid, print_exit_reason(state->exit_reason));

    if (state->clear_tid_address) {
        __atomic_store_n(state->clear_tid_address, 0, __ATOMIC_SEQ_CST);
        syscall(SYS_futex, state->clear_tid_address, FUTEX_WAKE, ~0ULL, 0, 0, 0);
    }

    ThreadState::Destroy(state);

    return nullptr;
}

int clone_handler(void* args) {
    CloneArgs* clone_args = (CloneArgs*)args;
    int res = prctl(PR_SET_NAME, (unsigned long)"CloneHandler", 0, 0, 0);
    if (res < 0) {
        ERROR("prctl failed with %d", errno);
    }

    ASSERT(clone_args->guest_flags & CLONE_VM);

    // We can't use this cloned process, because when the guest created it, it passed a guest TLS which we can't use,
    // both due to differences in TLS and because the guest needs it, and creating a host TLS is not possible sans some hacky ways.
    // So we need to create a pthread (which will create a proper TLS) as the actual child process.
    pthread_create(&clone_args->new_thread, nullptr, pthread_handler, args);
    pthread_detach(clone_args->new_thread);

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

long CloneMe(CloneArgs& host_clone_args) {
    ASSERT(host_clone_args.guest_flags & CLONE_VM);
    ASSERT(!(host_clone_args.guest_flags & CLONE_VFORK)); // should be handled in a vfork handler
    void* host_stack = malloc(1024 * 1024);

    // We use this "tid" to check that the cloned process has finished
    pid_t clone_tid = -1;

    int host_flags = (host_clone_args.guest_flags & (~CLONE_SETTLS)) | CLONE_CHILD_CLEARTID;
    long result = clone(clone_handler, (u8*)host_stack + 1024 * 1024, host_flags, &host_clone_args, nullptr, nullptr, &clone_tid);

    // Wait for the clone_handler to finish
    syscall(SYS_futex, &clone_tid, FUTEX_WAIT, -1, nullptr, nullptr, 0);

    // Wait for the pthread_handler to finish initialization and set this flag
    while (!__atomic_load_n(&host_clone_args.new_tid, __ATOMIC_SEQ_CST))
        ;

    // This is finally safe to free
    free(host_stack);

    if (result < 0) {
        ERROR("clone failed with %d", errno);
    }

    // Return the tid of the new thread that was started inside the clone_handler
    result = host_clone_args.new_tid;

    return result;
}

long ForkMe(CloneArgs& host_clone_args) {
    // If the child_stack argument is NULL, we need to handle it specially. The `clone` function can't take a null child_stack, we have to use
    // the syscall. Per the clone man page: Another difference for sys_clone is that the child_stack argument may be zero, in which case
    // copy-on-write semantics ensure that the child gets separate copies of stack pages when either process modifies the stack. In this case,
    // for correct operation, the CLONE_VM option should not be specified.
    ASSERT(!(host_clone_args.guest_flags & CLONE_VM));
    ASSERT(!(host_clone_args.guest_flags & CLONE_VFORK));
    int parent_tid = host_clone_args.parent_state->tid;
    int parent_pid = getpid();
    long ret = syscall(SYS_clone, host_clone_args.guest_flags, nullptr, host_clone_args.parent_tid, host_clone_args.child_tid,
                       nullptr); // args are flipped in syscall

    long result;
    if (ret == 0) {
        // Start the child at the instruction after the syscall
        result = 0;
        ThreadState* state = ThreadState::Get();
        // Destroy all states except the current state
        ASSERT(std::erase(g_process_globals.states, state) == 1);
        g_process_globals.initialize(); // New memory space, reinitialize the process globals
        g_process_globals.states.push_back(state);

        if (host_clone_args.new_rsp) {
            state->gprs[X86_REF_RSP] = host_clone_args.new_rsp;
        }

        if (host_clone_args.guest_flags & CLONE_SETTLS) {
            state->SetTLS(host_clone_args.new_tls);
        }

        // it's fine to just return to felix86_syscall, which will set the result to 0 and continue execution
        // in this new process. Just give it a new name to make debugging easier
        std::string name = "ForkedFrom" + std::to_string(parent_tid); // forked from parent tid
        prctl(PR_SET_NAME, name.c_str(), 0, 0, 0);
        SIGLOG("%d forked to %d", parent_pid, getpid());
        state->tid = gettid();
    } else {
        if (ret < 0) {
            ERROR("clone (probably fork) failed with %d", errno);
        }
        result = ret; // This process just continues normally
    }

    return result;
}

long VForkMe(CloneArgs& args) {
    // Thank you FEX
    // https://github.com/FEX-Emu/FEX/pull/2690
    int parent_pid = getpid();
    int pipes[2];
    ASSERT(pipe2(pipes, O_CLOEXEC) != -1);

    long result = fork();

    if (result == 0) {
        // Close the read end of the pipe.
        // Keep the write end open so the parent can poll it.
        close(pipes[0]);
        std::string name = "VForkedFrom" + std::to_string(parent_pid);
        prctl(PR_SET_NAME, name.c_str(), 0, 0, 0);
        SIGLOG("%d vforked to %d", parent_pid, getpid());
        ThreadState* state = ThreadState::Get();
        // TODO: probably clean up states here, but it doesn't matter cus it gets execve'd anyway
        if (args.new_rsp) {
            state->gprs[X86_REF_RSP] = args.new_rsp;
        }

        state->tid = gettid();

        if (args.guest_flags & CLONE_SETTLS) {
            WARN("vfork giving us new TLS?");
            state->SetTLS(args.new_tls);
        }

        // If this happens we'd need to refactor our fork() call above probably
        if (args.child_tid) {
            WARN("vfork giving us child tid?");
        }

        if (args.parent_tid) {
            WARN("vfork giving us parent tid?");
        }
    } else {
        // Close the write end of the pipe.
        close(pipes[1]);

        pollfd pollfd{};
        pollfd.fd = pipes[0];
        pollfd.events = POLLIN | POLLOUT | POLLRDHUP | POLLERR | POLLHUP | POLLNVAL;

        // Mask all signals until the child process returns.
        sigset_t mask{};
        sigfillset(&mask);
        while (ppoll(&pollfd, 1, nullptr, &mask) == -1 && errno == EINTR)
            ;

        // Close the read end now.
        close(pipes[0]);
    }

    return result;
}

long Threads::Clone(ThreadState* current_state, CloneArgs* args) {
    std::string sflags = flags_to_string(args->guest_flags);
    STRACE("clone({%s}, stack: %llx, parid: %llx, ctid: %llx, tls: %llx)", sflags.c_str(), args->new_rsp, args->parent_tid, args->child_tid,
           args->new_tls);

    bool clone_fs = args->guest_flags & CLONE_FS;
    bool clone_vm = args->guest_flags & CLONE_VM;
    if (clone_fs && !clone_vm) {
        // This would be quite cursed, because we'd have to use IPC to communicate any FS changes to the processes
        // that share the FS info. Hopefully we won't reach this ever but add a warning here
        WARN("CLONE_FS encountered without CLONE_VM");
    }

    // Not very well tested flags, most programs don't use them, so print them every time for now
    u64 sus_flags = CLONE_UNTRACED | CLONE_NEWCGROUP | CLONE_NEWNS | CLONE_NEWNET | CLONE_NEWPID | CLONE_NEWUSER | CLONE_NEWUTS | CLONE_IO;
    if (args->guest_flags & sus_flags) {
        WARN("Clone with rare flags!!\nSP: %lx, TLS: %lx, Flags: %s", args->new_rsp, args->new_tls, sflags.c_str());
    }

    u64 allowed_flags = CLONE_VM | CLONE_THREAD | CLONE_DETACHED | CLONE_SYSVSEM | CLONE_CHILD_CLEARTID | CLONE_CHILD_SETTID | CLONE_SIGHAND |
                        CLONE_FILES | CLONE_FS | CLONE_IO | CLONE_SETTLS | CLONE_PARENT_SETTID | CLONE_VFORK | CLONE_UNTRACED | CLONE_NEWNS |
                        CLONE_NEWUSER | CLONE_NEWUTS | CLONE_NEWNET | CLONE_NEWPID;
    if ((args->guest_flags & ~CSIGNAL) & ~allowed_flags) {
        ERROR("Unsupported flags %s", sflags.c_str());
        return -ENOSYS;
    }

    long result;

    if (args->guest_flags & CLONE_VFORK) {
        // 99% of the time you get these flags because that's what you get when you run the vfork() function
        // But it's of course possible to run clone() with more flags than that and CLONE_VFORK, but warn if that happens
        if (args->guest_flags != (CLONE_VM | CLONE_VFORK | SIGCLD)) {
            WARN("CLONE_VFORK with %s", flags_to_string(args->guest_flags).c_str());
        }

        result = VForkMe(*args);
    } else if (args->new_rsp == 0 || !(args->guest_flags & CLONE_VM)) {
        result = ForkMe(*args);
    } else {
        result = CloneMe(*args);
    }

    return result;
}

std::pair<u8*, size_t> Threads::AllocateStack(bool mode32) {
    struct rlimit stack_limit = {0};
    if (getrlimit(RLIMIT_STACK, &stack_limit) == -1) {
        ERROR("Failed to get stack size limit");
    }

    u64 stack_size = stack_limit.rlim_cur;
    if (stack_size == RLIM_INFINITY) {
        stack_size = 8 * 1024 * 1024;
    }

    u64 max_stack_size = stack_limit.rlim_max;
    if (max_stack_size == RLIM_INFINITY) {
        max_stack_size = 16 * 1024 * 1024;
    }

    max_stack_size &= ~0xFFF; // Make sure we are aligned

    u64 stack_hint;
    if (mode32) {
        stack_hint = 0xBFFF'F000 - max_stack_size;
    } else {
        // Randomish hint. Needs to be below 0x3f'ffff'ffff however as that is the lowest possible
        // user-space virtual memory (the one in Kernel SV39).
        stack_hint = 0x2A'FFFF'F000 - max_stack_size;
    }

    u8* base;
    int attempts = 0;
    int max_attempts = 14;

    while (true) {
        VERBOSE("Attempting to allocate stack on %p", (void*)stack_hint);
        base = (u8*)g_mapper->map((void*)stack_hint, max_stack_size, PROT_NONE,
                                  MAP_PRIVATE | MAP_FIXED_NOREPLACE | MAP_ANONYMOUS | MAP_GROWSDOWN | MAP_NORESERVE, -1, 0);
        if (base != MAP_FAILED) {
            break;
        }

        stack_hint -= max_stack_size;

        if (attempts++ >= max_attempts) {
            ERROR("Failed to allocate stack, ran out of attempts");
        }
    }

    u8* stack_pointer = (u8*)g_mapper->map(base + max_stack_size - stack_size, stack_size, PROT_READ | PROT_WRITE,
                                           MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS | MAP_GROWSDOWN, -1, 0);
    if (stack_pointer == MAP_FAILED) {
        ERROR("Failed to allocate stack");
    }

    if (mode32) {
        ASSERT((u64)stack_pointer < UINT32_MAX);
    }

    VERBOSE("Allocated stack at %p", base);
    stack_pointer += stack_size;
    VERBOSE("Stack pointer at %p", stack_pointer);

    SMCLOG("Allocated stack: %lx-%lx", stack_pointer, stack_pointer + max_stack_size);
    return {stack_pointer, max_stack_size};
}

void Threads::StartThread(ThreadState* state) {
    state->tid = gettid();
    state->recompiler->enterDispatcher(state);
    VERBOSE("Thread exited with reason %s", print_exit_reason(state->exit_reason));
}

int Threads::Unshare(int flags) {
    WARN("unshare(%s)", flags_to_string(flags).c_str());
    return ::unshare(flags);
}