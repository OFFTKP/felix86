#pragma once

#include <sys/shm.h>
#include "felix86/common/utility.hpp"

struct SharedMemory {
    SharedMemory() = default; // initializes a null shared memory
    SharedMemory(size_t size);
    ~SharedMemory();
    SharedMemory(const SharedMemory&);
    SharedMemory& operator=(const SharedMemory&);
    SharedMemory(SharedMemory&&) = default;
    SharedMemory& operator=(SharedMemory&&) = default;

    void* allocate(size_t bytes);

    template <typename T>
    T* allocate() {
        return allocate(sizeof(T));
    }

private:
    u8* memory = nullptr;
    size_t size = 0;
};
