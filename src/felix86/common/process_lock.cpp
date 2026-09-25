#include <cerrno>
#include <cstring>
#include "felix86/common/log.hpp"
#include "felix86/common/process_lock.hpp"

static void lock_semaphore(sem_t* sem) {
    while (true) {
        int result = sem_wait(sem);
        if (result == 0) {
            return;
        } else if (errno == EINTR) {
            continue;
        } else {
            IMPORTANT("Failed to lock semaphore with error %d", errno);
            break;
        }
    }
}

Semaphore::Semaphore() {
    int result = sem_init(&inner, 0, 1);
    if (result != 0) {
        ERROR("Failed to initialize semaphore. Error: %s", strerror(errno));
    }
}

SemaphoreGuard::SemaphoreGuard(sem_t* sem) : sem(sem) {
    lock_semaphore(sem);
}

void Semaphore::lock_before_fork() {
    lock_semaphore(&inner);
}

void Semaphore::unlock_after_fork() {
    sem_post(&inner);
}

RWLockReadGuard::RWLockReadGuard(decent_rwlock_t* lock) : lock(lock) {
    lock->read_lock();
}

RWLockReadGuard::~RWLockReadGuard() {
    lock->read_unlock();
}

RWLockWriteGuard::RWLockWriteGuard(decent_rwlock_t* lock) : lock(lock) {
    lock->write_lock();
}

RWLockWriteGuard::~RWLockWriteGuard() {
    lock->write_unlock();
}
