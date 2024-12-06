#include <csignal>
#include <elf.h>
#include <fmt/base.h>
#include <fmt/format.h>
#include <stdlib.h>
#include <sys/random.h>
#include "felix86/backend/disassembler.hpp"
#include "felix86/common/disk_cache.hpp"
#include "felix86/emulator.hpp"
#include "felix86/frontend/frontend.hpp"
#include "felix86/hle/cpuid.hpp"
#include "felix86/hle/rdtsc.hpp"
#include "felix86/hle/syscall.hpp"
#include "felix86/ir/passes/passes.hpp"

extern char** environ;

static char x86_64_string[] = "x86_64";

u64* addy = nullptr;

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
    if (thread_states.size() != 1) {
        ERROR("Expected exactly one thread state during Emulator::Run, the main thread");
    }

    VERBOSE("Executable: %016lx - %016lx", g_executable_start, g_executable_end);
    if (g_interpreter_start) {
        VERBOSE("Interpreter: %016lx - %016lx", g_interpreter_start, g_interpreter_end);
    }

    if (!g_testing) {
        VERBOSE("Entrypoint: %016lx", (u64)fs.GetEntrypoint());
    }

    VERBOSE("Entering main thread :)");

    ThreadState* state = &thread_states.back();
    backend.EnterDispatcher(state);

    VERBOSE("Bye-bye main thread :(");
    VERBOSE("Main thread exited: %d", (int)state->exit_reason);
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
    std::vector<u64> argv_addresses(argc);

    rsp = stack_push_string(rsp, path);
    const char* program_name = (const char*)rsp;

    rsp = stack_push_string(rsp, x86_64_string);
    const char* platform_name = (const char*)rsp;

    for (ssize_t i = 0; i < argc; i++) {
        rsp = stack_push_string(rsp, config.argv[i].c_str());
        argv_addresses[i] = rsp;
    }

    size_t envc = config.envp.size();
    std::vector<u64> envp_addresses(envc);

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

    addy = (u64*)rand_address;

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

void* Emulator::compileFunction(u64 rip) {
    {
        std::lock_guard<std::mutex> lock(compilation_mutex);
        void* already_compiled = backend.GetCodeAt(rip).first;
        if (already_compiled) {
            return already_compiled;
        }
    }

    VERBOSE("Now compiling: %016lx", rip);
    IRFunction function{rip};
    frontend_compile_function(function);

    VERBOSE("Hash: %016lx-%016lx", function.GetHash().values[1], function.GetHash().values[0]);

    // Check disk cache if enabled
    if (g_cache_functions) {
        ASSERT(!g_testing);
        std::string hex_hash = fmt::format("{:016x}{:016x}", function.GetHash().values[1], function.GetHash().values[0]);
        if (DiskCache::Has(hex_hash)) {
            std::vector<u8> function = DiskCache::Read(hex_hash);
            compilation_mutex.lock();
            void* start = backend.AddCodeAt(rip, function.data(), function.size());
            compilation_mutex.unlock();
            return start;
        }
    }

    PassManager::SSAPass(&function);
    PassManager::DeadCodeEliminationPass(&function);

    if (!g_dont_optimize) {
        bool changed = false;
        do {
            changed = false;
            changed |= PassManager::PeepholePass(&function);
            changed |= PassManager::LocalCSEPass(&function);
            changed |= PassManager::CopyPropagationPass(&function);
            changed |= PassManager::DeadCodeEliminationPass(&function);
        } while (changed);
        PassManager::ImmediateTransformationPass(&function);
    }

    PassManager::CriticalEdgeSplittingPass(&function);

    if (g_print_blocks) {
        fmt::print("{}", function.Print({}));
    }

    if (!function.Validate()) {
        ERROR("Function did not validate");
    }

    BackendFunction backend_function = BackendFunction::FromIRFunction(&function);

    AllocationMap allocations = ir_graph_coloring_pass(backend_function);

    // Add vector state instructions where necessary
    PassManager::VectorStatePass(&backend_function);

    // PassManager::BlockShenanigansPass(&backend_function);

    if (g_print_blocks) {
        fmt::print("Backend function IR:\n{}\n", backend_function.Print());
    }

    compilation_mutex.lock();
    auto [func, size] = backend.EmitFunction(backend_function, allocations);
    compilation_mutex.unlock();

    if (g_print_disassembly) {
        PLAIN("Disassembly of function at 0x%lx:\n", rip);
        PLAIN("%s", Disassembler::Disassemble(func, size).c_str());
    }

    if (g_cache_functions) {
        ASSERT(!g_testing);
        std::string hex_hash = fmt::format("{:016x}{:016x}", function.GetHash().values[1], function.GetHash().values[0]);
        DiskCache::Write(hex_hash, func, size);
    }

    return func;
}

void* Emulator::CompileNext(Emulator* emulator, ThreadState* thread_state) {

    // Mutex needs to be unlocked before the thread is dispatched
    void* volatile function = emulator->compileFunction(thread_state->GetRip());

    u64 address = thread_state->GetRip();
    if (address >= g_interpreter_start && address < g_interpreter_end) {
        address = address - g_interpreter_start;
    } else if (address >= g_executable_start && address < g_executable_end) {
        address = address - g_executable_start;
    }

#if 0
    // Hack to break on a specific address
    u64 addy = thread_state->GetRip();
    if (addy == 0x1'0000'ed69) {
        raise(SIGTRAP);
    }
#endif

    VERBOSE("Jumping to function %016lx (%016lx), located at %p", thread_state->GetRip(), address, function);

    return function;
}

ThreadState* Emulator::createThreadState() {
    thread_states.push_back(ThreadState{});

    ThreadState* thread_state = &thread_states.back();
    thread_state->syscall_handler = (u64)felix86_syscall;
    thread_state->cpuid_handler = (u64)felix86_cpuid;
    thread_state->rdtsc_handler = (u64)felix86_rdtsc;
    thread_state->compile_next = (u64)backend.GetCompileNext();
    thread_state->crash_handler = (u64)backend.GetCrashHandler();
    thread_state->div128_handler = (u64)felix86_div128;
    thread_state->divu128_handler = (u64)felix86_divu128;
    return thread_state;
}