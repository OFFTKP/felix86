#pragma once

#include <cassert>
#include <pthread.h>
#include <semaphore.h>
#include "felix86/common/decent_rwlock.hpp"

struct SemaphoreGuard {
    explicit SemaphoreGuard(sem_t* sem);

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

    void lock_before_fork();
    void unlock_after_fork();

private:
    sem_t inner;
};

struct RWLockReadGuard {
    explicit RWLockReadGuard(decent_rwlock_t* lock);
    ~RWLockReadGuard();

    RWLockReadGuard(const RWLockReadGuard&) = delete;
    RWLockReadGuard& operator=(const RWLockReadGuard&) = delete;
    RWLockReadGuard(RWLockReadGuard&&) = delete;
    RWLockReadGuard& operator=(RWLockReadGuard&&) = delete;

private:
    decent_rwlock_t* lock;
};

struct RWLockWriteGuard {
    explicit RWLockWriteGuard(decent_rwlock_t* lock);
    ~RWLockWriteGuard();

    RWLockWriteGuard(const RWLockWriteGuard&) = delete;
    RWLockWriteGuard& operator=(const RWLockWriteGuard&) = delete;
    RWLockWriteGuard(RWLockWriteGuard&&) = delete;
    RWLockWriteGuard& operator=(RWLockWriteGuard&&) = delete;

private:
    decent_rwlock_t* lock;
};

struct RWLock {
    [[nodiscard]] RWLockReadGuard lock_read() {
        return RWLockReadGuard(&inner);
    }

    [[nodiscard]] RWLockWriteGuard lock_write() {
        return RWLockWriteGuard(&inner);
    }

    void before_fork() {
        inner.read_lock();
    }

    void after_fork_parent() {
        inner.read_unlock();
    }

    void after_fork_child() {
        inner = {};
    }

private:
    decent_rwlock_t inner{};
};
