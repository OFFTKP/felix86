#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/hle/mmap.hpp"

void* Mapper::map32(void* addr, u64 size, int prot, int flags, int fd, u64 offset) {
    size = (size + 0xFFFull) & ~0xFFFull;
    auto guard = freelist.lock();

    struct stat64 stat{0};
    stat.st_ino = ino_anon_ctr++;
    if ((flags & MAP_ANONYMOUS) == 0) {
        fstat64(fd, &stat);
    }

    if ((flags & MAP_FIXED) || (flags & MAP_FIXED_NOREPLACE)) {
        // Fixed mapping, make sure it's inside 32-bit address space
        ASSERT_MSG((u64)addr <= UINT32_MAX, "felix86_mmap tried to FIXED allocate outside of 32-bit address space");

        // MAP_FIXED says allocate it at that address, and we don't care if it overlaps with other stuff
        // MAP_FIXED_NOREPLACE will fail if other stuff is at that address
        // If the mapping succeeds, we need to update our freelist accordingly
        void* result = mmap(addr, size, prot, flags, fd, offset);
        if (result == MAP_FAILED) {
            // For some reason the kernel rejected our mapping
            // Just return the result
            i64 error = -errno;
            if (!(flags & MAP_FIXED_NOREPLACE)) {
                WARN("MAP_FIXED 32-bit mapping rejected by kernel: %ld", error);
            } else {
                // Don't warn here, programs can use MAP_FIXED_NOREPLACE to probe memory regions
            }
            return (void*)error;
        }

        if (flags & MAP_FIXED) {
            // Since this mapping could be overwritting another existing mapping, let's do the quick
            // and dirty solution of unallocating it in freelist so we can reallocate it
            ASSERT((u64)result < UINT32_MAX);
            freelist.deallocate((u64)result, size);
            remove_tracked_region((u64)result, size);
        }

        void* mapping = freelist.allocate((u64)result, size);
        ASSERT_MSG(mapping == result, "Failed with mmap(%lx, %lx, %x, %x, %d, %lx)", addr, size, prot, flags, fd, offset);
        add_tracked_region((u64)result, size, prot, stat.st_dev, stat.st_ino, offset, (flags & MAP_SHARED) != 0, -1, (flags & MAP_ANONYMOUS) != 0);
        return result;
    } else {
        void* address = freelist.allocate(0, size);
        if ((i64)address < 0) {
            WARN("freelistAllocate failed for map32: %ld", (i64)address);
            return address;
        }

        void* result = mmap(address, size, prot, flags | MAP_FIXED_NOREPLACE, fd, offset);
        if (result == MAP_FAILED) {
            freelist.deallocate((u64)address, size);
            i64 error = -errno;
            WARN("Even though our freelist says we have memory at %lx-%lx, mmap failed with: %ld", (u64)address, (u64)address + size, error);
            return (void*)error;
        }

        add_tracked_region((u64)result, size, prot, stat.st_dev, stat.st_ino, offset, (flags & MAP_SHARED) != 0, -1, (flags & MAP_ANONYMOUS) != 0);
        ASSERT(result == address);
        return address;
    }
}

int Mapper::unmap32(void* addr, u64 size) {
    size = (size + 0xFFFull) & ~0xFFFull;
    ASSERT((u64)addr <= UINT32_MAX);
    int result = munmap(addr, size);
    if (result != -1) {
        // unmap it from our freelist as well
        auto guard = freelist.lock();
        freelist.deallocate((u64)addr, size);
        // also unmap it from the allocation tracker.
        remove_tracked_region((u64)addr, size);

        return result;
    } else {
        return -errno;
    }
}

