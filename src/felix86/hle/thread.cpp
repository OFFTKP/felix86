#include <atomic>
#include <csignal>
#include <dlfcn.h>
#include <fcntl.h>
#include <linux/futex.h>
#include <sys/mman.h>
#include <sys/personality.h>
#include <sys/poll.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include "felix86/common/exit.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/state.hpp"
#include "felix86/common/types.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/hle/fd.hpp"
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
    bool trace_clone = state->ptrace_data.constants.flags & PTRACE_O_TRACECLONE;

    if (clone_args.guest_flags & CLONE_SIGHAND) {
        // If CLONE_SIGHAND is set, the child and the parent share the same signal handler table
        ASSERT(clone_args.guest_flags & CLONE_VM);
        state->signal_table = clone_args.parent_state->signal_table;
    } else {
        // otherwise it gets a copy
        state->signal_table = SignalHandlerTable::create(clone_args.parent_state->signal_table);
    }

    // pthread_create may trample our signal handlers because it uses them for setxid/cancel but since
    // we don't care about those we'll trample them back with the guest signal handlers
    RegisteredSignal* sig32 = SignalHandlerTable::getRegisteredSignal(state->signal_table, 32);
    RegisteredSignal* sig33 = SignalHandlerTable::getRegisteredSignal(state->signal_table, 33);
    Signals::registerSignalHandler(state, 32, sig32->func, sig32->mask, sig32->flags, sig32->restorer);
    Signals::registerSignalHandler(state, 33, sig33->func, sig33->mask, sig33->flags, sig33->restorer);

    Signals::sigprocmask(state, SIG_SETMASK, &state->signal_mask, nullptr);

    int tid = gettid();
    if (clone_args.guest_flags & CLONE_CHILD_SETTID && clone_args.child_tid) {
        *clone_args.child_tid = tid;
    }

    if (clone_args.guest_flags & CLONE_PARENT_SETTID && clone_args.parent_tid) {
        *clone_args.parent_tid = tid;
    }

    if (clone_args.guest_flags & CLONE_PIDFD) {
        ERROR("CLONE_PIDFD in pthread_handler is not handled");
    }

    if (clone_args.guest_flags & CLONE_CHILD_CLEARTID) {
        state->clear_tid_address = clone_args.child_tid;
    }

    state->ctx.gprs[X86_REF_RAX] = 0; // return value
    state->ctx.rip = clone_args.new_rip;
    state->ctx.gprs[X86_REF_RSP] = clone_args.new_rsp;
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
    __atomic_store_n(finished, tid, __ATOMIC_SEQ_CST);

    LOG("Thread %ld started", tid);
    if (Ptrace::is_traced(state) && trace_clone) {
        int sig = SIGSTOP;
        siginfo_t info;
        memset(&info, 0, sizeof(siginfo_t));
        Ptrace::raise_stop(StopType::SignalDeliveryStop, sig, &info);
        if (sig != 0) {
            WARN("Clone SIGSTOP not skipped: %d", sig);
        }
    }

    Threads::StartThread(state);
    UNREACHABLE();
    return nullptr;
}

int clone_handler(void* args) {
    CloneArgs* clone_args = (CloneArgs*)args;
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
    if (!(host_clone_args.guest_flags & CLONE_THREAD)) {
        WARN("Starting thread with CLONE_VM but without CLONE_THREAD");
    }
    ASSERT(!(host_clone_args.guest_flags & CLONE_VFORK)); // should be handled in a vfork handler
    void* host_stack = malloc(1024 * 1024);

    if (host_clone_args.guest_flags & CLONE_PIDFD) {
        ERROR("CLONE_PIDFD in CloneMe is not handled");
    }

    bool trace_clone;
    ThreadState* state = ThreadState::Get();
    trace_clone = state->ptrace_data.constants.flags & PTRACE_O_TRACECLONE;
    if (Ptrace::is_traced(state) && trace_clone) {
        WARN("Process %d is traced and runs clone with uncommon flags %s, needs glibc fork to work", gettid(),
             flags_to_string(host_clone_args.guest_flags).c_str());
    }

    // We use this "tid" to check that the cloned process has finished
    pid_t clone_tid = -1;

    int host_flags = (host_clone_args.guest_flags & (~CLONE_SETTLS)) | CLONE_CHILD_CLEARTID;

    // Similar to fork handler, we need to signal to the tracer that the child SIGSTOP is to be skipped
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

    if (Ptrace::is_traced(state) && trace_clone) {
        int sig = SIGTRAP;
        siginfo_t info;
        memset(&info, 0, sizeof(siginfo_t));
        Ptrace::raise_stop(StopType::EventStop, sig, &info, PTRACE_EVENT_CLONE, result);
    }

    return result;
}

using pthread_attr_setsysflags_t = int (*)(pthread_attr_t*, size_t);

