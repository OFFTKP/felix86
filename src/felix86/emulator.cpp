#include <span>
#include <vector>
#include <elf.h>
#include <fcntl.h>
#include <fmt/base.h>
#include <fmt/format.h>
#include <linux/prctl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/random.h>
#include "felix86/common/script.hpp"
#include "felix86/common/types.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/emulator.hpp"
#include "felix86/hle/brk.hpp"
#include "felix86/hle/signals.hpp"
#include "felix86/hle/thread.hpp"
#include "felix86/hle/thunks.hpp"
#include "felix86/hle/vdso.hpp"
#include "felix86/v2/recompiler.hpp"

extern char** environ;

static char x86_string[] = "i686";
static char x86_64_string[] = "x86_64";

u64 stack_push64(u64 stack, u64 value) {
    stack -= 8;
    *(u64*)stack = value;
    return stack;
}

u64 stack_push32(u64 stack, u64 value) {
    stack -= 4;
    *(u32*)stack = value;
    return stack;
}

u64 stack_push_string(u64 stack, const char* str) {
    u64 len = strlen(str) + 1;
    stack -= len;
    strcpy((char*)stack, str);
    return stack;
}

struct auxv64_t {
    int a_type;

    union {
        u64 a_val;
        void* a_ptr;
        void (*a_fnc)();
    } a_un;
};

struct auxv32_t {
    int a_type;
    u32 a_val;
};

