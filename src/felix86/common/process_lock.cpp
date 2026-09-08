#include <cerrno>
#include <cstring>
#include "felix86/common/log.hpp"
#include "felix86/common/process_lock.hpp"

Semaphore::Semaphore() {
    int result = sem_init(&inner, 0, 1);
    if (result != 0) {
        ERROR("Failed to initialize semaphore. Error: %s", strerror(errno));
    }
}

SemaphoreGuard::SemaphoreGuard(sem_t* sem) : sem(sem) {
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
