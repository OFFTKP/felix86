#pragma once

#include <cassert>
#include <semaphore.h>

struct SemaphoreGuard {
    explicit SemaphoreGuard(sem_t* sem) : sem(sem) {
        sem_wait(sem);
    }

    ~SemaphoreGuard() {
        sem_post(sem);
    }

    SemaphoreGuard(const SemaphoreGuard&) = delete;
    SemaphoreGuard& operator=(const SemaphoreGuard&) = delete;
    SemaphoreGuard(SemaphoreGuard&&) = delete;
    SemaphoreGuard& operator=(SemaphoreGuard&&) = delete;

private:
    sem_t* sem;
};

struct Semaphore {
    Semaphore();

    [[nodiscard]] SemaphoreGuard lock() {
        return SemaphoreGuard(&inner);
    }

private:
    sem_t inner;
};