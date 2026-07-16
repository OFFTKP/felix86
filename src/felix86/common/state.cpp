#include <algorithm>
#include <sys/mman.h>
#include "felix86/common/state.hpp"
#include "felix86/hle/fd.hpp"
#include "felix86/hle/ptrace.hpp"
#include "felix86/v2/recompiler.hpp"

constexpr static size_t trampoline_storage_size = 1024 * 512;

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
    // Allocate an extra page before ThreadState which will be used for signal deferring
    u8* state_memory = (u8*)mmap(nullptr, sizeof(ThreadState) + 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASSERT(state_memory != MAP_FAILED);
    u8* state_location = state_memory + 4096;
    ThreadState* state = new (state_location) ThreadState;
    state->recompiler = new Recompiler;
    state->deferred_fault_page = state_memory;
    VERBOSE("ThreadState* for %d is %lx", gettid(), state);
    memset(&state->ptrace_data, 0, sizeof(state->ptrace_data));
    state->ptrace_data.constants.my_tid = gettid();
    state->ptrace_data.constants.my_tgid = getpid();

    sigemptyset(&state->signal_mask);

    if (copy_state) {
        state->ctx = copy_state->ctx;

        state->alt_stack = copy_state->alt_stack;
        state->signal_mask = copy_state->signal_mask;
        state->ptrace_data.constants.tracer_pid = copy_state->ptrace_data.constants.tracer_pid;
        state->ptrace_data.constants.flags = copy_state->ptrace_data.constants.flags;

        // Currently unsupported, warn
        if (copy_state->deferred_signals != 0) {
            WARN("Deferred signals during clone?");
        }
    }

    state->riscv_trampoline_storage =
        (u8*)mmap(nullptr, trampoline_storage_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    state->x86_trampoline_storage = (u8*)mmap(nullptr, trampoline_storage_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    state->riscv_trampoline_storage_start = state->riscv_trampoline_storage;
    state->x86_trampoline_storage_start = state->x86_trampoline_storage;
    ASSERT(state->riscv_trampoline_storage != MAP_FAILED);
    ASSERT(state->x86_trampoline_storage != MAP_FAILED);

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
        WARN("Thread state %ld not found in global list", gettid());
    }
    munmap(state->riscv_trampoline_storage_start, trampoline_storage_size);
    munmap(state->x86_trampoline_storage_start, trampoline_storage_size);
    delete state->recompiler;
    u8* ptr = (u8*)state;
    munmap(ptr - 4096, 4096 + sizeof(ThreadState));
}
