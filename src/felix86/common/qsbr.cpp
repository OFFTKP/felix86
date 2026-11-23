#include "felix86/common/qsbr.hpp"
#include "felix86/common/state.hpp"

std::atomic<u64> g_global_qsbr_epoch = 0;

void QSBR::enter_quiescent_state() {
    ThreadState* state = ThreadState::Get();
    u64 global_epoch = g_global_qsbr_epoch.load(std::memory_order::acquire);
    if (state->local_qsbr_epoch.exchange(global_epoch, std::memory_order::release) != global_epoch) {
        state->clearAddressCache();
    }
}

u64 QSBR::increment_global_epoch() {
    return g_global_qsbr_epoch.fetch_add(1, std::memory_order::release) + 1;
}

bool QSBR::is_all_past_epoch(u64 target) {
    // Make sure any changes we made to the RIP->block table are flushed before loading offline/epoch from other threads
    // This prevents this type of bug:
    // 1. Thread R removes block A from the map, and that store sits in store buffer
    // 2. Thread R checks thread T and it's offline
    // 3. Thread T comes back online and loads block A and gets an old entry
    // 4. Thread R reclaims the segment and overwrites the contents of block A
    std::atomic_thread_fence(std::memory_order::seq_cst);
    auto lock = g_process_globals.states_lock.lock();
    for (auto& state : g_process_globals.states) {
        u64 current = state->local_qsbr_epoch.load(std::memory_order::acquire);
        bool offline = state->offline.load(std::memory_order::acquire);
        if (current >= target || offline) {
            continue;
        } else {
            return false;
        }
    }
    return true;
}

void QSBR::wait_all_past_epoch(u64 target) {
    // This function assumes you already checked is_all_past_epoch manually, so it pokes immediately
    mark_offline();
    poke_threads(target);
    for (u32 i = 0; !is_all_past_epoch(target); i++) {
        if (i < 64) {
#ifdef __riscv
            asm volatile(".option push\n.option arch, +zihintpause\npause\n.option pop");
#endif
        } else if (i < 128) {
            sched_yield();
        } else {
            WARN("Thread is sleeping during wait_all_past_epoch");
            timespec ts{0, 100000};
            nanosleep(&ts, nullptr);
        }
    }
    mark_online();
}

void QSBR::poke_threads(u64 target) {
    auto lock = g_process_globals.states_lock.lock();
    for (auto& state : g_process_globals.states) {
        u64 current = state->local_qsbr_epoch.load(std::memory_order::acquire);
        bool offline = state->offline.load(std::memory_order::acquire);
        if (current >= target || offline) {
            continue;
        } else {
            state->Poke();
        }
    }
}

void QSBR::mark_offline() {
    ThreadState* state = ThreadState::Get();
    state->offline.store(true, std::memory_order::release);
}

void QSBR::mark_online() {
    enter_quiescent_state();
    ThreadState* state = ThreadState::Get();
    state->offline.store(false, std::memory_order::relaxed);

    // Prevent loads from segments past this point to be reordered before marking as online
    // This prevents this type of bug:
    // 1. Thread T is offline
    // 2. Thread T comes online, stores false to offline
    // 3. Thread T loads some blocks from segment, but its online state is not yet externally visible
    // 4. Another thread assumes T is offline, so it reclaims the segment T loaded from
    std::atomic_thread_fence(std::memory_order::seq_cst);
}