pthread_attr_setsysflags_t felix86_get_pthread_attr_setsysflags_ptr() {
    static void* pthread_attr_setsysflags_p = dlsym(RTLD_DEFAULT, "pthread_attr_setsysflags");
    if (!pthread_attr_setsysflags_p) {
        WARN_ONCE("pthread_attr_setsysflags not found, custom libc not present, using legacy clone implementation");
        return NULL;
    }
    return (pthread_attr_setsysflags_t)pthread_attr_setsysflags_p;
}

long NewCloneMe(CloneArgs& host_clone_args) {
    // We use a custom pthread_attr_t in our custom libc. It makes use of a `void* unused` field in pthread_attr_t
    // so as to not disturb the size. This way the usage of pthread_attr_t here allocates enough stack space.
    // This way we can use the TLS created by the libc via pthread_create and also modify the flags to be accurate on the guest side.
    // The custom libc is installed by default via the installation script, but if not present the code will fallback to the old
    // method using CloneMe and work as before
    ASSERT(host_clone_args.guest_flags & CLONE_VM);
    u64 pthread_create_flags =
        CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SYSVSEM | CLONE_SIGHAND | CLONE_THREAD | CLONE_SETTLS | CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    if (host_clone_args.guest_flags == pthread_create_flags) {
        // Great, just passthrough to host
    } else {
        // Try to use our custom glibc function to manipulate the pthread_create clone flags
        pthread_attr_setsysflags_t pthread_attr_setsysflags = felix86_get_pthread_attr_setsysflags_ptr();
        if (!pthread_attr_setsysflags) {
            // Not using the custom glibc, use the old method
            pthread_attr_destroy(&attr);
            return CloneMe(host_clone_args);
        }

        std::string sflags = flags_to_string(host_clone_args.guest_flags);
        WARN("Using pthread_attr_setsysflags for clone with flags: %s", sflags.c_str());
        u64 host_flags = host_clone_args.guest_flags | CLONE_SETTLS; // Always set a new host-side TLS
        pthread_attr_setsysflags(&attr, host_flags);
    }

    pthread_t thread;
    ThreadState* state = ThreadState::Get();
    bool trace_clone = state->ptrace_data.constants.flags & PTRACE_O_TRACECLONE;
    state->ptrace_data.stop_info.in_clone = true;
    pthread_create(&thread, &attr, pthread_handler, &host_clone_args);
    state->ptrace_data.stop_info.in_clone = false;
    pthread_detach(thread);

    pthread_attr_destroy(&attr);

    // Wait for the pthread_handler to finish initialization and set this flag
    while (!__atomic_load_n(&host_clone_args.new_tid, __ATOMIC_SEQ_CST))
        ;

    if (Ptrace::is_traced(state) && trace_clone) {
        int sig = SIGTRAP;
        siginfo_t info;
        memset(&info, 0, sizeof(siginfo_t));
        Ptrace::raise_stop(StopType::EventStop, sig, &info, PTRACE_EVENT_CLONE, host_clone_args.new_tid);
    }

    return host_clone_args.new_tid;
}

long ForkMe(CloneArgs& host_clone_args) {
    // If the child_stack argument is NULL, we need to handle it specially. The `clone` function can't take a null child_stack, we have to use
    // the syscall. Per the clone man page: Another difference for sys_clone is that the child_stack argument may be zero, in which case
    // copy-on-write semantics ensure that the child gets separate copies of stack pages when either process modifies the stack. In this case,
    // for correct operation, the CLONE_VM option should not be specified.
    ASSERT(!(host_clone_args.guest_flags & CLONE_VM));
    ASSERT(!(host_clone_args.guest_flags & CLONE_VFORK));
    int parent_pid = getpid();
    ThreadState* state = ThreadState::Get();
    u64 parent_flags = state->ptrace_data.constants.flags;
    bool trace_fork = parent_flags & PTRACE_O_TRACEFORK;
    // By setting ThreadState to null temporarily the tracer will know to skip the SIGSTOP the child starts with which will be re-raised
    // with raise_stop later after everything is initialized
    state->ptrace_data.stop_info.in_clone = true;
    long ret = syscall(SYS_clone, host_clone_args.guest_flags, nullptr, host_clone_args.parent_tid, host_clone_args.child_tid,
                       nullptr); // args are flipped in syscall
    state->ptrace_data.stop_info.in_clone = false;
    ASSERT(ret >= 0);

    long result;
    if (ret == 0) {
        // Start the child at the instruction after the syscall
        result = 0;
        // Destroy all states except the current state
        ASSERT(std::erase(g_process_globals.states, state) == 1);
        g_process_globals.initialize(); // New memory space, reinitialize the process globals
        g_process_globals.states.push_back(state);

        if (host_clone_args.new_rsp) {
            state->ctx.gprs[X86_REF_RSP] = host_clone_args.new_rsp;
        }

        if (host_clone_args.guest_flags & CLONE_SETTLS) {
            state->SetTLS(host_clone_args.new_tls);
        }

        // it's fine to just return to felix86_syscall, which will set the result to 0 and continue execution
        // in this new process
        int pid = getpid();
        SIGLOG("%d forked to %d", parent_pid, pid);

        if (Ptrace::is_traced(state) && trace_fork) {
            int sig = SIGSTOP;
            siginfo_t info;
            memset(&info, 0, sizeof(siginfo_t));
            Ptrace::raise_stop(StopType::SignalDeliveryStop, sig, &info);
            if (sig != 0) {
                WARN("Fork SIGSTOP not skipped: %d", sig);
            }
        }
    } else {
        ThreadState::Set(state);
        if (ret < 0) {
            ERROR("clone (probably fork) failed with %d", errno);
        }
        result = ret; // This process just continues normally

        if (Ptrace::is_traced(state) && trace_fork) {
            int sig = SIGTRAP;
            siginfo_t info;
            memset(&info, 0, sizeof(siginfo_t));
            Ptrace::raise_stop(StopType::EventStop, sig, &info, PTRACE_EVENT_FORK, ret);
        }
    }

    return result;
}

