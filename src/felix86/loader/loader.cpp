#include <vector>
#include <errno.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/random.h>
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/version.hpp"
#include "felix86/felix86.hpp"
#include "felix86/loader/elf.hpp"
#include "felix86/loader/loader.hpp"

extern char** environ;

static char x86_64_string[] = "x86_64";

typedef struct {
    int a_type;

    union {
        u64 a_val;
        void* a_ptr;
        void (*a_fnc)();
    } a_un;
} auxv_t;

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

void loader_run_elf(loader_config_t* config) {
    LOG("felix86 version %s", FELIX86_VERSION);

    if (config->argc > 1) {
        VERBOSE("Passing %d arguments to guest executable", config->argc - 1);
        for (int i = 1; i < config->argc; i++) {
            VERBOSE("Guest argument %d: %s", i, config->argv[i]);
        }
    }

    const char* path = config->argv[0];

    std::unique_ptr<Elf> elf = elf_load(path, false);
    if (!elf) {
        ERROR("Failed to load ELF");
        return;
    }

    u64 entry;
    std::unique_ptr<Elf> interpreter;
    if (!elf->interpreter.empty()) {
        interpreter = elf_load(elf->interpreter, true);
        entry = (u64)interpreter->program + interpreter->entry;
        g_interpreter_address = (u64)interpreter->program;
    } else {
        entry = (u64)elf->program + elf->entry;
    }

    // Initial process stack according to System V AMD64 ABI
    u64 rsp = (u64)elf->stack_pointer;

    // To hold the addresses of the arguments for later pushing
    std::vector<u64> argv_addresses(config->argc);

    rsp = stack_push_string(rsp, path);
    const char* program_name = (const char*)rsp;

    rsp = stack_push_string(rsp, x86_64_string);
    const char* platform_name = (const char*)rsp;

    for (int i = 0; i < config->argc; i++) {
        rsp = stack_push_string(rsp, config->argv[i]);
        argv_addresses[i] = rsp;
    }

    int envc = config->envc;

    if (config->use_host_envs) {
        char** envp = environ;
        while (*envp) {
            envc++;
            envp++;
        }
    }

    std::vector<u64> envp_addresses(envc);

    for (int i = 0; i < envc; i++) {
        char* env;
        if (i < config->envc) {
            env = config->envp[i];
        } else {
            env = environ[i - config->envc];
        }
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
        {AT_ENTRY, {elf->entry}},
        {AT_PLATFORM, {(u64)platform_name}},
        {AT_BASE, {(u64)elf->program}},
        {AT_FLAGS, {0}},
        {AT_UID, {1000}},
        {AT_EUID, {1000}},
        {AT_GID, {1000}},
        {AT_EGID, {1000}},
        {AT_SECURE, {0}},
        {AT_PHDR, {(u64)elf->phdr}},
        {AT_PHENT, {elf->phent}},
        {AT_PHNUM, {elf->phnum}},
        {AT_RANDOM, {rand_address}},
        {AT_NULL, {0}} // null terminator
    };

    VERBOSE("AT_PHDR: %p", auxv_entries[12].a_un.a_ptr);
    u16 auxv_count = sizeof(auxv_entries) / sizeof(auxv_t);

    // This is the varying amount of space needed for the stack
    // past our own information block
    // It's important to calculate this because the RSP final
    // value needs to be aligned to 16 bytes
    u16 size_needed = 16 * auxv_count +  // aux vector entries
                      8 +                // null terminator
                      envc * 8 +         // envp
                      8 +                // null terminator
                      config->argc * 8 + // argv
                      8;                 // argc

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
    for (int i = config->argc - 1; i >= 0; i--) {
        rsp = stack_push(rsp, argv_addresses[i]);
    }

    // Argument count
    rsp = stack_push(rsp, config->argc);

    if (rsp & 0xF) {
        ERROR("Stack not aligned to 16 bytes\n");
        return;
    }

    felix86_recompiler_config_t fconfig = {.testing = false, .optimize = !config->dont_optimize, .print_blocks = config->print_blocks, .use_interpreter = config->use_interpreter, .base_address = (u64)elf->program, .brk_base_address = (u64)elf->brk_base};
    felix86_recompiler_t* recompiler = felix86_recompiler_create(&fconfig);
    felix86_set_guest(recompiler, X86_REF_RIP, entry);
    felix86_set_guest(recompiler, X86_REF_RSP, rsp);
    felix86_recompiler_run(recompiler);
    felix86_recompiler_destroy(recompiler);
}