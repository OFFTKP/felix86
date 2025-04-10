#pragma once

#include <mutex>
#include "felix86/common/process_lock.hpp"
#include "felix86/common/utility.hpp"

// TODO: add verifications using /proc/self/maps and optional debugging mode that always verifies
struct Mapper {
    [[nodiscard]] void* map(void* addr, u64 size, int prot, int flags, int fd, u64 offset);
    int unmap(void* addr, u64 size);
    [[nodiscard]] void* remap(void* old_address, u64 old_size, u64 new_size, int flags, void* new_address);

    [[nodiscard]] void* map32(void* addr, u64 size, int prot, int flags, int fd, u64 offset);
    int unmap32(void* addr, u64 size);
    [[nodiscard]] void* remap32(void* old_address, u64 old_size, u64 new_size, int flags, void* new_address);

    static constexpr u64 addressSpaceEnd32 = 0xBFFF'FFFF; // 32-bit userspace end

private:
    void initialize();

    // In 32-bit mode we need to handle mmap page finding ourselves
    // Using a simple freelist sounds good
    // Hopefully mmap itself is *rarely* used in 32-bit applications (as the malloc implementation usually prefers using brk)
    // so this overhead isn't that bad
    struct Node {
        Node* next = nullptr;
        u32 start = 0;
        u32 end = 0;
    };

    Node* freelist = nullptr;
    Semaphore lock;
    std::once_flag initialized;
    bool mode32;

    void deleteBlock(Node* current, Node* previous, Node* next);

    std::vector<std::pair<u32, u32>> getRegions();

    [[nodiscard]] void* freelistAllocate(void* addr, u64 size);

    void freelistDeallocate(void* addr, u64 size);

    friend void verifyRegions(Mapper& mapper, const std::vector<std::pair<u32, u32>>& regions);
};