std::pair<void*, size_t> Emulator::setupMainStack(ThreadState* state) {
    ssize_t argc = g_params.argv.size();
    if (argc > 1) {
        VERBOSE("Passing %zu arguments to guest executable", argc - 1);
        for (ssize_t i = 1; i < argc; i++) {
            VERBOSE("Guest argument %zu: %s", i, g_params.argv[i].c_str());
        }
    }

    const char* path = g_params.argv[0].c_str();

    std::shared_ptr<Elf> elf = g_fs->GetExecutable();

    // Initial process stack according to System V AMD64 ABI
    auto pair = Threads::AllocateStack(g_mode32);
    u64 rsp = (u64)pair.first;

    // To hold the addresses of the arguments for later pushing
    u64* argv_addresses = (u64*)alloca(argc * sizeof(u64));

    rsp = stack_push_string(rsp, path);
    const char* program_name = (const char*)rsp;

    rsp = stack_push_string(rsp, g_mode32 ? x86_string : x86_64_string);
    const char* platform_name = (const char*)rsp;

    // wine-preloader actually relies on us pushing these in this order
    for (ssize_t i = argc - 1; i >= 0; i--) {
        rsp = stack_push_string(rsp, g_params.argv[i].c_str());
        argv_addresses[i] = rsp;
    }

    size_t envc = g_params.envp.size();
    u64* envp_addresses = (u64*)alloca(envc * sizeof(u64));

    for (size_t i = 0; i < envc; i++) {
        const char* env = g_params.envp[i].c_str();
        rsp = stack_push_string(rsp, env);
        envp_addresses[i] = rsp;
    }

    // Align up, to 16 bytes
    if (rsp & 0xF) {
        rsp -= rsp & 0xF;
    }

    // Push 128-bits to stack that are gonna be used as random data
    rsp = stack_push64(rsp, 0);
    rsp = stack_push64(rsp, 0);
    u64 rand_address = rsp;

    int result = getrandom((void*)rand_address, 16, 0);
    if (result == -1 || result != 16) {
        ERROR("Failed to get random data");
        return pair;
    }

    std::vector<std::pair<u64, u64>> auxv_entries = {
        {AT_PAGESZ, {4096}},
        {AT_EXECFN, {(u64)program_name}},
        {AT_CLKTCK, {100}},
        {AT_ENTRY, {elf->GetEntrypoint()}},
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
    };

    if (!g_mode32) {
        // Add pointer to VDSO object
        // Since we include it as part of the felix86 binary we can just
        // point there directly in 64-bit mode
        std::span<u8> vdso_object = VDSO::getObject64();
        void* mem = mmap(nullptr, vdso_object.size(), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        ASSERT(mem != MAP_FAILED);
        memcpy(mem, vdso_object.data(), vdso_object.size());
        mprotect(mem, vdso_object.size(), PROT_READ | PROT_EXEC);
        auxv_entries.push_back({AT_SYSINFO_EHDR, {(u64)mem}});
    }

    auxv_entries.push_back({AT_NULL, {0}}); // null terminator

    u16 auxv_count = std::size(auxv_entries);

    // This is the varying amount of space needed for the stack
    // past our own information block
    // It's important to calculate this because the RSP final
    // value needs to be aligned to 16 bytes
    int pointer_size = g_mode32 ? 4 : 8;
    u16 size_needed = (2 * pointer_size) * auxv_count + // aux vector entries
                      pointer_size +                    // null terminator
                      envc * pointer_size +             // envp
                      pointer_size +                    // null terminator
                      argc * pointer_size +             // argv
                      pointer_size;                     // argc

    // 16-byte align the RSP
    if (size_needed & 0xF) {
        rsp -= 16 - (size_needed & 0xF);
    }

    u64 final_rsp = rsp - size_needed;

    u64 (*stack_push)(u64, u64) = g_mode32 ? stack_push32 : stack_push64;

    for (int i = auxv_count - 1; i >= 0; i--) {
        rsp = stack_push(rsp, auxv_entries[i].second);
        rsp = stack_push(rsp, auxv_entries[i].first);
    }

    g_guest_auxv = rsp;
    g_guest_auxv_size = auxv_count * pointer_size;

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

    ASSERT_MSG(rsp == final_rsp, "%lx == %lx", rsp, final_rsp);
    if (rsp & 0xF) {
        ERROR("Stack not aligned to 16 bytes");
        return pair;
    }

    u64 rsp_guest = rsp;
    state->SetGpr(X86_REF_RSP, rsp_guest);

    return pair;
}

void* Emulator::CompileNext(ThreadState* thread_state) {
    g_dispatcher_exit_count++;

    SignalGuard guard;
    u64 next_block = thread_state->recompiler->getCompiledBlock(thread_state, thread_state->GetRip());
    ASSERT_MSG(next_block != 0, "getCompiledBlock returned null?");
    return (void*)next_block;
}

void Emulator::ExitDispatcher(felix86_frame* frame) {
    frame->state->recompiler->exitDispatcher(frame);
}

std::pair<ExitReason, int> Emulator::Start() {
    ExitReason exit_reason;
    int exit_code;

    g_process_globals.initialize();

#ifdef PR_RISCV_SET_ICACHE_FLUSH_CTX
    prctl(PR_RISCV_SET_ICACHE_FLUSH_CTX, PR_RISCV_CTX_SW_FENCEI_ON, PR_RISCV_SCOPE_PER_PROCESS);
#endif

    std::string path = g_params.executable_path;
    Elf::PeekResult peek = Elf::Peek(g_params.executable_path);
    if (peek == Elf::PeekResult::NotElf) {
        Script::PeekResult peek = Script::Peek(g_params.executable_path);
        if (peek == Script::PeekResult::Script) {
            ERROR("You are trying to run a script file\nPlease start emulated bash and use it to run the script instead");
        } else {
            if (std::filesystem::exists(g_params.executable_path)) {
                FILE* f = fopen(g_params.executable_path.c_str(), "r");
                ASSERT(f);
                fseek(f, 0L, SEEK_END);
                size_t size = ftell(f);
                fclose(f);
                if (size == 0) {
                    // Sometimes, things decide to just execute an empty file.
                    // We need to return 0 and warn
                    WARN("Tried to execute an empty file: %s, returning 0...", g_params.executable_path.c_str());
                    _exit(0);
                }
            }
            ERROR("Unknown file format: %s", g_params.executable_path.c_str());
        }
    }

    if (peek == Elf::PeekResult::Elf32) {
        g_mode32 = true;
        // Allocate a 2GiB guard right after to catch bad addresses (that may need to loop around the address space?)
        constexpr u64 GB = 1024 * 1024 * 1024;
        void* guard = mmap((void*)(4 * GB), 2 * GB, PROT_NONE, MAP_FIXED_NOREPLACE | MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        if (guard == MAP_FAILED) {
            ERROR("I failed to allocate the 32-bit guard");
        }

        prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, 4 * GB, 2 * GB, "felix86-guard");
    } else {
        g_mode32 = false;
    }

    if (!g_mode32 && !g_config.thunks_path.empty()) {
        Thunks::initialize();
    }

    g_fs->LoadExecutable(g_params.executable_path);

    BRK::allocate();

    if (!g_execve_process) {
        // Go inside the rootfs
        ASSERT(g_rootfs_fd > 0);
        ASSERT(fchdir(g_rootfs_fd) == 0);
    }

    ThreadState* main_state = ThreadState::Create(nullptr);
    main_state->signal_table = SignalHandlerTable::Create(nullptr);
    main_state->SetRip(g_fs->GetEntrypoint());

    auto [stack, size] = setupMainStack(main_state);

    // The Emulator::Run will only return when exit_dispatcher is jumped to
    VERBOSE("Executable: %016lx - %016lx", g_executable_start, g_executable_end);
    if (g_interpreter_start != 0) {
        VERBOSE("Interpreter: %016lx - %016lx", g_interpreter_start, g_interpreter_end);
    }

    VERBOSE("Entering main thread :)");

    Threads::StartThread(main_state);

    VERBOSE("Bye-bye main thread :(");

    exit_reason = main_state->exit_reason;
    exit_code = main_state->exit_code;

    munmap(stack, size);
    munmap((void*)g_initial_brk, g_current_brk_size);
    g_fs.reset();
    g_breakpoints.clear();
    ThreadState::Destroy(main_state);
    pthread_setspecific(g_thread_state_key, nullptr);

    return {exit_reason, exit_code};
}

void Emulator::StartTest(const TestConfig& config, u64 stack) {
    g_mode32 = config.mode32;

    ThreadState* main_state = ThreadState::Create(nullptr);
    main_state->SetGpr(X86_REF_RSP, stack);
    main_state->SetRip(config.entrypoint);

    Threads::StartThread(main_state);
}
