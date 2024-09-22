#include <stdlib.h>
#include <sys/mman.h>
#include "felix86/common/allocator.hpp"
#include "felix86/common/log.hpp"

struct allocator_s {
    u8* memory;
    u64 size;
    u64 offset;
};

allocator_t* allocator_create(u64 size) {
    allocator_t* allocator = (allocator_t*)malloc(sizeof(allocator_t));
    allocator->memory =
        (u8*)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (allocator->memory == MAP_FAILED) {
        ERROR("Failed to allocate memory");
        free(allocator);
        return NULL;
    }

    allocator->size = size;
    allocator->offset = 0;
    return allocator;
}

void* allocator_alloc(allocator_t* allocator, u64 size) {
    if (allocator->offset + size > allocator->size) {
        ERROR("Out of memory");
        return NULL;
    }

    void* ptr = allocator->memory + allocator->offset;
    allocator->offset += size;
    return ptr;
}

void allocator_reset(allocator_t* allocator) {
    allocator->offset = 0;
}

u64 allocator_get_size(allocator_t* allocator) {
    return allocator->size - allocator->offset;
}

void allocator_protect(allocator_t* allocator, void* ptr, u64 size, bool read, bool write,
                       bool execute) {
    u8 prot = 0;

    if (read) {
        prot |= PROT_READ;
    }

    if (write) {
        prot |= PROT_WRITE;
    }

    if (execute) {
        prot |= PROT_EXEC;
    }

    if ((u8*)ptr + size > allocator->memory + allocator->size || (u8*)ptr < allocator->memory) {
        ERROR("Invalid memory region");
        return;
    }

    mprotect(ptr, size, prot);
}

void allocator_destroy(allocator_t* allocator) {
    free(allocator->memory);
    free(allocator);
}