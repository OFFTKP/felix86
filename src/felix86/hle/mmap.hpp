#pragma once

#include <unordered_map>
#include "felix86/common/freelist.hpp"
#include "felix86/common/types.hpp"

// TODO: add verifications using /proc/self/maps and optional debugging mode that always verifies
struct Mapper {
    [[nodiscard]] void* map(void* addr, u64 size, int prot, int flags, int fd, u64 offset);
    int unmap(void* addr, u64 size);
    [[nodiscard]] void* remap(void* old_address, u64 old_size, u64 new_size, int flags, void* new_address);

    [[nodiscard]] void* map32(void* addr, u64 size, int prot, int flags, int fd, u64 offset);
    int unmap32(void* addr, u64 size);
    [[nodiscard]] void* remap32(void* old_address, u64 old_size, u64 new_size, int flags, void* new_address);

    int shmat32(int shmid, void* address, int flags, u32* result_address);
    int shmdt32(void* address);

    // Directly allocate in the freelist
    void* allocate(u64 addr, u64 size) {
        return freelist.allocate(addr, size);
    }

private:
    Freelist freelist;
    std::unordered_map<u32, int> page_to_shmid{};

    std::vector<std::pair<u32, u32>> getRegions();

    friend void verifyRegions(Mapper& mapper, const std::vector<std::pair<u32, u32>>& regions);
};