void* Mapper::remap32(void* old_address, u64 old_size, u64 new_size, int flags, void* new_address) {
    ASSERT(old_address);
    ASSERT(old_size);
    ASSERT(new_size);

    old_size = (old_size + 0xFFFull) & ~0xFFFull;
    new_size = (new_size + 0xFFFull) & ~0xFFFull;

    auto guard = freelist.lock();

    if ((flags & MREMAP_FIXED) || !(flags & MREMAP_MAYMOVE)) {

        // Give it to the kernel first
        ASSERT((u64)new_address <= UINT32_MAX);
        void* result = ::mremap(old_address, old_size, new_size, flags, new_address);
        if (result == MAP_FAILED) {
            return MAP_FAILED;
        }

        // Since the mapping succeeded we also need to update our freelist allocator
        if (!(flags & MREMAP_DONTUNMAP)) {
            freelist.deallocate((u64)old_address, old_size);
        }

        freelist.deallocate((u64)result, new_size);
        void* mapping = freelist.allocate((u64)result, new_size);
        move_tracked_region((u64)old_address, old_size, (u64)result, new_size, !(flags & MREMAP_DONTUNMAP));
        ASSERT(result == new_address);
        ASSERT(mapping == result);

        return result;
    } else {
        // If we are here it means there's MREMAP_MAYMOVE and not MREMAP_FIXED
        // So we need to find an adequate mapping, pass that to host mremap with MREMAP_FIXED and unmap from freelist
        // Host mremap should not fail if everything is ok
        // Find an adequate mapping in our freelist first
        void* new_address = freelist.allocate(0, new_size);
        if ((i64)new_address <= 0) {
            WARN("freelistAllocate failed with %ld", (i64)new_address);
            return new_address;
        }

        // Actually perform the remap now, but make it fixed
        void* result = ::mremap(old_address, old_size, new_size, flags | MREMAP_MAYMOVE | MREMAP_FIXED, new_address);
        if (result == MAP_FAILED) {
            freelist.deallocate((u64)new_address, new_size);
            return MAP_FAILED;
        }

        // After everything goes ok we can unmap the old region
        if (!(flags & MREMAP_DONTUNMAP)) {
            freelist.deallocate((u64)old_address, old_size);
        }

        move_tracked_region((u64)old_address, old_size, (u64)result, new_size, !(flags & MREMAP_DONTUNMAP));

        ASSERT(result == new_address);
        return result;
    }
}

void* Mapper::map(bool mode32, void* addr, u64 size, int prot, int flags, int fd, u64 offset) {
    if (mode32) {
        return map32(addr, size, prot, flags, fd, offset);
    } else {
        auto guard = freelist.lock();

        size = (size + 0xFFFull) & ~0xFFFull;
        void* result = mmap(addr, size, prot, flags, fd, offset);

        // On success, add tracked allocation.
        if (result != (void*)-1) {
            if ((u64)result <= (u64)UINT32_MAX) {
                u64 to_alloc_s = (u64)result;
                u64 to_alloc_e = std::min(to_alloc_s + size, (u64)UINT32_MAX + 1);
                freelist.allocate(to_alloc_s, to_alloc_e - to_alloc_s);
            }

            struct stat64 stat{0};
            stat.st_ino = ino_anon_ctr++;
            if ((flags & MAP_ANONYMOUS) == 0) {
                fstat64(fd, &stat);
            }

            add_tracked_region((u64)result, size, prot, stat.st_dev, stat.st_ino, offset, (flags & MAP_SHARED) != 0, -1,
                               (flags & MAP_ANONYMOUS) != 0);
        }

        return result;
    }
}

int Mapper::unmap(bool mode32, void* addr, u64 size) {
    if (mode32) {
        return unmap32(addr, size);
    } else {
        auto guard = freelist.lock();

        size = (size + 0xFFFull) & ~0xFFFull;
        int result = munmap(addr, size);

        // On success, remove tracked allocation.
        if (result != -1) {
            remove_tracked_region((u64)addr, size);
            if ((u64)addr <= (u64)UINT32_MAX) {
                u64 to_free_s = std::max((u64)addr, mmap_min_addr());
                u64 to_free_e = std::min((u64)addr + size, (u64)UINT32_MAX + 1);
                if (to_free_e > to_free_s)
                    freelist.deallocate(to_free_s, to_free_e - to_free_s);
            }
        }

        return result;
    }
}

