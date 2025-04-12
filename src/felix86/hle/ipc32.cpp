#include "felix86/common/log.hpp"
#include "felix86/hle/guest_types.hpp"
#include "felix86/hle/ipc32.hpp"

int ipc32(u32 call, u32 first, u64 second, u64 third, void* ptr, u64 fifth) {
    enum {
        felix86_SEMOP = 1,
        felix86_SEMGET = 2,
        felix86_SEMCTL = 3,
        felix86_SEMTIMEDOP = 4,
        felix86_MSGSND = 11,
        felix86_MSGRCV = 12,
        felix86_MSGGET = 13,
        felix86_MSGCTL = 14,
        felix86_SHMAT = 21,
        felix86_SHMDT = 22,
        felix86_SHMGET = 23,
        felix86_SHMCTL = 24,
    };

    u32 operation = call & 0xFFFF;
    switch (operation) {
    case felix86_SEMOP: {
        return ::syscall(SYS_semop, first, (sembuf*)ptr, second);
    }
    case felix86_SEMGET: {
        return ::syscall(SYS_semget, first, second, third);
    }
    default: {
        ERROR("Unknown IPC operation: %d", operation);
        return 0;
    }
    }
}
