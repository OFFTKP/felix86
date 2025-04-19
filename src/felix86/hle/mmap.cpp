#include <sys/mman.h>
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/hle/mmap.hpp"

#define PAGE_SIZE 4096

void Mapper::initialize() {
    std::call_once(initialized, [&]() {
        freelist = new Node;
        freelist->start = mmap_min_addr();
        freelist->end = addressSpaceEnd32;
        freelist->next = nullptr;
    });
}

void* Mapper::map32(void* addr, u64 size, int prot, int flags, int fd, u64 offset) {
    initialize();
    auto guard = lock.lock();
    if ((flags & MAP_FIXED) || (flags & MAP_FIXED_NOREPLACE)) {
        // Fixed mapping, make sure it's inside 32-bit address space
        ASSERT_MSG((u64)addr < addressSpaceEnd32, "felix86_mmap tried to FIXED allocate outside of 32-bit address space");

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
            freelistDeallocate(result, size);
        }

        if (size & 0xFFF) {
            size = (size + PAGE_SIZE) & ~0xFFF;
        }

        void* mapping = freelistAllocate(result, size);
        ASSERT(mapping == result);
        return result;
    } else {
        if (size & 0xFFF) {
            size = (size + PAGE_SIZE) & ~0xFFF;
        }

        void* address = freelistAllocate(nullptr, size);
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

void* Mapper::freelistAllocate(void* addr, u64 size) {
    if (addr == nullptr) {
        // We need to use our freelist to find a free mapping that has enough size
        // Also we completely ignore the addr hint, since there's no MAP_FIXED this should be fine...
        // Iterate the free list...
        Node* previous = nullptr;
        Node* current = freelist;
        while (current) {
            u64 current_size = current->end - current->start;
            if (size > current_size) {
                // Not enough space in this free space
                previous = current;
                current = current->next;
                continue;
            } else if (size < current_size) {
                // We can allocate here
                void* address = (void*)(u64)current->start;
                current->start = current->start + size;
                return address;
            } else if (size == current_size) {
                // Just enough -- link previous with next and delete the current listing
                deleteBlock(current, previous, current->next);
                UNREACHABLE(); // TODO: finish this and test
                break;
            }
        }
        ERROR("Freelist allocator ran out of free spaces when trying to allocate %lx bytes", size);
        return (void*)-ENOMEM;
    } else {
        u64 mmap_start = (u64)addr;
        u64 mmap_end = mmap_start + size;
        ASSERT(mmap_start <= addressSpaceEnd32 - 0x1000);
        ASSERT(mmap_end <= addressSpaceEnd32 + 1);
        Node* current = freelist;
        ASSERT(current);
        Node* previous = nullptr;
        bool all_ok = false; // makes sure the freelist agrees with host mmap
        while (current) {
            if (mmap_start == current->start && mmap_end - 1 == current->end) {
                // The mmap is exactly this block, delete it and break
                all_ok = true;
                deleteBlock(current, previous, current->next);
                break;
            }

            if (mmap_start >= current->start && mmap_start <= current->end) {
                all_ok = true;
                // The mmap definitely starts in this block, the problem is where does it end
                if (mmap_end - 1 <= current->end) {
                    // Ok the mmap is entirely contained in this singular block
                    // This means that a new block needs to be inserted to split the current block
                    u64 new_block_start = mmap_end;
                    u64 new_block_end = current->end;

                    // End the current block where the mmap starts
                    current->end = mmap_start - 1;

                    if (new_block_start != new_block_end + 1) {
                        Node* new_node = new Node;
                        new_node->start = new_block_start;
                        new_node->end = new_block_end;
                        new_node->next = current->next;
                        current->next = new_node;
                    }

                    if (current->start == current->end + 1) {
                        // We consumed the entire block
                        deleteBlock(current, previous, current->next);
                    }
                    break;
                } else {
                    // Allocation starts here but spans multiple blocks
                    // End the current block where the mmap starts
                    current->end = mmap_start - 1;

                    // Find the block that this allocation ends in
                    previous = current;
                    current = current->next;
                    while (current) {
                        if (mmap_end >= current->start && mmap_end - 1 <= current->end) {
                            // mmap ends in this block, we are finished here
                            current->start = mmap_end;
                            if (current->start == current->end + 1) {
                                // We consumed the entire block
                                deleteBlock(current, previous, current->next);
                            }
                            break;
                        } else {
                            ASSERT(mmap_end >= current->start);
                            // mmap ends past this block, delete this block
                            Node* next = current->next;
                            deleteBlock(current, previous, next);
                            current = next;
                        }
                    }
                    break;
                }
                UNREACHABLE();
            } else {
                // Doesn't start in this block, continue...
                previous = current;
                current = current->next;
                continue;
            }
            UNREACHABLE();
        }
        ASSERT(all_ok);
        return addr;
    }
}

int Mapper::unmap32(void* addr, u64 size) {
    initialize();
    ASSERT((u64)addr < addressSpaceEnd32);
    int result = munmap(addr, size);
    if (result != -1) {
        freelistDeallocate(addr, size); // unmap it from our freelist as well
        return result;
    } else {
        return -errno;
    }
}

void* Mapper::remap32(void* old_address, u64 old_size, u64 new_size, int flags, void* new_address) {
    ASSERT(old_address);
    ASSERT(old_size);
    ASSERT(new_size);

    VERBOSE("Calling remap32 old: [%p, %zu] -> [%p, %zu]", old_address, old_size, new_address, new_size);

    auto guard = lock.lock();

    if (new_size & 0xFFF) {
        new_size = (new_size + PAGE_SIZE) & ~0xFFF;
    }

    if ((flags & MREMAP_FIXED) || !(flags & MREMAP_MAYMOVE)) {
        // Give it to the kernel first
        ASSERT((u64)new_address < addressSpaceEnd32);
        void* result = ::mremap(old_address, old_size, new_size, flags, new_address);
        if (result == MAP_FAILED) {
            return MAP_FAILED;
        }

        ASSERT(result == new_address);

        // Since the mapping succeeded we also need to update our freelist allocator
        if (!(flags & MREMAP_DONTUNMAP)) {
            freelistDeallocate(old_address, old_size);
        }

        void* mapping = freelistAllocate(result, new_size);
        ASSERT(mapping == result);
        return result;
    } else {
        // If we are here it means there's MREMAP_MAYMOVE and not MREMAP_FIXED
        // So we need to find an adequate mapping, pass that to host mremap with MREMAP_FIXED and unmap from freelist
        // Host mremap should not fail if everything is ok
        // Find an adequate mapping in our freelist first
        void* new_address = freelistAllocate(nullptr, new_size);
        if ((i64)new_address <= 0) {
            WARN("freelistAllocate failed with %ld", (i64)new_address);
            return new_address;
        }

        // Actually perform the remap now, but make it fixed
        void* result = ::mremap(old_address, old_size, new_size, flags | MREMAP_MAYMOVE | MREMAP_FIXED, new_address);
        if (result == MAP_FAILED) {
            ERROR("Freelist and mremap disagree during mremap32: %ld vs %p", result, new_address);
            freelistDeallocate(new_address, new_size);
            return MAP_FAILED;
        }

        // After everything goes ok we can unmap the old region
        if (!(flags & MREMAP_DONTUNMAP)) {
            freelistDeallocate(old_address, old_size);
        }

        ASSERT(result == new_address);
        return result;
    }
}

void* Mapper::map(void* addr, u64 size, int prot, int flags, int fd, u64 offset) {
    initialize();
    if (g_mode32) {
        return map32(addr, size, prot, flags, fd, offset);
    } else {
        // Nothing to do here
        // In the future if we want to track mmaps we can add something
        return mmap(addr, size, prot, flags, fd, offset);
    }
}

int Mapper::unmap(void* addr, u64 size) {
    initialize();
    if (g_mode32) {
        return unmap32(addr, size);
    } else {
        return munmap(addr, size);
    }
}

void* Mapper::remap(void* old_address, u64 old_size, u64 new_size, int flags, void* new_address) {
    initialize();
    if (g_mode32) {
        return remap32(old_address, old_size, new_size, flags, new_address);
    } else {
        if ((flags & MREMAP_FIXED) && (u64)new_address <= addressSpaceEnd32) {
            return remap32(old_address, old_size, new_size, flags, new_address);
        } else {
            void* result = ::mremap(old_address, old_size, new_size, flags, new_address);
            ASSERT((u64)result > addressSpaceEnd32); // we don't want an allocation in the low 32-bit area
            return result;
        }
    }
}

void Mapper::freelistDeallocate(void* addr, u64 size) {
    u64 unmap_start = (u64)addr;
    if (size & 0xFFF) {
        size = ((size + PAGE_SIZE) & ~0xFFF);
    }
    u64 unmap_end = unmap_start + size;
    ASSERT(unmap_start <= addressSpaceEnd32 - 0x1000);
    ASSERT(unmap_end <= addressSpaceEnd32 + 1);
    Node* current = freelist;
    Node* min = nullptr;
    Node* max = nullptr;
    Node* nearest = nullptr;
    while (current) {
        if (unmap_start >= current->start && unmap_end <= current->end + 1) {
            // Area that was munmap'ed is not mapped to anything
            // as it is entirely contained in a block, return early
            return;
        }

        if (unmap_start >= current->start && unmap_start <= current->end + 1) {
            min = current;
        }

        if (unmap_end >= current->start && unmap_end <= current->end + 1) {
            max = current;
        }

        if (current->next) {
            if (unmap_start > current->end && unmap_end < current->next->start) {
                nearest = current;
            }
        }

        current = current->next;
    }

    if (min && !max) {
        min->end = unmap_end - 1;
    } else if (!min && max) {
        max->start = unmap_start;
    } else if (min && max) {
        // Link min and max, delete everything in between
        current = min->next;
        Node* max_next = max->next;
        while (current != max_next) {
            Node* next = current->next;
            min->end = current->end;
            deleteBlock(current, min, next);
            current = next;
        }
    } else {
        // No min or max, means unmap in the middle of allocated region
        // Create a new free block
        Node* new_node = new Node;
        new_node->start = unmap_start;
        new_node->end = unmap_end - 1;

        if (nearest) {
            new_node->next = nearest->next;
            nearest->next = new_node;
        } else {
            // It is the new first block
            new_node->next = freelist;
            freelist = new_node;
        }
    }
}

void Mapper::deleteBlock(Node* current, Node* previous, Node* next) {
    if (previous) {
        previous->next = next;
    } else {
        ASSERT(current == freelist);
        freelist = next;
    }

    delete current;
}

std::vector<std::pair<u32, u32>> Mapper::getRegions() {
    std::vector<std::pair<u32, u32>> result;

    Node* current = freelist;
    while (current) {
        result.push_back({current->start, current->end});
        current = current->next;
    }

    return result;
}
