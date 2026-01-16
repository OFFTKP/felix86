#include "felix86/common/freelist.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/utility.hpp"

Freelist::Freelist() {
    list = new Node;
    list->start = mmap_min_addr();
    list->end = UINT32_MAX;
    list->next = nullptr;
}

Freelist::~Freelist() {
    Node* cur = list;
    while (cur) {
        Node* delete_me = cur;
        cur = cur->next;
        delete delete_me;
    }
}

void* Freelist::allocate(u32 addr, size_t size) {
    ASSERT(!(size & 0xFFF));
    if (addr) {
        ASSERT(!(addr & 0xFFF));
        // Scan the list, find the first mapping that is fully inside a node, break it off
        Node* previous = nullptr;
        Node* current = list;
        if ((u64)addr + size > (u64)UINT32_MAX + 1) {
            // There can be a mmap or similar that starts in 32-bit address space and ends outside it
            // For the freelist, we only care to track the 32-bit address space
            size = ((u64)UINT32_MAX + 1) - addr;
        }
        u64 end_addr = addr + size - 1;
        ASSERT(end_addr <= UINT32_MAX);
        bool all_ok = false;
        while (current) {
            if (addr == current->start && end_addr == current->end) {
                if (previous) {
                    previous->next = current->next;
                } else {
                    ASSERT(current == list);
                    list = current->next;
                }
                delete current;
                all_ok = true;
                break;
            } else if (addr > current->start && end_addr == current->end) {
                current->end = addr - 1;
                all_ok = true;
                break;
            } else if (addr == current->start && end_addr < current->end) {
                current->start = end_addr + 1;
                all_ok = true;
                break;
            } else if (addr > current->start && end_addr < current->end) {
                u32 end = current->end;
                current->end = addr - 1;
                Node* next = current->next;
                Node* new_node = new Node;
                new_node->start = end_addr + 1;
                new_node->end = end;
                new_node->next = next;
                current->next = new_node;
                all_ok = true;
                break;
            } else {
                previous = current;
                current = current->next;
            }
        }

        if (!all_ok) {
            WARN("Couldn't find an available mapping in our freelist");
            dump();
            return (void*)-ENOMEM;
        }

        return (void*)(u64)addr;
    } else {
        // We need to choose the first available mapping
        Node* current = list;
        Node* best = nullptr;
        u32 best_size = UINT32_MAX;
        while (current) {
            u32 start = current->start;
            u32 end = current->end + 1;
            u32 cur_size = end - start;

            if (cur_size >= size && (!best || best_size < cur_size)) {
                best = current;
                best_size = cur_size;
            }

            current = current->next;
        }

        if (best) {
            return allocate(best->start, size);
        } else {
            // Couldn't find...
            WARN("Couldn't find an available mapping in our freelist");
            dump();
            return (void*)-ENOMEM;
        }
    }
}

void Freelist::deallocate(u32 addr, size_t size) {
    ASSERT(!(addr & 0xFFF));
    ASSERT(!(size & 0xFFF));

    if ((u64)addr + size > (u64)UINT32_MAX + 1) {
        // There can be a munmap or similar that starts in 32-bit address space and ends outside it
        // For the freelist, we only care to track the 32-bit address space
        size = ((u64)UINT32_MAX + 1) - addr;
    }

    u32 new_start = addr;
    u32 new_end = addr + size - 1;

    // Find where to place the new node
    Node* previous = nullptr;
    Node* current = list;
    bool placed = false;
    while (current) {
        if (new_start >= current->start && new_end <= current->end) {
            // Entirely contained in an existing node, nothing to do
            return;
        }

        if (current->start >= new_start) {
            Node* new_node = new Node;
            new_node->start = new_start;
            new_node->end = new_end;
            new_node->next = current;
            if (previous) {
                previous->next = new_node;
            } else {
                list = new_node;
            }
            placed = true;
            break;
        }

        previous = current;
        current = current->next;
    }

    if (!placed) {
        // We are freeing at the last node
        Node* new_node = new Node;
        new_node->start = new_start;
        new_node->end = new_end;
        new_node->next = nullptr;
        ASSERT(previous->next == nullptr);
        ASSERT(current == nullptr);
        previous->next = new_node;
    }

    consolidate();
}

void Freelist::consolidate() {
    // Obviously there's better ways to do this than a fixpoint algo, but this is simpler
    // and performance for this algorithm doesn't matter right now
    bool changed = false;
    do {
        changed = false;
        Node* current = list;
        while (current) {
            Node* next = current->next;
            if (next) {
                if (current->end + 1 >= next->start) {
                    current->end = std::max(current->end, next->end);
                    current->next = next->next;
                    delete next;
                    changed = true;
                    break;
                }
            }

            current = current->next;
        }
    } while (changed);
}

void Freelist::dump() {
    Node* current = list;
    int i = 0;
    while (current) {
        LOG("Free mapping %d: %lx-%lx", i++, (u64)current->start, (u64)current->end);
        current = current->next;
    }
}