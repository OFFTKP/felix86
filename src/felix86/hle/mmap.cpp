#include <sys/mman.h>
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/hle/mmap.hpp"

void* felix86_mmap(void* addr, u64 size, int prot, int flags, int fd, u64 offset) {
    if (g_mode32) {
        // We need these allocations to happen in the 32-bit address space
        // auto lock = g_process_globals.mmap_lock.lock();
        UNREACHABLE();
        return nullptr;
    } else {
        // Nothing to do here
        // In the future if we want to track mmaps we can add something
        return mmap(addr, size, prot, flags, fd, offset);
    }
}