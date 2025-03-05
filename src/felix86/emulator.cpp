#include <csignal>
#include <vector>
#include <elf.h>
#include <fcntl.h>
#include <fmt/base.h>
#include <fmt/format.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/random.h>
#include "felix86/common/script.hpp"
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
    if (!thread_state->pending_signals.empty()) {
        sigset_t full, old;
        sigfillset(&full);
        sigprocmask(SIG_BLOCK, &full, &old); // block signals to make changing pending_signals safe

        PendingSignal& signal = thread_state->pending_signals.back();

        int sig = signal.sig;
        siginfo_t info = signal.info;

        thread_state->pending_signals.pop_back();

        sigprocmask(SIG_SETMASK, &old, nullptr);

        ASSERT(sig != 0);

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

        Signals::setupFrame(nullptr, rip, 0, thread_state, mask_during_signal, gprs, xmms, use_altstack, false, &info);

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

    HostAddress next_block = thread_state->recompiler->getCompiledBlock(thread_state, thread_state->GetRip().toHost());

    if (g_block_trace) {
        thread_state->recompiler->trace(thread_state->GetRip().toHost().raw());
    }

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

    g_process_globals.initialize();
    g_fs = std::make_unique<Filesystem>();

    Elf::PeekResult peek = Elf::Peek(g_config.executable_path);
    if (peek == Elf::PeekResult::NotElf) {
        Script::PeekResult peek = Script::Peek(g_config.executable_path);
        if (peek == Script::PeekResult::Script) {
            Script script(g_config.executable_path);
            const std::filesystem::path& interpreter = script.GetInterpreter();
            const std::string& args = script.GetArgs();

            // Scripts start with a line that goes #! (usually) and that means
            // use the interpreter after #!. This can be bash, zsh, python, whatever.
            // So, set executable path to be the interpreter itself and push it to the front of argv.
            // In that #! line args can follow and if they exist we need to push them to the front in opposite order
            auto args_array = split_string(args, ' ');
            for (auto it = args_array.rbegin(); it < args_array.rend(); it++) {
                if (it->empty())
                    continue;

                g_config.argv.push_front(*it);
            }

            g_config.argv.push_front(interpreter.string());

            std::string final;
            for (auto& arg : g_config.argv) {
                final += arg + " ";
            }

            LOG("I built the script arguments: %s", final.c_str());

            g_config.executable_path = interpreter;
        } else {
            ERROR("Unknown file format: %s", g_config.executable_path.c_str());
        }
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

void Emulator::LinkIndirect(u64 host_address, u64 guest_address, u8* link_address, ThreadState* state) {
    Assembler& as = state->recompiler->getAssembler();

    u8* before = as.GetCursorPointer();
    as.SetCursorPointer(link_address);

    Label unlink_indirect;
    Literal guest(guest_address);
    Literal host(host_address);
    Literal unlink_address((u64)Emulator::UnlinkIndirect);
    as.LD(t1, offsetof(ThreadState, rip), Recompiler::threadStatePointer());
    as.LD(t0, &guest);
    as.BNE(t0, t1, &unlink_indirect);
    as.LD(t2, &host);
    if (g_rsb) {
        as.JALR(t2); // push to rsb
    } else {
        as.JR(t2);
    }
    as.Bind(&unlink_indirect);
    as.NOP(); // important it's here, due to -11 * 4 in unlink indirect
    as.NOP();

    const u64 offset = (u64)state->recompiler->getUnlinkIndirectThunk() - (u64)as.GetCursorPointer();
    ASSERT(IsValid2GBImm(offset));
    const auto hi20 = static_cast<int32_t>(((static_cast<uint32_t>(offset) + 0x800) >> 12) & 0xFFFFF);
    const auto lo12 = static_cast<int32_t>(offset << 20) >> 20;

    as.AUIPC(t2, hi20);
    as.JALR(ra, lo12, t2);

    // The instruction following is a jump that goes back exactly 10 instructions, the same amount that we have here
    // Note: LD with Literal is 2 instructions -> AUIPC + LD. So we have 11 instructions here too.
    u8* here = as.GetCursorPointer();
    ASSERT(here - link_address == 11 * 4);

    // We don't wanna overwrite this jump. Not only do we need it after we return from this function,
    // but we are also gonna need it in case the comparison fails, as UnlinkIndirect will once again rewrite this
    // chunk of code
    as.SetCursorPointer(here + 4);

    // Now overwrite these 3 literals from Recompiler::linkIndirect with our own
    as.Place(&guest);
    as.Place(&host);
    as.Place(&unlink_address);

    as.SetCursorPointer(before);

    // Spooky self-modifying code over
    flush_icache();
}

void Emulator::UnlinkIndirect(ThreadState* state, u8* link_address) {
    // This function is called when an indirect jump prediction fails once. One time is enough for it to not be worth
    // the check anymore... for example OOP structs with vtables can change function pointers quite a bit so we would rather
    // always jump to the dispatcher for those... So replace our link with a backToDispatcher

    // This function takes no arguments, yet we have enough info to deduce where we are from the registers
    Assembler& as = state->recompiler->getAssembler();
    u8* before = as.GetCursorPointer();

    as.SetCursorPointer(link_address);

    // Replace the first two instructions with a back to dispatcher jump and forget whatever follows
    state->recompiler->backToDispatcher();

    as.SetCursorPointer(before);

    flush_icache();
}