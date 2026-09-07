#pragma once

#include <list>
#include <unordered_map>
#include <sys/types.h>
#include "felix86/common/freelist.hpp"
#include "felix86/common/types.hpp"
#include "felix86/common/utility.hpp"

struct GuestRegion {
    /// Inclusive end address.
    u64 start;
    /// Non inclusive end address.
    u64 end;
    /// Protection levels of the mapped region.
    int prot;
    /// The device containing the file.
    dev_t dev;
    /// The associated inode. This is an incrementing value in case of anonmyous mapping.
    ino_t ino;
    /// Offset into the associated file.
    u64 offset;
    /// The id if shmat was used for the guest region.
    // If the region is not allocated by shmat, this is -1.
    int shmid;
    /// If the allocated region is shared memory.
    bool shmem;
    /// Determines if the mapping is anonymous.
    bool anonymous;
};

// TODO: add verifications using /proc/self/maps and optional debugging mode that always verifies
struct Mapper {
    [[nodiscard]] void* map(bool mode32, void* addr, u64 size, int prot, int flags, int fd, u64 offset);
    int unmap(bool mode32, void* addr, u64 size);
    [[nodiscard]] void* remap(bool mode32, void* old_address, u64 old_size, u64 new_size, int flags, void* new_address);
    int protect(void* addr, u64 size, int prot);

    [[nodiscard]] void* map32(void* addr, u64 size, int prot, int flags, int fd, u64 offset);
    int unmap32(void* addr, u64 size);
    [[nodiscard]] void* remap32(void* old_address, u64 old_size, u64 new_size, int flags, void* new_address);

    int shmat(bool mode32, int shmid, void* address, int flags, u64* result_address);
    int shmdt(bool mode32, void* address);

    // Directly allocate in the freelist
    void* allocate(u64 addr, u64 size) {
        return freelist.allocate(addr, size);
    }

    /// Return the total amount of bytes allocated by the guest.
    uint64_t total_mapped_memory();

    /// Is the provided address mapped by the guest.
    bool is_guest_address(void* address);

    /// Return the tracked allocated regions.
    std::vector<GuestRegion> get_guest_regions();

private:
    Freelist freelist;
    std::unordered_map<u64, int> page_to_shmid{};
    /// Tracks the amount of currently mapped regions in memory.
    /// This will merge multiple mappings together.
    std::list<GuestRegion> allocated_regions;
    ino_t ino_anon_ctr{0};

    std::vector<std::pair<u32, u32>> getRegions();

    friend void verifyRegions(Mapper& mapper, const std::vector<std::pair<u32, u32>>& regions);

    void add_tracked_region(u64 address, u64 len, int prot, dev_t dev, ino_t ino, u64 offset, bool shmem, int shmid, bool anon);
    void move_tracked_region(u64 old_address, u64 old_len, u64 new_address, u64 new_len, bool remove_source, int new_prot = -1, bool can_grow = true);
    void remove_tracked_region(u64 address, u64 len, bool only_shmat);
};
