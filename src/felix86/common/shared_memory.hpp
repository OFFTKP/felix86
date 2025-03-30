#pragma once

#include "felix86/common/utility.hpp"

struct SharedMemory {
    explicit SharedMemory(size_t size);
    ~SharedMemory();
    SharedMemory(const SharedMemory&) = delete;
    SharedMemory& operator=(const SharedMemory&) = delete;
    SharedMemory(SharedMemory&&) = default;
    SharedMemory& operator=(SharedMemory&&) = default;

    void* allocate(size_t bytes);

    template <typename T>
    T* allocate() {
        return (T*)allocate(sizeof(T));
    }

private:
    u8* memory = nullptr;
    size_t size = 0;
};
