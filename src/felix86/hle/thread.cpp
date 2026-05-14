#include <atomic>
#include <csignal>
#include <fcntl.h>
#include <linux/futex.h>
#include <sys/mman.h>
#include <sys/personality.h>
#include <sys/poll.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include "felix86/common/exit.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/state.hpp"
#include "felix86/common/types.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/hle/mmap.hpp"
#include "felix86/hle/signals.hpp"
#include "felix86/hle/thread.hpp"
#include "felix86/v2/recompiler.hpp"

struct StackTLS {
    void* stack;
    void* tls;
};

__attribute__((noreturn)) __attribute__((naked)) void* steal_stack_and_tls_and_exit(void* pointer) {
    asm(R"(
        sd tp, 8(a0)
        sd sp, 0(a0)
        li a0, 0
        li a7, 93
        ecall
    )");
}

int clone_handler(void* args) {
    CloneArgs* clone_args = (CloneArgs*)args;

    ThreadState* state;
    bool same_vm = clone_args->guest_flags & CLONE_VM;
    if (!same_vm) {
        state = ThreadState::Get();
        ASSERT(std::erase(g_process_globals.states, state) == 1);
        g_process_globals.initialize(); // New memory space, reinitialize the process globals
        g_process_globals.states.push_back(state);
    } else {
        state = ThreadState::Create(clone_args->parent_state);
    }

    if (clone_args->guest_flags & CLONE_SIGHAND) {
        // If CLONE_SIGHAND is set, the child and the parent share the same signal handler table
        ASSERT(clone_args->guest_flags & CLONE_VM);
        state->signal_table = clone_args->parent_state->signal_table;
    } else {
        // otherwise it gets a copy
        state->signal_table = SignalHandlerTable::Create(clone_args->parent_state->signal_table);
    }

    Signals::sigprocmask(state, SIG_SETMASK, &state->signal_mask, nullptr);

    int tid = gettid();
    if (clone_args->guest_flags & CLONE_CHILD_SETTID && clone_args->child_tid) {
        *clone_args->child_tid = tid;
    }

    if (clone_args->guest_flags & CLONE_PARENT_SETTID && clone_args->parent_tid) {
        *clone_args->parent_tid = tid;
    }

    if (clone_args->guest_flags & CLONE_PIDFD) {
        ERROR("CLONE_PIDFD in pthread_handler is not handled");
    }

    if (clone_args->guest_flags & CLONE_CHILD_CLEARTID) {
        state->clear_tid_address = clone_args->child_tid;
    }

    state->rip = clone_args->new_rip; // TODO: move in same_vm?
    if (clone_args->new_rsp) {
        state->gprs[X86_REF_RSP] = clone_args->new_rsp;
    } else {
        // Uses the parent stack
        ASSERT(!same_vm);
    }

    if (clone_args->guest_flags & CLONE_SETTLS) {
        state->SetTLS(clone_args->new_tls);
    } else if (clone_args->new_tls) {
        ERROR("TLS specified but CLONE_SETTLS not set");
    }

    // A child process created via fork(2) inherits a
    // copy of its parent's alternate signal stack settings.  The same
    // is also true for a child process created using clone(2), unless
    // the clone flags include CLONE_VM and do not include CLONE_VFORK,
    // in which case any alternate signal stack that was established in
    // the parent is disabled in the child process.
    if ((clone_args->guest_flags & CLONE_VM) && !(clone_args->guest_flags & CLONE_VFORK)) {
        state->alt_stack = {};
    }

    LOG("Process %ld started", tid);
    if (same_vm) {
        state->gprs[X86_REF_RAX] = 0; // return value for thread
        Threads::StartThread(state);
        UNREACHABLE();
    } else {
        // since we don't share a vm just return naturally
        return 0;
    }
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
    ASSERT(!(host_clone_args.guest_flags & CLONE_VFORK)); // should be handled in a vfork handler

    if (host_clone_args.guest_flags & CLONE_PIDFD) {
        ERROR("CLONE_PIDFD in CloneMe is not handled");
    }

    bool same_vm = host_clone_args.guest_flags & CLONE_VM;
    if (same_vm && host_clone_args.new_rsp == 0) {
        // Shared vm + stack would mean chaos
        // Maybe this is possible in vfork situations...
        ERROR("CLONE_VM and null stack, this is unexpected");
    }

    int host_flags = (host_clone_args.guest_flags & ~(CLONE_SETTLS | CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID | CLONE_PARENT_SETTID));
    void* new_stack = nullptr;
    void* new_tls = nullptr;
    if (same_vm) {
        StackTLS stacktls;
        pthread_t thread;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&thread, &attr, steal_stack_and_tls_and_exit, &stacktls);
        pthread_attr_destroy(&attr);
        new_stack = stacktls.stack;
        new_tls = stacktls.tls;
        host_flags |= CLONE_SETTLS;
        SIGLOG("Stolen stack: %lx, tls: %lx", new_stack, new_tls);
    } else {
        // Just use the parent stack/tls, since we are in a new address space there's no issue
    }

    long result = syscall(SYS_clone, host_flags, new_stack, nullptr, new_tls, nullptr);

    if (result < 0) {
        ERROR("clone failed with %d", errno);
    }

    if (result == 0) {
        clone_handler(&host_clone_args);
        return 0;
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
        SIGLOG("%d vforked to %d", parent_pid, getpid());
        ThreadState* state = ThreadState::Get();
        // TODO: probably clean up states here, but it doesn't matter cus it gets execve'd anyway
        if (args.new_rsp) {
            state->gprs[X86_REF_RSP] = args.new_rsp;
        }

        if (args.guest_flags & CLONE_SETTLS) {
            WARN("vfork giving us new TLS?");
            state->SetTLS(args.new_tls);
        }

        // If this happens we'd need to refactor our fork() call above probably
        if (args.child_tid) {
            WARN("vfork giving us child tid?");
        }

        // Wait for pidfd_open to finish
        pollfd pollfd{};
        pollfd.fd = pipes[0];
        pollfd.events = POLLIN | POLLOUT | POLLRDHUP | POLLERR | POLLHUP | POLLNVAL;
        sigset_t mask{};
        sigfillset(&mask);
        while (ppoll(&pollfd, 1, nullptr, &mask) == -1 && errno == EINTR)
            ;
    } else {
        if (args.guest_flags & CLONE_PIDFD) {
            ASSERT(!(args.guest_flags & CLONE_PARENT_SETTID));
            if (!args.parent_tid) {
                WARN("CLONE_PIDFD but no args.parent_tid?");
            } else {
                int fd = syscall(SYS_pidfd_open, result, 0);
                ASSERT_MSG(fd >= 0, "fd returned from pidfd_open is bad: %d", fd);
                *args.parent_tid = fd;
            }
        }

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
    u64 sus_flags = CLONE_UNTRACED | CLONE_NEWCGROUP | CLONE_NEWNS | CLONE_NEWNET | CLONE_NEWPID | CLONE_NEWUSER | CLONE_NEWUTS | CLONE_IO |
                    CLONE_NEWIPC | CLONE_PIDFD;
    if (args->guest_flags & sus_flags) {
        WARN("Clone with rare flags!!\nSP: %lx, TLS: %lx, Flags: %s", args->new_rsp, args->new_tls, sflags.c_str());
    }

    u64 allowed_flags = CLONE_VM | CLONE_THREAD | CLONE_DETACHED | CLONE_SYSVSEM | CLONE_CHILD_CLEARTID | CLONE_CHILD_SETTID | CLONE_SIGHAND |
                        CLONE_FILES | CLONE_FS | CLONE_IO | CLONE_SETTLS | CLONE_PARENT_SETTID | CLONE_VFORK | CLONE_UNTRACED | CLONE_NEWNS |
                        CLONE_NEWUSER | CLONE_NEWUTS | CLONE_NEWIPC | CLONE_NEWNET | CLONE_NEWPID | CLONE_PIDFD;
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
    state->recompiler->enterDispatcher(state);
}

int Threads::Unshare(int flags) {
    WARN("unshare(%s)", flags_to_string(flags).c_str());
    return ::unshare(flags);
}