void* Mapper::remap(bool mode32, void* old_address, u64 old_size, u64 new_size, int flags, void* new_address) {
    if (mode32) {
        return remap32(old_address, old_size, new_size, flags, new_address);
    } else {
        if (!(flags & MREMAP_MAYMOVE) && (u64)old_address < UINT32_MAX) {
            // Expands the old mapping, since the old mapping is in 32-bit area we need to pass it to remap32
            ASSERT(!(flags & MREMAP_FIXED));
            return remap32(old_address, old_size, new_size, flags, old_address);
        } else {
            // Lock access to 'allocated_regions'.
            auto guard = freelist.lock();

            old_size = (old_size + 0xFFFull) & ~0xFFFull;
            new_size = (new_size + 0xFFFull) & ~0xFFFull;

            void* result = ::mremap(old_address, old_size, new_size, flags, new_address);

            // On success, remap the tracked allocation
            if (result != (void*)-1) {
                if ((u64)old_address <= (u64)UINT32_MAX && !(flags & MREMAP_DONTUNMAP)) {
                    u64 to_free_s = std::max((u64)old_address, mmap_min_addr());
                    u64 to_free_e = std::min((u64)old_address + old_size, (u64)UINT32_MAX + 1);
                    if (to_free_e > to_free_s)
                        freelist.deallocate(to_free_s, to_free_e - to_free_s);
                }
                if ((u64)result <= (u64)UINT32_MAX) {
                    u64 to_alloc_s = (u64)result;
                    u64 to_alloc_e = std::min(to_alloc_s + new_size, (u64)UINT32_MAX + 1);
                    freelist.allocate(to_alloc_s, to_alloc_e - to_alloc_s);
                }
                move_tracked_region((u64)old_address, old_size, (u64)result, new_size, !(flags & MREMAP_DONTUNMAP));
            }

            return result;
        }
    }
}

int Mapper::protect(void* addr, u64 size, int prot) {
    auto guard = freelist.lock();

    int res = ::mprotect(addr, size, prot);
    if (res != -1)
        move_tracked_region((u64)addr, size, (u64)addr, size, true, prot & (PROT_READ | PROT_WRITE | PROT_EXEC | PROT_GROWSDOWN | PROT_GROWSUP),
                            false);

    return res;
}

std::vector<std::pair<u32, u32>> Mapper::getRegions() {
    auto guard = freelist.lock();
    return freelist.getRegions();
}

