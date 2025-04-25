#include <algorithm>
#include "felix86/common/state.hpp"
#include "felix86/v2/recompiler.hpp"

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

        state->alt_stack = copy_state->alt_stack;
    }

    auto lock = g_process_globals.states_lock.lock();
    g_process_globals.states.push_back(state);
    ASSERT(g_thread_state_key != (pthread_key_t)-1);
    ASSERT(pthread_getspecific(g_thread_state_key) == nullptr);
    pthread_setspecific(g_thread_state_key, state);
    VERBOSE("Created thread state with tid %ld", state->tid);
    return state;
}

ThreadState* ThreadState::Get() {
    return (ThreadState*)pthread_getspecific(g_thread_state_key);
}

void ThreadState::Destroy(ThreadState* state) {
    auto lock = g_process_globals.states_lock.lock();
    state->signals_disabled = true;
    auto it = std::find(g_process_globals.states.begin(), g_process_globals.states.end(), state);
    if (it != g_process_globals.states.end()) {
        g_process_globals.states.erase(it);
    } else {
        WARN("Thread state %ld not found in global list", state->tid);
    }
    delete state->recompiler;
    delete state;
}

SignalGuard::SignalGuard(ThreadState* state) : state(state) {
    state->signals_disabled = true;
}

SignalGuard::~SignalGuard() {
    state->signals_disabled = false;
}