#include <algorithm>
#include <sys/mman.h>
#include "felix86/common/state.hpp"
#include "felix86/v2/recompiler.hpp"

constexpr size_t trampoline_storage_size = 1024 * 512;

void ThreadState::InitializeKey() {
    int result = pthread_key_create(&g_thread_state_key, [](void*) {});
    if (result != 0) {
        ERROR("Failed to create thread state key: %s", strerror(result));
        exit(1);
    }
}

ThreadState* ThreadState::Create(ThreadState* copy_state) {
    ThreadState* state = new ThreadState;
    state->recompiler = new Recompiler;

    sigemptyset(&state->signal_mask);

    if (copy_state) {
        for (size_t i = 0; i < sizeof(state->gprs) / sizeof(state->gprs[0]); i++) {
            state->gprs[i] = copy_state->gprs[i];
        }

        for (size_t i = 0; i < sizeof(state->xmm) / sizeof(state->xmm[0]); i++) {
            state->xmm[i] = copy_state->xmm[i];
        }

        for (size_t i = 0; i < sizeof(state->fp) / sizeof(state->fp[0]); i++) {
            state->fp[i] = copy_state->fp[i];
        }

        state->cf = copy_state->cf;
        state->zf = copy_state->zf;
        state->sf = copy_state->sf;
        state->of = copy_state->of;
        state->pf = copy_state->pf;
        state->af = copy_state->af;

        state->fsbase = copy_state->fsbase;
        state->gsbase = copy_state->gsbase;
        state->dsbase = copy_state->dsbase;
        state->csbase = copy_state->csbase;
        state->ssbase = copy_state->ssbase;
        state->esbase = copy_state->esbase;

        state->fs = copy_state->fs;
        state->gs = copy_state->gs;
        state->ds = copy_state->ds;
        state->cs = copy_state->cs;
        state->ss = copy_state->ss;
        state->es = copy_state->es;

        state->alt_stack = copy_state->alt_stack;
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
    ASSERT(g_thread_state_key != (pthread_key_t)-1);
    ASSERT(pthread_getspecific(g_thread_state_key) == nullptr);
    pthread_setspecific(g_thread_state_key, state);
    return state;
}

ThreadState* ThreadState::Get() {
    return (ThreadState*)pthread_getspecific(g_thread_state_key);
}

void ThreadState::Destroy(ThreadState* state) {
    auto lock = g_process_globals.states_lock.lock();
    auto it = std::find(g_process_globals.states.begin(), g_process_globals.states.end(), state);
    if (it != g_process_globals.states.end()) {
        g_process_globals.states.erase(it);
    } else {
        WARN("Thread state %ld not found in global list", state->tid);
    }
    munmap(state->riscv_trampoline_storage_start, trampoline_storage_size);
    munmap(state->x86_trampoline_storage_start, trampoline_storage_size);
    delete state->recompiler;
    delete state;
}
