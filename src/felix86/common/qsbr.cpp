#include "felix86/common/qsbr.hpp"
#include "felix86/common/state.hpp"

std::atomic<u64> g_global_qsbr_epoch = 0;

void QSBR::enter_quiescent_state() {
    ThreadState* state = ThreadState::Get();
    state->local_qsbr_epoch.store(g_global_qsbr_epoch.load(std::memory_order::acquire), std::memory_order::release);
}

u64 QSBR::increment_global_epoch() {
    return g_global_qsbr_epoch.fetch_add(1, std::memory_order::release) + 1;
}

bool QSBR::is_all_past_epoch(u64 target) {
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
            asm volatile("pause");
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
    state->offline.store(false, std::memory_order::release);
}
