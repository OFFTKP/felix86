#pragma once

#include <cassert>
#include <semaphore.h>
#include "felix86/common/shared_memory.hpp"

struct ProcessLockGuard {
    explicit ProcessLockGuard(sem_t* sem) : sem(sem) {
        sem_wait(sem);
    }

    ~ProcessLockGuard() {
        sem_post(sem);
    }

    ProcessLockGuard(const ProcessLockGuard&) = delete;
    ProcessLockGuard& operator=(const ProcessLockGuard&) = delete;
    ProcessLockGuard(ProcessLockGuard&&) = delete;
    ProcessLockGuard& operator=(ProcessLockGuard&&) = delete;

private:
    sem_t* sem;
};

struct ProcessLock {
    ProcessLock() = default;
    explicit ProcessLock(SharedMemory& mem);

    [[nodiscard]] ProcessLockGuard lock() {
        assert(inner != SEM_FAILED);
        return ProcessLockGuard(inner);
    }

private:
    sem_t* inner = SEM_FAILED;
};