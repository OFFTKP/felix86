#pragma once

#include "felix86/common/utility.hpp"

typedef struct allocator_s allocator_t;

allocator_t* allocator_create(u64 size);
void* allocator_alloc(allocator_t* allocator, u64 size);
void allocator_reset(allocator_t* allocator);
u64 allocator_get_size(allocator_t* allocator);
void allocator_protect(allocator_t* allocator, void* ptr, u64 size, bool read, bool write, bool execute);
void allocator_destroy(allocator_t* allocator);
