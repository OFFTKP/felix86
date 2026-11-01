#include <algorithm>
#include <sys/mman.h>
#include "felix86/common/state.hpp"
#include "felix86/hle/fd.hpp"
#include "felix86/hle/ptrace.hpp"
#include "felix86/v2/recompiler.hpp"

__attribute__((naked)) static void set_thread_state(ThreadState* state) {
#ifdef __riscv
    asm volatile(R"(
        mv gp, a0
        ret
    )");
#else
#warning "Unimplemented"
#endif
}

void ThreadState::Set(ThreadState* state) {
    set_thread_state(state);
}

ThreadState* ThreadState::Create(ThreadState* copy_state) {
    // Allocating the address cache near the ThreadState allows us to access it with a LUI+ADDI+ADD
    // Allocate an extra page before ThreadState which will be used for signal deferring
    const u64 total_size = address_cache_size + sizeof(ThreadState) + deferred_fault_page_offset;
    // Reserve an extra 2MB of virtual address space and skip it so our mapping is 2MB aligned
    void* reservation_base = mmap(nullptr, 2 * MB + total_size, PROT_NONE, MAP_NORESERVE | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASSERT(reservation_base != MAP_FAILED);
    void* real_base = (void*)(((u64)reservation_base + 2 * MB - 1) & ~(2 * MB - 1));
    void* real_memory = mmap(real_base, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    ASSERT(real_memory != MAP_FAILED);
    u8* state_location = (u8*)real_memory + address_cache_offset;
    ThreadState* state = new (state_location) ThreadState;
    state->recompiler = new Recompiler;
    u32 tid = gettid();
    VERBOSE("ThreadState* for %d is %lx", tid, (u64)state);
    memset(&state->ptrace_data, 0, sizeof(state->ptrace_data));
    state->ptrace_data.constants.my_tid = tid;
    state->ptrace_data.constants.my_tgid = getpid();
    state->reservation_base = reservation_base;

    madvise(state->getAddressCache(), address_cache_size, MADV_HUGEPAGE);
    if (g_config.address_cache) {
        state->getAddressCache()[0].guest = -1ull;
    }

    if (g_emit_stats) {
        state->thread_stats = g_process_globals.shm_manager.addThread(tid);
    }

    sigemptyset(&state->signal_mask);

    if (copy_state) {
        state->ctx = copy_state->ctx;

        state->alt_stack = copy_state->alt_stack;
        state->signal_mask = copy_state->signal_mask;
        state->persona = copy_state->persona;
        state->ptrace_data.constants.tracer_pid = copy_state->ptrace_data.constants.tracer_pid;
        state->ptrace_data.constants.parent_tid = copy_state->ptrace_data.constants.parent_tid;
        state->ptrace_data.constants.flags = copy_state->ptrace_data.constants.flags;

        // Currently unsupported, warn
        if (copy_state->deferred_signals != 0) {
            WARN("Deferred signals during clone?");
        }
    }

    auto lock = g_process_globals.states_lock.lock();
    g_process_globals.states.push_back(state);
    ThreadState::Set(state);
    return state;
}

__attribute__((naked)) ThreadState* ThreadState::Get() {
#ifdef __riscv
    asm volatile(R"(
        mv a0, gp
        ret
    )");
#else
#warning "Unimplemented"
#endif
}
static_assert(Recompiler::threadStatePointer() == gp);

void ThreadState::Destroy(ThreadState* state) {
    auto lock = g_process_globals.states_lock.lock();
    auto it = std::find(g_process_globals.states.begin(), g_process_globals.states.end(), state);
    if (it != g_process_globals.states.end()) {
        g_process_globals.states.erase(it);
    } else {
        WARN("Thread state %d not found in global list", gettid());
    }
    delete state->recompiler;
    const u64 total_size = address_cache_size + sizeof(ThreadState) + deferred_fault_page_offset;
    munmap(state->reservation_base, 2 * MB + total_size);
}
