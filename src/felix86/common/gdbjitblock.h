// Shared between felix86-emu/jitreader and felix86 so we don't include any headers here
#pragma once

#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long u64;

struct gdb_line_mapping {
    int line;
    u64 pc;
};

struct jit_code_entry {
    struct jit_code_entry* next_entry;
    struct jit_code_entry* prev_entry;
    const char* symfile_addr;
    u64 symfile_size;
};

// Represents a block of recompiled instructions and their names for gdb
typedef struct {
    char filename[64];
    FILE* file;
    u64 host_start;
    u64 host_end;
    u64 guest_address;
    u64 line_count;
    // Also contains the node for the interface
    jit_code_entry entry;

    // Unfortunately this can't be a pointer to some other storage -- we need to allocate the line mappings
    // in this linear storage for when we pass it to gdb, presumably because it copies it to its own address space?
    struct gdb_line_mapping lines[0];
} felix86_jit_block_t;

#ifdef __cplusplus
}
#endif