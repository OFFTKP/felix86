#include "felix86/loader/loader.h"
#include "felix86/common/version.h"
#include "felix86/loader/elf.h"
#include "felix86/common/log.h"
#include "felix86/felix86.h"
#include <string.h>
#include <sys/auxv.h>

static char x86_64_string[] = "x86_64";

typedef struct
{
    int a_type;
    union {
        long a_val;
        void *a_ptr;
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

    elf_t* elf = elf_load(path, NULL);
    if (!elf) {
        ERROR("Failed to load ELF\n");
        return;
    }

    if (elf->interpreter) {
        ERROR("Interpreter not implemented\n");
    } else {
        VERBOSE("Entrypoint: %p", (void*)elf->entry);
    }

    u64 entry = (u64)elf->program + elf->entry;

    // Initial process stack according to System V AMD64 ABI
    u64 rsp = (u64)elf->stackPointer;

    // To hold the addresses of the arguments for later pushing
    u64* argv_addresses = malloc(config->argc * sizeof(u64));

    rsp = stack_push_string(rsp, path);
    const char* program_name = (const char*)rsp;

    rsp = stack_push_string(rsp, x86_64_string);
    const char* platform_name = (const char*)rsp;

    for (int i = 0; i < config->argc; i++) {
        rsp = stack_push_string(rsp, config->argv[i]);
        argv_addresses[i] = rsp;
    }

    rsp &= ~0xF; // Align to 16 bytes

    auxv_t auxv_entries[16] =
    {
        [0] = { AT_PAGESZ, 4096 },
        [1] = { AT_EXECFN, (u64)program_name },
        [2] = { AT_CLKTCK, 100 },
        [3] = { AT_ENTRY, elf->entry },
        [4] = { AT_PLATFORM, (u64)platform_name },
        [5] = { AT_BASE, (u64)elf->program },
        [6] = { AT_FLAGS, 0 },
        [7] = { AT_UID, 1000 },
        [8] = { AT_EUID, 1000 },
        [9] = { AT_GID, 1000 },
        [10] = { AT_EGID, 1000 },
        [11] = { AT_SECURE, 0 },
        [12] = { AT_PHDR, (u64)elf->phdr },
        [13] = { AT_PHENT, elf->phent },
        [14] = { AT_PHNUM, elf->phnum },
        [15] = { AT_NULL, 0 } // null terminator
    };

    u16 auxv_count = sizeof(auxv_entries) / sizeof(auxv_t);

    // This is the varying amount of space needed for the stack
    // past our own information block
    // It's important to calculate this because the RSP final
    // value needs to be aligned to 16 bytes
    u16 size_needed =
        16 * auxv_count + // aux vector entries
        8 + // null terminator
        config->envc * 8 + // envp
        8 + // null terminator
        config->argc * 8 + // argv
        8; // argc

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

    // TODO: push environment variables

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

    free(argv_addresses);

    felix86_recompiler_config_t fconfig = { .testing = true };
    felix86_recompiler_t* recompiler = felix86_recompiler_create(&fconfig);
    felix86_set_guest(recompiler, X86_REF_RIP, entry);
    felix86_set_guest(recompiler, X86_REF_RSP, rsp);
    felix86_recompiler_run(recompiler, 0);
    felix86_recompiler_destroy(recompiler);

    elf_destroy(elf);
}