long VForkMe(CloneArgs& args) {
    // Thank you FEX
    // https://github.com/FEX-Emu/FEX/pull/2690
    int parent_pid = getpid();
    int pipes[2];
    ASSERT(pipe2(pipes, O_CLOEXEC) != -1);

    ThreadState* state = ThreadState::Get();
    u64 parent_flags = state->ptrace_data.constants.flags;
    bool trace_vfork = parent_flags & PTRACE_O_TRACEVFORK;
    bool trace_vfork_done = parent_flags & PTRACE_O_TRACEVFORKDONE;

    // Similar to fork handler, we need to signal to the tracer that the child SIGSTOP is to be skipped
    state->ptrace_data.stop_info.in_clone = true;
    long result = fork();
    state->ptrace_data.stop_info.in_clone = false;
    ASSERT(result >= 0);

    if (result == 0) {
        // Close the read end of the pipe.
        // Keep the write end open so the parent can poll it.
        close(pipes[0]);
        int pid = getpid();
        SIGLOG("%d vforked to %d", parent_pid, pid);
        ThreadState* state = ThreadState::Get();

        // TODO: probably clean up states here, but it doesn't matter cus it gets execve'd anyway
        if (args.new_rsp) {
            state->ctx.gprs[X86_REF_RSP] = args.new_rsp;
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

        if (Ptrace::is_traced(state) && trace_vfork) {
            int sig = SIGSTOP;
            siginfo_t info;
            memset(&info, 0, sizeof(siginfo_t));
            Ptrace::raise_stop(StopType::SignalDeliveryStop, sig, &info);
            if (sig != 0) {
                WARN("Vfork SIGSTOP not skipped: %d", sig);
            }
        }
    } else {
        int child_pid = result;
        if (Ptrace::is_traced(state) && trace_vfork) {
            int sig = SIGTRAP;
            siginfo_t info;
            memset(&info, 0, sizeof(siginfo_t));
            Ptrace::raise_stop(StopType::EventStop, sig, &info, PTRACE_EVENT_VFORK, child_pid);
        }

        if (args.guest_flags & CLONE_PIDFD) {
            ASSERT(!(args.guest_flags & CLONE_PARENT_SETTID));
            if (!args.parent_tid) {
                WARN("CLONE_PIDFD but no args.parent_tid?");
            } else {
                int fd = syscall(SYS_pidfd_open, child_pid, 0);
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

        if (Ptrace::is_traced(state) && trace_vfork_done) {
            int sig = SIGTRAP;
            siginfo_t info;
            memset(&info, 0, sizeof(siginfo_t));
            Ptrace::raise_stop(StopType::EventStop, sig, &info, PTRACE_EVENT_VFORK_DONE, child_pid);
        }
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

    // This would copy the stack on the cloned child which would cause chaos
    if (args->new_rsp == 0 && clone_vm) {
        WARN("CLONE_VM with new_rsp == 0");
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
    } else if (!clone_vm) {
        result = ForkMe(*args);
    } else {
        result = NewCloneMe(*args);
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
        base = (u8*)g_mapper->map(mode32, (void*)stack_hint, max_stack_size, PROT_NONE,
                                  MAP_PRIVATE | MAP_FIXED_NOREPLACE | MAP_ANONYMOUS | MAP_GROWSDOWN | MAP_NORESERVE, -1, 0);
        if (base != MAP_FAILED) {
            break;
        }

        stack_hint -= max_stack_size;

        if (attempts++ >= max_attempts) {
            ERROR("Failed to allocate stack, ran out of attempts");
        }
    }

    u8* stack_pointer = (u8*)g_mapper->map(mode32, base + max_stack_size - stack_size, stack_size, PROT_READ | PROT_WRITE,
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