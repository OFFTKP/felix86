#include <cstdint>
#include <sys/mman.h>
#include <sys/shm.h>
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/hle/mmap.hpp"

void* Mapper::map32(void* addr, u64 size, int prot, int flags, int fd, u64 offset) {
    size = (size + 0xFFFull) & ~0xFFFull;
    auto guard = freelist.lock();
    if ((flags & MAP_FIXED) || (flags & MAP_FIXED_NOREPLACE)) {
        // Fixed mapping, make sure it's inside 32-bit address space
        ASSERT_MSG((u64)addr < UINT32_MAX, "felix86_mmap tried to FIXED allocate outside of 32-bit address space");

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
        }

        void* mapping = freelist.allocate((u64)result, size);
        ASSERT_MSG(mapping == result, "Failed with mmap(%lx, %lx, %x, %x, %d, %lx)", addr, size, prot, flags, fd, offset);
        return result;
    } else {
        void* address = freelist.allocate(0, size);
        if ((i64)address < 0) {
            WARN("freelistAllocate failed for map32: %ld", (i64)address);
            return address;
        }

        void* result = mmap(address, size, prot, flags | MAP_FIXED_NOREPLACE, fd, offset);
        if (result == MAP_FAILED) {
            i64 error = -errno;
            WARN("Even though our freelist says we have memory at %lx-%lx, mmap failed with: %ld", (u64)address, (u64)address + size, error);
            return (void*)error;
        }
        ASSERT(result == address);
        return address;
    }
}

int Mapper::unmap32(void* addr, u64 size) {
    size = (size + 0xFFFull) & ~0xFFFull;
    ASSERT((u64)addr < UINT32_MAX);
    int result = munmap(addr, size);
    if (result != -1) {
        auto guard = freelist.lock();
        freelist.deallocate((u64)addr, size); // unmap it from our freelist as well
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
        ASSERT((u64)new_address < UINT32_MAX);
        void* result = ::mremap(old_address, old_size, new_size, flags, new_address);
        if (result == MAP_FAILED) {
            return MAP_FAILED;
        }

        ASSERT(result == new_address);

        // Since the mapping succeeded we also need to update our freelist allocator
        if (!(flags & MREMAP_DONTUNMAP)) {
            freelist.deallocate((u64)old_address, old_size);
        }

        void* mapping = freelist.allocate((u64)result, new_size);
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
            ERROR("Freelist and mremap disagree during mremap32: %ld vs %p", result, new_address);
            freelist.deallocate((u64)new_address, new_size);
            return MAP_FAILED;
        }

        // After everything goes ok we can unmap the old region
        if (!(flags & MREMAP_DONTUNMAP)) {
            freelist.deallocate((u64)old_address, old_size);
        }

        ASSERT(result == new_address);
        return result;
    }
}

void* Mapper::map(void* addr, u64 size, int prot, int flags, int fd, u64 offset) {
    if (g_mode32) {
        return map32(addr, size, prot, flags, fd, offset);
    } else {
        if ((flags & (MAP_FIXED | MAP_FIXED_NOREPLACE)) && (u64)addr < UINT32_MAX) {
            return map32(addr, size, prot, flags, fd, offset);
        } else {
            return mmap(addr, size, prot, flags, fd, offset);
        }
    }
}

int Mapper::unmap(void* addr, u64 size) {
    if (g_mode32) {
        return unmap32(addr, size);
    } else {
        if ((u64)addr < UINT32_MAX) {
            return unmap32(addr, size);
        } else {
            return munmap(addr, size);
        }
    }
}

void* Mapper::remap(void* old_address, u64 old_size, u64 new_size, int flags, void* new_address) {
    if (g_mode32) {
        return remap32(old_address, old_size, new_size, flags, new_address);
    } else {
        if ((flags & MREMAP_FIXED) && (u64)new_address <= UINT32_MAX) {
            return remap32(old_address, old_size, new_size, flags, new_address);
        } else if (!(flags & MREMAP_MAYMOVE) && (u64)old_address < UINT32_MAX) {
            // Expands the old mapping, since the old mapping is in 32-bit area we need to pass it to remap32
            ASSERT(!(flags & MREMAP_FIXED));
            return remap32(old_address, old_size, new_size, flags, old_address);
        } else {
            void* result = ::mremap(old_address, old_size, new_size, flags, new_address);
            // we don't want an allocation in the low 32-bit area
            ASSERT((u64)result > UINT32_MAX);
            return result;
        }
    }
}

std::vector<std::pair<u32, u32>> Mapper::getRegions() {
    auto guard = freelist.lock();
    return freelist.getRegions();
}

int Mapper::shmat32(int shmid, void* address, int flags, u32* result_address) {
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

    if (address == nullptr) {
        // Use our freelist allocator to find a region in memory, but don't mmap it
        our_mem = freelist.allocate(0, size);
        if ((i64)our_mem < 0) {
            WARN("freelistAllocate failed for shmat: %ld", (i64)our_mem);
            return (i64)our_mem;
        }

        // Now we need to place the shmat exactly at that address. If that fails then we return an error
        // We can't let the shmat decide for itself what address it wants to go in, because it will
        // almost always choose a 64-bit address which can't be used in 32-bit mode
        shm_mem = shmat(shmid, our_mem, flags);
    } else {
        ASSERT_MSG((u64)address + size <= UINT32_MAX, "shmat segment would end up outside of address space");

        // Since an address is provided by the application, we are going to assume it's
        // inside 32-bit address space and just check after the shmat
        shm_mem = shmat(shmid, address, flags);
        if ((i64)shm_mem != -1) {
            u64 top_bits = (u64)shm_mem >> 32;
            ASSERT_MSG(top_bits == 0 || top_bits == 0xFFFF'FFFF, "shmat returned address in 64-bit address space?");

            // Now remove it from the freelist to avoid overlaps
            our_mem = freelist.allocate((u64)shm_mem, size);
            if ((i64)our_mem < 0) {
                ERROR("shmat succeeded, but freelistAllocate failed for address: %lx", address);
                return (i64)our_mem;
            }
        } else {
            return (i64)-1ull;
        }
    }

    if (shm_mem != our_mem) {
        ERROR("While our freelistAllocate returned %lx, shmat failed to place the segment there and returned %lx", our_mem, shm_mem);
        return (i64)-errno;
    }

    u64 top_bits = (u64)shm_mem >> 32;
    ASSERT_MSG(top_bits == 0 || top_bits == 0xFFFF'FFFF, "shmat returned address in 64-bit address space?");
    *result_address = (u32)(u64)shm_mem;
    page_to_shmid[(u32)(u64)shm_mem & ~0xFFFull] = shmid;
    return 0;
}

int Mapper::shmdt32(void* address) {
    auto it = page_to_shmid.find((u32)(u64)address & ~0xFFFull);
    if (it == page_to_shmid.end()) {
        WARN("Could not find page during shmdt: %lx", (u64)address & ~0xFFFull);
        return -EINVAL;
    }

    auto guard = freelist.lock();
    int shmid = it->second;
    struct shmid_ds ds;
    if (shmctl(shmid, IPC_STAT, &ds) == 0) {
        size_t size = ds.shm_segsz;
        if (size & 0xFFF) {
            size_t new_size = (size + 0xFFFull) & ~0xFFFull;
            WARN("shmctl returned size not aligned to a page: %lx, setting to new size: %lx", size, new_size);
            size = new_size;
        }
        freelist.deallocate((u64)address, size);
    } else {
        WARN("shmctl returned error %d", -errno);
    }

    int result = shmdt(address);

    if (result == 0) {
        page_to_shmid.erase(it);
    }

    return result;
}