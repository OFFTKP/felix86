#include <csignal>
#include <vector>
#include <elf.h>
#include <fcntl.h>
#include <fmt/base.h>
#include <fmt/format.h>
#include <stdlib.h>
#include <sys/random.h>
#include "felix86/emulator.hpp"
#include "felix86/v2/recompiler.hpp"

extern char** environ;

static char x86_64_string[] = "x86_64";

u64 stack_push(u64 stack, u64 value) {
    stack -= 8;
    *(u64*)stack = value;
    return stack;
}

u64 stack_push_string(u64 stack, const char* str) {
    u64 len = strlen(str) + 1;
    stack -= len;
    strcpy((char*)stack, str);
    return stack;
}

typedef struct {
    int a_type;

    union {
        u64 a_val;
        void* a_ptr;
        void (*a_fnc)();
    } a_un;
} auxv_t;

void Emulator::Run() {
    VERBOSE("Executable: %016lx - %016lx", g_executable_start, g_executable_end);
    if (g_interpreter_start) {
        VERBOSE("Interpreter: %016lx - %016lx", g_interpreter_start, g_interpreter_end);
    }

    if (!g_testing) {
        VERBOSE("Entrypoint: %016lx", (u64)fs.GetEntrypoint());
    }

    VERBOSE("Entering main thread :)");

    StartThread(ThreadState::Get());

    VERBOSE("Bye-bye main thread :(");
}

void Emulator::setupMainStack(ThreadState* state) {
    ssize_t argc = config.argv.size();
    if (argc > 1) {
        VERBOSE("Passing %zu arguments to guest executable", argc - 1);
        for (ssize_t i = 1; i < argc; i++) {
            VERBOSE("Guest argument %zu: %s", i, config.argv[i].c_str());
        }
    }

    const char* path = config.argv[0].c_str();

    std::shared_ptr<Elf> elf = fs.GetExecutable();

    // Initial process stack according to System V AMD64 ABI
    u64 rsp = (u64)elf->GetStackPointer();

    // To hold the addresses of the arguments for later pushing
    u64* argv_addresses = (u64*)alloca(argc * sizeof(u64));

    rsp = stack_push_string(rsp, path);
    const char* program_name = (const char*)rsp;

    rsp = stack_push_string(rsp, x86_64_string);
    const char* platform_name = (const char*)rsp;

    for (ssize_t i = 0; i < argc; i++) {
        rsp = stack_push_string(rsp, config.argv[i].c_str());
        argv_addresses[i] = rsp;
    }

    size_t envc = config.envp.size();
    u64* envp_addresses = (u64*)alloca(envc * sizeof(u64));

    for (size_t i = 0; i < envc; i++) {
        const char* env = config.envp[i].c_str();
        rsp = stack_push_string(rsp, env);
        envp_addresses[i] = rsp;
    }

    // Align up, to 16 bytes
    if (rsp & 0xF) {
        rsp -= rsp & 0xF;
    }

    // Push 128-bits to stack that are gonna be used as random data
    rsp = stack_push(rsp, 0);
    rsp = stack_push(rsp, 0);
    u64 rand_address = rsp;

    int result = getrandom((void*)rand_address, 16, 0);
    if (result == -1 || result != 16) {
        ERROR("Failed to get random data");
        return;
    }

    auxv_t auxv_entries[18] = {
        {AT_PAGESZ, {4096}},
        {AT_EXECFN, {(u64)program_name}},
        {AT_CLKTCK, {100}},
        {AT_ENTRY, {(u64)elf->GetEntrypoint()}},
        {AT_PLATFORM, {(u64)platform_name}},
        {AT_BASE, {(u64)elf->GetProgramBase()}},
        {AT_FLAGS, {0}},
        {AT_UID, {1000}},
        {AT_EUID, {1000}},
        {AT_GID, {1000}},
        {AT_EGID, {1000}},
        {AT_SECURE, {0}},
        {AT_PHDR, {(u64)elf->GetPhdr()}},
        {AT_PHENT, {elf->GetPhent()}},
        {AT_PHNUM, {elf->GetPhnum()}},
        {AT_RANDOM, {rand_address}},
        {AT_HWCAP, {0xBFEBFBFF}},
        {AT_NULL, {0}} // null terminator
    };

    VERBOSE("AT_PHDR: %p", auxv_entries[12].a_un.a_ptr);
    VERBOSE("AT_PHENT: %lu", auxv_entries[13].a_un.a_val);
    VERBOSE("AT_PHNUM: %lu", auxv_entries[14].a_un.a_val);
    VERBOSE("AT_RANDOM: %p", auxv_entries[15].a_un.a_ptr);
    u16 auxv_count = std::size(auxv_entries);

    // This is the varying amount of space needed for the stack
    // past our own information block
    // It's important to calculate this because the RSP final
    // value needs to be aligned to 16 bytes
    u16 size_needed = 16 * auxv_count + // aux vector entries
                      8 +               // null terminator
                      envc * 8 +        // envp
                      8 +               // null terminator
                      argc * 8 +        // argv
                      8;                // argc

    u64 final_rsp = rsp - size_needed;
    if (final_rsp & 0xF) {
        rsp -= 8;
    }

    for (int i = auxv_count - 1; i >= 0; i--) {
        rsp = stack_push(rsp, (u64)auxv_entries[i].a_un.a_ptr);
        rsp = stack_push(rsp, auxv_entries[i].a_type);
    }

    auxv_base = (void*)rsp;
    auxv_size = auxv_count * 16;

    // End of environment variables
    rsp = stack_push(rsp, 0);

    for (int i = envc - 1; i >= 0; i--) {
        rsp = stack_push(rsp, envp_addresses[i]);
    }

    // End of arguments
    rsp = stack_push(rsp, 0);
    for (ssize_t i = argc - 1; i >= 0; i--) {
        rsp = stack_push(rsp, argv_addresses[i]);
    }

    // Argument count
    rsp = stack_push(rsp, argc);

    if (rsp & 0xF) {
        ERROR("Stack not aligned to 16 bytes\n");
        return;
    }

    state->SetGpr(X86_REF_RSP, rsp);
}

