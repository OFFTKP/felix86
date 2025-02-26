#include <csignal>
#include <vector>
#include <elf.h>
#include <fcntl.h>
#include <fmt/base.h>
#include <fmt/format.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/random.h>
#include "felix86/emulator.hpp"
#include "felix86/hle/thread.hpp"
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

std::pair<void*, size_t> Emulator::setupMainStack(ThreadState* state) {
    ssize_t argc = g_config.argv.size();
    if (argc > 1) {
        VERBOSE("Passing %zu arguments to guest executable", argc - 1);
        for (ssize_t i = 1; i < argc; i++) {
            VERBOSE("Guest argument %zu: %s", i, g_config.argv[i].c_str());
        }
    }

    const char* path = g_config.argv[0].c_str();

    std::shared_ptr<Elf> elf = g_fs->GetExecutable();

    // Initial process stack according to System V AMD64 ABI
    auto pair = Threads::AllocateStack(g_mode32);
    u64 rsp = (u64)pair.first;

    // To hold the addresses of the arguments for later pushing
    u64* argv_addresses = (u64*)alloca(argc * sizeof(u64));

    rsp = stack_push_string(rsp, path);
    const char* program_name = (const char*)rsp;

    rsp = stack_push_string(rsp, x86_64_string);
    const char* platform_name = (const char*)rsp;

    for (ssize_t i = 0; i < argc; i++) {
        rsp = stack_push_string(rsp, g_config.argv[i].c_str());
        argv_addresses[i] = rsp;
    }

    size_t envc = g_config.envp.size();
    u64* envp_addresses = (u64*)alloca(envc * sizeof(u64));

    for (size_t i = 0; i < envc; i++) {
        const char* env = g_config.envp[i].c_str();
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
        return pair;
    }

    auxv_t auxv_entries[18] = {
        {AT_PAGESZ, {4096}},
        {AT_EXECFN, {(u64)program_name}},
        {AT_CLKTCK, {100}},
        {AT_ENTRY, {elf->GetEntrypoint().raw()}},
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
    VERBOSE("AT_ENTRY: %p", auxv_entries[3].a_un.a_ptr);
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

    g_guest_auxv = HostAddress{rsp};
    g_guest_auxv_size = auxv_count * 16;

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
        ERROR("Stack not aligned to 16 bytes");
        return pair;
    }

    GuestAddress rsp_guest = HostAddress{rsp}.toGuest();
    state->SetGpr(X86_REF_RSP, rsp_guest.raw());

    return pair;
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

        GuestAddress rip = thread_state->GetRip();

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
        thread_state->SetRip(handler.func);

        if (sig == SIGCHLD) {
            WARN("SIGCHLD, are we copying siginfo correctly?");
        }

        // Block the signals specified in the sa_mask until the signal handler returns
        sigset_t new_mask;
        sigandset(&new_mask, &mask_during_signal, Signals::hostSignalMask());
        pthread_sigmask(SIG_BLOCK, &new_mask, nullptr);

        if (handler.flags & SA_RESETHAND) {
            handler.func = GuestAddress{};
        }
        WARN("Handling deferred signal %d", sig);
    }

    g_dispatcher_exit_count++;

    HostAddress next_block = thread_state->recompiler->getCompiledBlock(thread_state->GetRip().toHost());
    return (void*)next_block.raw();
}

void Emulator::initialize32BitAddressSpace() {
    constexpr u64 GB = 1024 * 1024 * 1024;
    constexpr u64 size = 2 * GB + 4 * GB + 2 * GB;

    // Find a 32-bit address space that is not used by the host
    // We also allocate a guard on either side of 2GB to catch
    // any out-of-bounds accesses
    u8* cur = (u8*)0x1'0000'0000;
    int attempts = 0; // don't try forever
    while (true) {
        void* addr = mmap(cur, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
        if (addr != MAP_FAILED) {
            ASSERT(addr == cur);
            break;
        }

        if (++attempts >= 100) {
            ERROR("Failed to find a 32-bit address space after %d", attempts);
            return;
        }

        cur += size;
    }

    g_address_space_base = (u64)(cur + 2 * GB);
    VERBOSE("32-bit address space at %p", (void*)g_address_space_base);
}

void Emulator::ExitDispatcher(ThreadState* state) {
    state->recompiler->exitDispatcher(state);
}

void Emulator::uninitialize32BitAddressSpace() {
    ASSERT(g_address_space_base != 0);
    constexpr u64 GB = 1024 * 1024 * 1024;
    constexpr u64 size = 2 * GB + 4 * GB + 2 * GB;

    u8* addr = (u8*)g_address_space_base - 2 * GB;
    munmap(addr, size);
}

std::pair<ExitReason, int> Emulator::Start(const Config& config) {
    g_config = config;
    ExitReason exit_reason;
    int exit_code;
    g_config = config;

    do {
        g_process_globals.initialize();
        g_fs = std::make_unique<Filesystem>();

        Elf::PeekResult peek = Elf::Peek(g_config.executable_path);
        if (peek == Elf::PeekResult::NotElf) {
            ERROR("File %s is not an ELF file", g_config.executable_path.c_str());
        }

        if (peek == Elf::PeekResult::Elf32) {
            g_mode32 = true;
            initialize32BitAddressSpace();
        } else {
            g_mode32 = false;
        }

        g_fs->LoadExecutable(g_config.executable_path);
        ThreadState* main_state = ThreadState::Create(nullptr);
        main_state->signal_handlers = std::make_shared<SignalHandlerTable>();
        main_state->SetRip(g_fs->GetEntrypoint());

        auto [stack, size] = setupMainStack(main_state);

        // The Emulator::Run will only return when exit_dispatcher is jumped to
        VERBOSE("Executable: %016lx - %016lx", g_executable_start.raw(), g_executable_end.raw());
        if (!g_interpreter_start.isNull()) {
            VERBOSE("Interpreter: %016lx - %016lx", g_interpreter_start.raw(), g_interpreter_end.raw());
        }

        if (!g_testing) {
            VERBOSE("Entrypoint: %016lx", g_fs->GetEntrypoint().toHost().raw());
        }

        VERBOSE("Entering main thread :)");

        Threads::StartThread(main_state);

        VERBOSE("Bye-bye main thread :(");

        exit_reason = main_state->exit_reason;
        exit_code = main_state->exit_code;

        if (g_mode32) {
            uninitialize32BitAddressSpace();
        }

        munmap(stack, size);
        munmap((void*)g_initial_brk, g_current_brk_size);
        g_fs.reset();
        g_breakpoints.clear();
        ThreadState::Destroy(main_state);
        pthread_setspecific(g_thread_state_key, nullptr);

        if (exit_reason == EXIT_REASON_EXECVE) {
            // Just start the emulator again
            // The execve handler has changed g_config to the new executable
            ERROR("TODO: Implement execve");
            continue;
        }
    } while (false);

    return {exit_reason, exit_code};
}

void Emulator::StartTest(const TestConfig& config, GuestAddress stack) {
    g_mode32 = config.mode32;

    ThreadState* main_state = ThreadState::Create(nullptr);
    main_state->SetGpr(X86_REF_RSP, stack.raw());
    main_state->SetRip(config.entrypoint.toGuest());

    if (g_mode32) {
        initialize32BitAddressSpace();
    }

    Threads::StartThread(main_state);
}