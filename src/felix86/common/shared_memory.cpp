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

    // Our shared memory is going to look like this:
    // 8 bytes for atomic reference counter
    // 8 bytes for cursor
    // Rest of the data is usable
    u8* cursor = memory + sizeof(u64) * 2;
    u64 counter = 1;
    memcpy(memory, &counter, sizeof(u64));
    memcpy(memory + sizeof(u64), cursor, sizeof(u8*));
}

SharedMemory::SharedMemory(const SharedMemory& other) {
    *this = other;
}

SharedMemory& SharedMemory::operator=(const SharedMemory& other) {
    ASSERT(other.memory != nullptr);
    __atomic_fetch_add(other.memory, 1, __ATOMIC_RELAXED);
    memory = other.memory;
    size = other.size;

    return *this;
}

SharedMemory::~SharedMemory() {
    u64 counter = __atomic_sub_fetch(memory, 1, __ATOMIC_RELAXED);
    if (counter == 0) {
        int result = munmap(memory, size);
        if (result != 0) {
            WARN("Failed to unmap shared memory. Error: %s", strerror(errno));
        }
    }
}

void* SharedMemory::allocate(size_t bytes) {
    // Fetch current cursor, and increment it by the bytes we allocate, return that
    void* data = (void*)__atomic_fetch_add((u64*)(memory + 8), bytes, __ATOMIC_RELAXED);
    ASSERT_MSG(data < memory + size, "Shared memory ran out of space");
    return data;
}