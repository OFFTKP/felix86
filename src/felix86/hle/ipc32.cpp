#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/hle/guest_types.hpp"
#include "felix86/hle/ipc32.hpp"
#include "felix86/hle/mmap.hpp"

#define SHM_LOCK 11
#define SHM_UNLOCK 12
#define SHM_STAT 13
#define SHM_INFO 14
#define SHM_STAT_ANY 15

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
    case felix86_SEMCTL: {
        u64 semid = first;
        u64 semnum = second;
        u64 semcmd = third & 0xFF;
        bool ipc64 = third & 0x100;
        x86_semun* semun = (x86_semun*)ptr;
        switch (semcmd) {
        case IPC_SET: {
            riscv64_semid64_ds host_semid{};
            if (ipc64) {
                host_semid = *(x86_semid_ds_64*)(u64)semun->_u32;
            } else {
                host_semid = *(x86_semid_ds_32*)(u64)semun->_u32;
            }
            int result = ::syscall(SYS_semctl, semid, semnum, semcmd, &host_semid);
            if (result != -1) {
                if (ipc64) {
                    *(x86_semid_ds_64*)(u64)semun->_u32 = host_semid;
                } else {
                    *(x86_semid_ds_32*)(u64)semun->_u32 = host_semid;
                }
            }
            return result;
        }
        case SEM_STAT:
        case SEM_STAT_ANY:
        case IPC_STAT: {
            riscv64_semid64_ds host_semid{};
            int result = ::syscall(SYS_semctl, semid, semnum, semcmd, &host_semid);
            if (result != -1) {
                if (ipc64) {
                    *(x86_semid_ds_64*)(u64)semun->_u32 = host_semid;
                } else {
                    *(x86_semid_ds_32*)(u64)semun->_u32 = host_semid;
                }
            }
            return result;
        }
        case SEM_INFO:
        case IPC_INFO: {
            return ::syscall(SYS_semctl, semid, semnum, semcmd, (u64)semun->_u32);
        }
        case GETALL:
        case SETALL: {
            return ::syscall(SYS_semctl, semid, semnum, semcmd, (u64)semun->_u32);
        }
        case SETVAL: {
            return ::syscall(SYS_semctl, semid, semnum, semcmd, (u64)semun->_i32);
        }
        case IPC_RMID:
        case GETPID:
        case GETNCNT:
        case GETZCNT:
        case GETVAL: {
            return ::syscall(SYS_semctl, semid, semnum, semcmd, nullptr);
        }
        default: {
            ERROR("Unknown SEMCTL operation: %d", semcmd);
            return -ENOSYS;
        }
        }
        break;
    }
    case felix86_SEMTIMEDOP: {
        x86_timespec* guest_timespec = (x86_timespec*)fifth;
        timespec host_timespec = {};
        timespec* host_timespec_ptr = nullptr;
        if (guest_timespec) {
            host_timespec = *guest_timespec;
            host_timespec_ptr = &host_timespec;
        }
        return ::syscall(SYS_semtimedop, first, ptr, second, host_timespec_ptr);
    }
    case felix86_SHMGET: {
        return ::syscall(SYS_shmget, first, second, third);
    }
    case felix86_SHMCTL: {
        u32 shmid = first;
        u32 shmcmd = second;
        u8 cmd = shmcmd & 0xFF;
        bool ipc64 = shmcmd & 0x100;
        x86_shmid_ds_64* ptr64 = (x86_shmid_ds_64*)ptr;
        x86_shmid_ds_32* ptr32 = (x86_shmid_ds_32*)ptr;
        x86_shminfo_64* shminfo64 = (x86_shminfo_64*)ptr;
        x86_shminfo_32* shminfo32 = (x86_shminfo_32*)ptr;
        switch (cmd) {
        case IPC_SET: {
            riscv64_shmid64_ds host_shmid{};
            if (ipc64) {
                host_shmid = *ptr64;
            } else {
                host_shmid = *ptr32;
            }
            return ::syscall(SYS_shmctl, shmid, cmd, &host_shmid);
        }
        case SHM_STAT:
        case SHM_STAT_ANY:
        case IPC_STAT: {
            riscv64_shmid64_ds host_shmid{};
            int result = ::syscall(SYS_shmctl, shmid, cmd, &host_shmid);
            if (result != -1) {
                if (ipc64) {
                    *ptr64 = host_shmid;
                } else {
                    *ptr32 = host_shmid;
                }
            }
            return result;
        }
        case IPC_INFO: {
            struct riscv64_shminfo host_shminfo{};
            int result = ::syscall(SYS_shmctl, shmid, cmd, &host_shminfo);
            if (result != -1) {
                if (ipc64) {
                    *shminfo64 = host_shminfo;
                } else {
                    *shminfo32 = host_shminfo;
                }
            }
            return result;
        }
        case SHM_INFO: {
            struct riscv64_shm_info host_shm_info{};
            int result = ::syscall(SYS_shmctl, shmid, cmd, &host_shm_info);
            if (result != -1) {
                *(x86_shm_info_32*)ptr = host_shm_info;
            }
            return result;
        }
        case SHM_LOCK:
            return ::syscall(SYS_shmctl, shmid, cmd, nullptr);
        case SHM_UNLOCK:
            return ::syscall(SYS_shmctl, shmid, cmd, nullptr);
        case IPC_RMID:
            return ::syscall(SYS_shmctl, shmid, cmd, nullptr);
        default: {
            ERROR("Unknown SHMCTL operation: %d", cmd);
            return 0;
        }
        }
        break;
    }
    case felix86_SHMAT: {
        int shmid = first;
        void* address = ptr;
        int flags = second;
        u32* result_address = (u32*)third;
        return g_mapper->shmat32(shmid, address, flags, result_address);
    }
    case felix86_SHMDT: {
        void* address = ptr;
        return g_mapper->shmdt32(address);
    }
    case felix86_MSGGET: {
        return ::syscall(SYS_msgget, first, second);
    }
    default: {
        ERROR("Unknown IPC operation: %d", operation);
        return 0;
    }
    }
}
