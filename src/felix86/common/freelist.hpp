#pragma once

#include <vector>
#include "felix86/common/process_lock.hpp"
#include "felix86/common/types.hpp"

// A freelist allocator implementation that matches our needs
struct Freelist {
    Freelist();
    ~Freelist();

    // Mark a region of memory as allocated
    void* allocate(u32 addr, u64 size);

    // Free a region of memory
    void deallocate(u32 addr, u64 size);

    SemaphoreGuard lock() {
        return sem.lock();
    }

    std::vector<std::pair<u32, u32>> getRegions() {
        std::vector<std::pair<u32, u32>> ret;
        Node* current = list;
        while (current) {
            ret.push_back({current->start, current->end});
            current = current->next;
        }
        return ret;
    }

private:
    struct Node {
        u32 start;
        u32 end;
        Node* next;
    };

    Node* list = nullptr;
    Semaphore sem{};

    void consolidate();

    void dump();
};