#include <cerrno>
#include <cstring>
#include "felix86/common/log.hpp"
#include "felix86/common/process_lock.hpp"

ProcessLock::ProcessLock(SharedMemory& mem) {
    inner = mem.allocate<sem_t>();
    int result = sem_init(inner, 1, 1);
    if (result != 0) {
        ERROR("Failed to initialize semaphore. Error: %s", strerror(errno));
    }
}