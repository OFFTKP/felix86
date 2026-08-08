#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include "felix86/common/config.hpp"
#include "felix86/common/global.hpp"
#include "felix86/common/info.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/shm_stats.hpp"

bool felix86_shm_stats_enabled() {
    return g_config.stats_enabled;
}

void SHMManager::initialize() {
    if (g_emit_stats) {
        name = "fex-" + std::to_string(getpid()) + "-stats";
        int fd = shm_open(name.c_str(), O_CREAT | O_TRUNC | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
        if (fd == -1) {
            WARN("Failed to open shm, disabling stats");
            g_emit_stats = false;
            name = {};
        } else {
            constexpr u64 initial_size = 4096 * 4;
            int result = ftruncate(fd, initial_size);
            if (result != -1) {
                base = mmap(nullptr, FELIX86_MAX_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
                if (base != MAP_FAILED) {
                    void* mem = mmap(base, initial_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
                    if (mem != MAP_FAILED) {
                        current_size = initial_size;
                    } else {
                        WARN("Failed to allocate initial memory, disabling stats");
                        g_emit_stats = false;
                        base = nullptr;
                    }
                } else {
                    WARN("Failed to reserve memory, disabling stats");
                    g_emit_stats = false;
                    base = nullptr;
                }
            } else {
                WARN("Failed to ftruncate, disabling stats");
                g_emit_stats = false;
            }
            close(fd);
        }
    }

    if (g_emit_stats) {
        ASSERT(base);
        stat_base = (FEXCore::SHMStats::ThreadStats*)((u64)base + sizeof(FEXCore::SHMStats::ThreadStatsHeader));
        stat_header = (FEXCore::SHMStats::ThreadStatsHeader*)base;
        stat_header->Version = FEXCore::SHMStats::STATS_VERSION;
        stat_header->app_type = FEXCore::SHMStats::AppType::LINUX_64; // TODO
        stat_header->ThreadStatsSize = sizeof(FEXCore::SHMStats::ThreadStats);
        std::string version = get_version_full();
        strncpy(stat_header->fex_version, version.data(), std::min(sizeof(stat_header->fex_version) - 1, version.size()));
        stat_header->Size = current_size;
    }
}

FEXCore::SHMStats::ThreadStats* SHMManager::findSlot(u32 tid, u64 scan_size) {
    ASSERT(g_emit_stats);
    ASSERT(stat_base);
    FEXCore::SHMStats::ThreadStats* slot = nullptr;
    u64 count = totalSlots(scan_size);
    for (u64 i = 0; i < count; i++) {
        FEXCore::SHMStats::ThreadStats* current = &stat_base[i];
        if (current->TID == 0) {
            memset(current, 0, sizeof(FEXCore::SHMStats::ThreadStats));
            current->TID = tid;
            slot = current;
            break;
        }
    }

    if (slot) {
        u64 offset = (u64)slot - (u64)base;
        if (stat_header->Head == 0) {
            stat_header->Head = offset;
        } else {
            stat_tail->Next = offset;
        }
        stat_tail = slot;
        return slot;
    } else {
        WARN("Slot not found");
        return nullptr;
    }
}

FEXCore::SHMStats::ThreadStats* SHMManager::addThread(u32 tid) {
    if (!g_emit_stats) {
        return nullptr;
    }

    auto guard = lock.lock();
    thread_count.fetch_add(1);

    u64 scan_size = current_size;
    FEXCore::SHMStats::ThreadStats* slot = findSlot(tid, scan_size);
    if (slot) {
        return slot;
    }

    if (current_size == FELIX86_MAX_SHM_SIZE) {
        IMPORTANT("Ran out of shm slots");
        return nullptr;
    }

    const u64 new_size = std::min(current_size * 2, FELIX86_MAX_SHM_SIZE);
    ASSERT(!name.empty());
    int fd = shm_open(name.c_str(), O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
    if (fd == -1) {
        IMPORTANT("Failed to open shm %s", name.c_str());
        return nullptr;
    }

    int result = ftruncate(fd, new_size);
    if (result == -1) {
        IMPORTANT("Failed to increase shm size");
        close(fd);
        return nullptr;
    }

    void* mem = mmap(base, new_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
    if (mem == MAP_FAILED) {
        IMPORTANT("Failed to increase shm mapping");
        close(fd);
        return nullptr;
    }

    close(fd);
    current_size = new_size;
    stat_header->Size = current_size;
    slot = findSlot(tid, new_size);
    if (!slot) {
        IMPORTANT("Couldn't find empty slot after increasing shm size");
    }
    return slot;
}

void SHMManager::removeThread(FEXCore::SHMStats::ThreadStats* slot) {
    if (!g_emit_stats) {
        return;
    }

    auto guard = lock.lock();
    slot->TID = 0;
    const u64 offset = (u64)slot - (u64)base;
    const u64 next = slot->Next;
    if (stat_header->Head == offset) {
        stat_header->Head = next;
        if (stat_tail == slot) {
            stat_tail = nullptr;
        }
    } else {
        u64 slot_count = totalSlots(current_size);
        for (u64 i = 0; i < slot_count; i++) {
            auto current = &stat_base[i];
            if (current->Next == offset) {
                current->Next = next;
                if (stat_tail == slot) {
                    stat_tail = current;
                }
                break;
            }
        }
    }

    if (--thread_count == 0) {
        this->unlink();
    }
}

void SHMManager::unlink() {
    if (!g_emit_stats) {
        return;
    }

    if (!name.empty()) {
        shm_unlink(name.c_str());
    }
}
