#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include "felix86/common/log.hpp"
#include "felix86/common/shared_memory.hpp"

SharedMemory::SharedMemory(size_t size) : size(size) {
    ASSERT(size >= 0x1000);
    memory = (u8*)mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) {
        ERROR("Failed to allocate shared memory. Error: %s", strerror(errno));
        return;
    }

    // Our shared memory is going to look like this:
    // 8 bytes for cursor
    // Rest of the data is usable
    u64 cursor = (u64)memory + sizeof(u64);
    memcpy(memory, &cursor, sizeof(u64));
}

SharedMemory::~SharedMemory() {
    int result = munmap(memory, size);
    if (result != 0) {
        WARN("Failed to unmap shared memory. Error: %s", strerror(errno));
    }
}

void* SharedMemory::allocate(size_t bytes) {
    // Fetch current cursor, and increment it by the bytes we allocate, return that
    void* data = (void*)__atomic_fetch_add((u64*)(memory), bytes, __ATOMIC_RELAXED);
    ASSERT_MSG(data < memory + size, "Shared memory ran out of space");
    return data;
}