void* Emulator::CompileNext(ThreadState* thread_state) {
    // Check if there's any pending asynchronous signals. If there are, raise them.
    if (thread_state->pending_signals != 0) {
        sigset_t full, old;
        sigfillset(&full);
        sigprocmask(SIG_BLOCK, &full, &old); // block signals to make changing pending_signals safe

        int sig = 0;
        for (int i = 0; i < 64; i++) {
            if (thread_state->pending_signals & (1 << i)) {
                sig = i + 1;
                thread_state->pending_signals &= ~(1 << i);
                break;
            }
        }

        sigprocmask(SIG_SETMASK, &old, nullptr);

        ASSERT(sig != 0); // found the signal

        SignalHandlerTable& handlers = *thread_state->signal_handlers;
        RegisteredSignal& handler = handlers[sig - 1];

        u64 rip = thread_state->GetRip();

        sigset_t mask_during_signal;
        mask_during_signal = handler.mask;

        if (!(handler.flags & SA_NODEFER)) {
            sigaddset(&mask_during_signal, sig);
        }

        u64* gprs = thread_state->gprs;
        XmmReg* xmms = thread_state->xmm;

        bool use_altstack = handler.flags & SA_ONSTACK;

        Signals::setupFrame(nullptr, rip, thread_state, mask_during_signal, gprs, xmms, use_altstack, false);

        thread_state->SetGpr(X86_REF_RDI, sig);

        // Now we just need to set RIP to the handler function
        thread_state->SetRip((u64)handler.func);

        if (sig == SIGCHLD) {
            WARN("SIGCHLD, are we copying siginfo correctly?");
        }

        // Block the signals specified in the sa_mask until the signal handler returns
        sigset_t new_mask;
        sigandset(&new_mask, &mask_during_signal, Signals::hostSignalMask());
        pthread_sigmask(SIG_BLOCK, &new_mask, nullptr);

        if (handler.flags & SA_RESETHAND) {
            handler.func = nullptr;
        }
        WARN("Handling deferred signal %d", sig);
    }

    return thread_state->recompiler->getCompiledBlock(thread_state->GetRip());
}

void Emulator::StartThread(ThreadState* state) {
    state->tid = gettid();
    state->recompiler->enterDispatcher(state);
    VERBOSE("Thread exited with reason %d\n", state->exit_reason);
}

void Emulator::CleanExit(ThreadState* state) {
    state->recompiler->exitDispatcher(state);
}

void Emulator::UnlinkBlock(ThreadState* state, u64 rip) {
    state->recompiler->unlinkBlock(state, rip);
}