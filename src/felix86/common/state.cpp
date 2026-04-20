#include <algorithm>
#include <sys/mman.h>
#include "felix86/common/state.hpp"
#include "felix86/hle/fd.hpp"
#include "felix86/hle/ptrace.hpp"
#include "felix86/v2/recompiler.hpp"

constexpr size_t trampoline_storage_size = 1024 * 512;

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

void ThreadState::CreatePtracePage(ThreadState* state) {
    char buffer[128];
    int written = snprintf(buffer, sizeof(buffer), "ptrace-memfd-%d", gettid());
    ASSERT(written > 0 && written < sizeof(buffer));
    state->ptrace_fd = memfd_create(buffer, MFD_CLOEXEC);
    ASSERT(state->ptrace_fd >= 0);
    state->ptrace_fd = FD::moveToHighNumber(state->ptrace_fd);
    FD::protect(state->ptrace_fd);
    ASSERT(ftruncate(state->ptrace_fd, 4096) == 0);
    state->ptrace_page = (PtracePage*)mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, state->ptrace_fd, 0);
    ASSERT(state->ptrace_page != MAP_FAILED);
    state->ptrace_page->constants.state = state;
    state->ptrace_page->constants.tracer_pid = 0;
    state->ptrace_page->constants.in_clone = false;
    state->ptrace_page->constants.my_tid = gettid();
    state->ptrace_page->constants.my_tgid = getpid();
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
    CreatePtracePage(state);

    sigemptyset(&state->signal_mask);

    if (copy_state) {
        state->ctx = copy_state->ctx;

        state->alt_stack = copy_state->alt_stack;
        state->signal_mask = copy_state->signal_mask;

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
    munmap(state->ptrace_page, 4096);
    FD::unprotectAndClose(state->ptrace_fd);
    delete state->recompiler;
    u8* ptr = (u8*)state;
    munmap(ptr - 4096, 4096 + sizeof(ThreadState));
}
