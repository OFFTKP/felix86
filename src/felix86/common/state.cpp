#include <algorithm>
#include "felix86/common/state.hpp"
#include "felix86/v2/recompiler.hpp"

ThreadState::ThreadState(ThreadState* copy_state) {
    recompiler = std::make_unique<Recompiler>();

    sigemptyset(&signal_mask);

    if (copy_state) {
        for (size_t i = 0; i < sizeof(this->gprs) / sizeof(this->gprs[0]); i++) {
            this->gprs[i] = copy_state->gprs[i];
        }

        for (size_t i = 0; i < sizeof(this->xmm) / sizeof(this->xmm[0]); i++) {
            this->xmm[i] = copy_state->xmm[i];
        }

        for (size_t i = 0; i < sizeof(this->fp) / sizeof(this->fp[0]); i++) {
            this->fp[i] = copy_state->fp[i];
        }

        this->cf = copy_state->cf;
        this->zf = copy_state->zf;
        this->sf = copy_state->sf;
        this->of = copy_state->of;
        this->pf = copy_state->pf;
        this->af = copy_state->af;

        this->fsbase = copy_state->fsbase;
        this->gsbase = copy_state->gsbase;

        this->alt_stack = copy_state->alt_stack;
    }
}

void ThreadState::InitializeKey() {
    int result = pthread_key_create(&g_thread_state_key, [](void*) {});
    if (result != 0) {
        ERROR("Failed to create thread state key: %s", strerror(result));
        exit(1);
    }
}

ThreadState* ThreadState::Create(ThreadState* copy_state) {
    ThreadState* state = new ThreadState(copy_state);
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
    delete state;
}

SignalGuard::SignalGuard(ThreadState* state) : state(state) {
    state->signals_disabled = true;
}

SignalGuard::~SignalGuard() {
    state->signals_disabled = false;
}