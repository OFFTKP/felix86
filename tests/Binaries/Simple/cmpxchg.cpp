#include <atomic>
#include <cassert>
#include <thread>
#include <vector>
#include "common.h"

constexpr int NUM_THREADS = 8;
constexpr uint64_t INCREMENTS_PER_THREAD = 1000000;

using Type = uint8_t;
std::atomic<Type> lock(0);
uint64_t shared_counter(0);

void acquire_lock(std::atomic<Type>& lock) {
    Type expected;
    while (true) {
        expected = 0;
        if (lock.compare_exchange_strong(expected, 1, std::memory_order_acquire)) {
            return;
        }
    }
}

void release_lock(std::atomic<Type>& lock) {
    lock.store(0, std::memory_order_release);
}

void thread_func() {
    for (int i = 0; i < INCREMENTS_PER_THREAD; ++i) {
        acquire_lock(lock);
        ++shared_counter;
        release_lock(lock);
    }
}

int main() {
    std::vector<std::thread> threads;

    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(thread_func);
    }

    for (auto& t : threads) {
        t.join();
    }

    uint64_t expected = static_cast<uint64_t>(NUM_THREADS) * INCREMENTS_PER_THREAD;
    if (shared_counter != expected) {
        return 1;
    }

    return FELIX86_BTEST_SUCCESS;
}