int Mapper::shmat(bool mode32, int shmid, void* address, int flags, u64* result_address) {
    auto guard = freelist.lock();

    struct shmid_ds ds;
    int result = shmctl(shmid, IPC_STAT, &ds);
    if (result != 0) {
        WARN("Invalid shmid %d? Could not determine size", shmid);
        return -EINVAL;
    }

    size_t size = ds.shm_segsz;

    if (size & 0xFFF) {
        size_t new_size = (size + 0xFFFull) & ~0xFFFull;
        WARN("shmctl returned size not aligned to a page: %lx, setting to new size: %lx", size, new_size);
        size = new_size;
    }

    ASSERT(size < 0xFFFF'FFFF);

    void *our_mem, *shm_mem;

    if (mode32 && address == nullptr) {
        // Use our freelist allocator to find a region in memory, but don't mmap it
        our_mem = freelist.allocate(0, size);
        if ((i64)our_mem < 0) {
            WARN("freelistAllocate failed for shmat: %ld", (i64)our_mem);
            return (i64)our_mem;
        }

        // Now we need to place the shmat exactly at that address. If that fails then we return an error
        // We can't let the shmat decide for itself what address it wants to go in, because it will
        // almost always choose a 64-bit address which can't be used in 32-bit mode
        shm_mem = ::shmat(shmid, our_mem, flags);
    } else {
        ASSERT_MSG(!mode32 || (u64)address + size <= UINT32_MAX, "shmat segment would end up outside of address space");

        // Since an address is provided by the application, we are going to assume it's
        // inside 32-bit address space and just check after the shmat
        shm_mem = ::shmat(shmid, address, flags);
        if ((i64)shm_mem != -1) {
            u64 top_bits = (u64)shm_mem >> 32;
            ASSERT_MSG(!mode32 || top_bits == 0 || top_bits == 0xFFFF'FFFF, "shmat returned address in 64-bit address space?");

            // Now remove it from the freelist to avoid overlaps
            if (mode32) {
                our_mem = freelist.allocate((u64)shm_mem, size);
                if ((i64)our_mem < 0) {
                    ERROR("shmat succeeded, but freelistAllocate failed for address: %lx", address);
                    return (i64)our_mem;
                }
            }
        } else {
            return (i64)-1ull;
        }
    }

    if (mode32 && shm_mem != our_mem) {
        ERROR("While our freelistAllocate returned %lx, shmat failed to place the segment there and returned %lx", our_mem, shm_mem);
        return (i64)-errno;
    }

    u64 top_bits = (u64)shm_mem >> 32;
    ASSERT_MSG(!mode32 || top_bits == 0 || top_bits == 0xFFFF'FFFF, "shmat returned address in 64-bit address space?");
    *result_address = (u64)shm_mem;
    page_to_shmid[(u64)shm_mem & ~0xFFFull] = shmid;
    int prot = PROT_READ | PROT_WRITE;
    if ((flags & SHM_EXEC) != 0)
        prot |= PROT_EXEC;
    if ((flags & SHM_RDONLY) != 0)
        prot &= ~PROT_WRITE;
    add_tracked_region((u64)shm_mem, size, prot, 0, 0, 0, true, shmid, false);

    return 0;
}

int Mapper::shmdt(bool mode32, void* address) {
    auto guard = freelist.lock();

    auto it = page_to_shmid.find((u64)address & ~0xFFFull);
    if (it == page_to_shmid.end()) {
        IMPORTANT("Could not find page during shmdt: %lx", (u64)address & ~0xFFFull);
        return ::shmdt(address);
    }

    int shmid = it->second;
    size_t size = 0;
    bool size_known = false;
    struct shmid_ds ds;
    if (shmctl(shmid, IPC_STAT, &ds) == 0) {
        size = ds.shm_segsz;
        if (size & 0xFFF) {
            size_t new_size = (size + 0xFFFull) & ~0xFFFull;
            WARN("shmctl returned size not aligned to a page: %lx, setting to new size: %lx", size, new_size);
            size = new_size;
        }
        size_known = true;
    } else {
        WARN("shmctl returned error %d", -errno);
    }

    int result = ::shmdt(address);

    if (result == 0) {
        if (size_known) {
            remove_tracked_region((u64)address, size);
            if (mode32)
                freelist.deallocate((u64)address, size);
        }
        page_to_shmid.erase(it);
    }

    return result;
}

static bool can_guest_regions_merge(GuestRegion& l, GuestRegion& h) {
    return
        // Mappings must share protection.
        l.prot == h.prot
        // Mappings must share device id.
        && l.dev == h.dev
        // If both mappings are not anonymous, do they share the same backing file? If both are anonymous, it is irrelevant.
        && ((l.anonymous && h.anonymous) || ((l.offset + (l.end - l.start) == h.offset) && (l.ino == h.ino)))
        // If both mappings are shared memory, do they refer to the same underlying file/anonymous memory?
        && ((!l.shmem && !h.shmem) || (l.ino == h.ino));
}

void Mapper::add_tracked_region(u64 address, u64 len, int prot, dev_t dev, ino_t ino, u64 offset, bool shmem, int shmid, bool anon) {
    if (len == 0)
        return;

    // Assume 4KiB alignment.
    address = address & ~0xfff;
    u64 end = address + len;
    end = (end + 0xfff) & ~0xfff;

    GuestRegion v = GuestRegion{address, end, prot, dev, ino, offset, shmid, shmem, anon};

    remove_tracked_region(address, len);
    for (auto it = allocated_regions.begin(); it != allocated_regions.end(); it++) {
        auto& r = *it;

        // Region extends another leading.
        if (r.start == end && can_guest_regions_merge(v, r)) {
            r.start = address;
            r.offset = offset;
            return;
        }
        // Region extends another trailing.
        else if (r.end == address && can_guest_regions_merge(r, v)) {
            r.end = std::max(r.end, end);
            // Check if we can continue merging onwards.
            it++;
            while (it != allocated_regions.end()) {
                if ((*it).start <= r.end && can_guest_regions_merge(r, *it)) {
                    r.end = std::max(r.end, (*it).end);
                    it = allocated_regions.erase(it);
                } else {
                    break;
                }
            }
            return;
        } else if (end <= r.start) {
            allocated_regions.insert(it, v);
            return;
        }
    }

    allocated_regions.emplace_back(v);
}

void Mapper::move_tracked_region(u64 old_address, u64 old_len, u64 new_address, u64 new_len, bool remove_src, int new_prot, bool can_grow) {
    old_address &= ~0xfff;
    old_len = (old_len + 0xfff) & ~0xfff;
    new_address &= ~0xfff;
    new_len = (new_len + 0xfff) & ~0xfff;

    std::vector<GuestRegion> to_add;

    bool is_increase = new_len > old_len;
    u64 old_end = old_address + old_len;

    for (auto it = allocated_regions.begin(); it != allocated_regions.end();) {
        auto& r = *it;
        if (r.end <= old_address) {
            it++;
            continue;
        }
        if (r.start >= old_end) {
            break;
        }

        auto n = r;
        n.prot = new_prot == -1 ? n.prot : new_prot;

        if (old_address >= r.start && old_end <= r.end) {
            n.start = new_address;
            n.end = can_grow ? new_address + new_len : new_address + old_len;
            n.offset += old_address - r.start;
            to_add.push_back(n);
            it++;
            continue;
        }

        n.offset += (old_address - r.offset);
        n.start = new_address + std::min((std::max(r.start, old_address) - old_address), new_len);
        n.end = new_address + (is_increase && can_grow ? new_len : std::min((std::min(r.end, old_end) - old_address), new_len));
        to_add.push_back(n);
        it++;

        if (is_increase)
            break;
    }

    ASSERT(!is_increase || to_add.size() <= 1);

    if (remove_src)
        remove_tracked_region(old_address, old_len);

    remove_tracked_region(new_address, new_len);
    for (auto add : std::move(to_add)) {
        add_tracked_region(add.start, add.end - add.start, add.prot, add.dev, add.ino, add.offset, add.shmem, add.shmid, add.anonymous);
    }
}

void Mapper::remove_tracked_region(u64 address, u64 len) {
    if (len == 0)
        return;

    address = address & ~0xfff;
    u64 end = address + len;
    end = (end + 0xfff) & ~0xfff;

    for (auto it = allocated_regions.begin(); it != allocated_regions.end();) {
        auto& r = *it;
        // Because the linked list is ordered, we know if the start address is greater than end,
        // then there is no more regions to remove with this unmap.
        if (end <= r.start || address == end) {
            break;
        }

        /// The mapped region is entirely within the unmapped region
        if (address <= r.start && end >= r.end) {
            it = allocated_regions.erase(it);
            continue;
        }

        /// The unmapped region is entirely within an existing region.
        if (r.start <= address && r.end >= end) {
            auto m = r;
            it = allocated_regions.erase(it);

            if (address - m.start > 0) {
                it = allocated_regions.insert(it, GuestRegion{m.start, address, m.prot, m.dev, m.ino, m.offset, m.shmid, m.shmem, m.anonymous});
                it++;
            }
            if (m.end - end > 0) {
                it = allocated_regions.insert(
                    it, GuestRegion{end, m.end, m.prot, m.dev, m.ino, m.offset + (end - m.start), m.shmid, m.shmem, m.anonymous});
                it++;
            }
            continue;
        }

        /// The unmapped region removes from the left side.
        if (r.start >= address && r.start < end) {
            r.offset += end - r.start;
            r.start = end;
        }

        /// The unmapped region removes from the right side.
        if (r.end > address && r.end <= end) {
            r.end = address;
        }

        if (r.start >= r.end) {
            it = allocated_regions.erase(it);
        } else {
            it++;
        }
    }
}

uint64_t Mapper::total_mapped_memory() {
    // Ensure no race conditions.
    auto guard = freelist.lock();

    uint64_t total_bytes = 0;
    for (auto r : allocated_regions) {
        total_bytes += r.end - r.start;
    }

    return total_bytes;
}

bool Mapper::is_guest_address(void* address) {
    // Ensure no race conditions.
    auto guard = freelist.lock();

    // TODO: could maybe be done faster using caching?
    // Optionally, an ordered map could maybe be iterated faster?
    u64 a = (u64)address;
    for (auto r : allocated_regions) {
        if (r.start <= a && a < r.end) {
            return true;
        }
    }

    return false;
}

std::vector<GuestRegion> Mapper::get_guest_regions() {
    // Ensure no race conditions.
    auto guard = freelist.lock();

    std::vector<GuestRegion> regions;
    for (auto region : allocated_regions) {
        regions.push_back(region);
    }
    return regions;
}
