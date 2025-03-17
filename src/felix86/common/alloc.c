#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#define ALIAS(name) __attribute__((alias(#name), visibility("default")))

extern void* __libc_malloc(size_t);
extern void* __libc_calloc(size_t, size_t);
extern void __libc_free(void*);
extern void* __libc_memalign(size_t, size_t);
extern void* __libc_realloc(void*, size_t);
extern void* __libc_valloc(size_t);
extern int __posix_memalign(void**, size_t, size_t);

void* malloc(size_t) ALIAS(felix86_malloc);
void* calloc(size_t, size_t) ALIAS(felix86_calloc);
void free(void*) ALIAS(felix86_free);
void* memalign(size_t, size_t) ALIAS(felix86_memalign);
void* realloc(void*, size_t) ALIAS(felix86_realloc);
void* valloc(size_t) ALIAS(felix86_valloc);
int posix_memalign(void**, size_t, size_t) ALIAS(felix86_posix_memalign);
void* aligned_alloc(size_t, size_t) ALIAS(felix86_aligned_alloc);

void* validate(void* ptr) {
    uint64_t address = (uint64_t)ptr;
    if (address <= UINT32_MAX) {
        // printf may allocate so let's use write
        static const char* message = "Warning: allocate in 32-bit address space: ";
        const int length = strlen(message);
        char buffer[4096];
        char address_hex[8];
        for (int i = 0; i < 8; i++) {
            uint8_t nibble = (address >> (i * 4));
            if (nibble >= 0 && nibble <= 9) {
                address_hex[7 - i] = '0' + nibble;
            } else if (nibble >= 0xA && nibble <= 0xF) {
                address_hex[7 - i] = 'A' + (nibble - 0xA);
            }
        }

        memcpy(buffer, message, length);
        memcpy(buffer + length, address_hex, 8);
        buffer[length + 8 + 1] = '\n';
        syscall(SYS_write, 2, buffer, length + 8 + 1);
    }

    return ptr;
}

void* felix86_malloc(size_t size) {
    return validate(__libc_malloc(size));
}

void* felix86_calloc(size_t num, size_t size) {
    return validate(__libc_calloc(num, size));
}

void felix86_free(void* ptr) {
    return __libc_free(ptr);
}

void* felix86_memalign(size_t alignment, size_t size) {
    return validate(__libc_memalign(alignment, size));
}

void* felix86_realloc(void* ptr, size_t size) {
    return validate(__libc_realloc(ptr, size));
}

void* felix86_valloc(size_t size) {
    return validate(__libc_valloc(size));
}

int felix86_posix_memalign(void** memptr, size_t alignment, size_t size) {
    int result = posix_memalign(memptr, alignment, size);
    void* address = *memptr;
    validate(address);
    return result;
}

void* felix86_aligned_alloc(size_t alignment, size_t size) {
    return validate(__libc_memalign(alignment, size));
}