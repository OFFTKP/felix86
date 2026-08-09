#pragma once

#include <string>
#include "FEXCore/Utils/SHMStats.h"
#include "felix86/common/process_lock.hpp"
#include "felix86/common/types.hpp"

constexpr u64 FELIX86_MAX_SHM_SIZE = 16 * 1024 * 1024;

// Our ThreadState struct is different
#define FELIX86_PROFILE_ACCUMULATION(ThreadStats, Stat)                                                                                              \
    FEXCore::SHMStats::AccumulationBlock<decltype(ThreadStats->Stat)> UniqueScopeName(ScopedAccumulation_,                                           \
                                                                                      __LINE__)(ThreadStats ? &ThreadStats->Stat : nullptr);
#define FELIX86_PROFILE_INSTANT_INCREMENT(ThreadStats, Stat, value)                                                                                  \
    do {                                                                                                                                             \
        if (ThreadStats) {                                                                                                                           \
            ThreadStats->Stat += value;                                                                                                              \
        }                                                                                                                                            \
    } while (0)

bool felix86_shm_stats_enabled();

struct SHMManager {
    void initialize();

    FEXCore::SHMStats::ThreadStats* addThread(u32 tid);
    void removeThread(FEXCore::SHMStats::ThreadStats* slot);

    // Called from exit_group, termination signal, or when all threads have exited
    void unlink();

private:
    std::atomic_uint64_t thread_count = {0};
    std::string name{};
    void* base{};
    FEXCore::SHMStats::ThreadStats* stat_base{};
    FEXCore::SHMStats::ThreadStats* stat_tail{};
    FEXCore::SHMStats::ThreadStatsHeader* stat_header{};
    std::atomic_uint64_t current_size{};
    Semaphore lock{};

    static u64 totalSlots(u64 size) {
        return (size - sizeof(FEXCore::SHMStats::ThreadStatsHeader)) / sizeof(FEXCore::SHMStats::ThreadStats) - 1;
    }

    FEXCore::SHMStats::ThreadStats* findSlot(u32 tid, u64 scan_size);
};
