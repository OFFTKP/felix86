#include <elf.h>
#include <fmt/base.h>
#include <fmt/format.h>
#include <stdlib.h>
#include <sys/random.h>
#include "felix86/backend/disassembler.hpp"
#include "felix86/emulator.hpp"
#include "felix86/frontend/frontend.hpp"
#include "felix86/ir/passes/passes.hpp"

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
    if (thread_states.size() != 1) {
        ERROR("Expected exactly one thread state during Emulator::Run, the main thread");
    }

    VERBOSE("Entering main thread :)");

    ThreadState* state = &thread_states.back();
    backend.EnterDispatcher(state);

    VERBOSE("Bye-bye main thread :(");
    VERBOSE("Main thread exited: %d", (int)state->exit_dispatcher_flag);
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

    // Push 128-bits to stack that are gonna be used as random data
    u64 rand_address = stack_push(rsp, 0);
    stack_push(rsp, 0);

    int result = getrandom((void*)rand_address, 16, 0);
    if (result == -1 || result != 16) {
        ERROR("Failed to get random data");
        return;
    }

    rsp &= ~0xF; // Align to 16 bytes

    auxv_t auxv_entries[17] = {
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
        {AT_NULL, {0}} // null terminator
    };

    VERBOSE("AT_PHDR: %p", auxv_entries[12].a_un.a_ptr);
    u16 auxv_count = sizeof(auxv_entries) / sizeof(auxv_t);

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
        // The final rsp wouldn't be aligned to 16 bytes
        // so we need to add padding
        rsp = stack_push(rsp, 0);
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
    IRFunction function{rip};
    frontend_compile_function(&function);

    PassManager::SSAPass(&function);
    PassManager::DeadCodeEliminationPass(&function);

    bool changed = false;

    do {
        changed = false;
        changed |= PassManager::PeepholePass(&function);
        changed |= PassManager::LocalCSEPass(&function);
        changed |= PassManager::CopyPropagationPass(&function);
        changed |= PassManager::DeadCodeEliminationPass(&function);
    } while (changed);

    PassManager::CriticalEdgeSplittingPass(&function);

    if (config.print_blocks) {
        fmt::print("{}", function.Print({}));
    }

    if (!function.Validate()) {
        ERROR("Function did not validate");
    }

    BackendFunction backend_function = BackendFunction::FromIRFunction(&function);

    AllocationMap allocations = ir_graph_coloring_pass(backend_function);

    auto [func, size] = backend.EmitFunction(backend_function, allocations);
    std::string disassembly = Disassembler::Disassemble(func, size);
    fmt::print("{}\n", disassembly);

    return func;
}

void* Emulator::CompileNext(Emulator* emulator, ThreadState* thread_state) {
    void* function;

    // Mutex needs to be unlocked before the thread is dispatched
    {
        std::lock_guard<std::mutex> lock(emulator->compilation_mutex);
        function = emulator->compileFunction(thread_state->rip);
    }

    return function;
}