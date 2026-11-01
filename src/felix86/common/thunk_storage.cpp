#include <sys/mman.h>
#include "felix86/common/log.hpp"
#include "felix86/common/thunk_storage.hpp"

constexpr static size_t trampoline_storage_size = 1024 * 512;

ThunkStorage::ThunkStorage() {
    riscv_trampoline_storage = (u8*)mmap(nullptr, trampoline_storage_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    x86_trampoline_storage = (u8*)mmap(nullptr, trampoline_storage_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    riscv_trampoline_storage_start = riscv_trampoline_storage;
    x86_trampoline_storage_start = x86_trampoline_storage;
    ASSERT(riscv_trampoline_storage != MAP_FAILED);
    ASSERT(x86_trampoline_storage != MAP_FAILED);
}

u8* ThunkStorage::getRiscvStorageEnd() {
    return riscv_trampoline_storage_start + trampoline_storage_size;
}

u8* ThunkStorage::getX86StorageEnd() {
    return x86_trampoline_storage_start + trampoline_storage_size;
}

u8* ThunkStorage::allocateRiscvStorage(u64 size) {
    ASSERT(riscv_trampoline_storage);
    u8* ret = __atomic_fetch_add(&riscv_trampoline_storage, size, __ATOMIC_RELAXED);
    ASSERT(ret + size <= riscv_trampoline_storage_start + trampoline_storage_size);
    return ret;
}

u8* ThunkStorage::allocateX86Storage(u64 size) {
    ASSERT(x86_trampoline_storage);
    u8* ret = __atomic_fetch_add(&x86_trampoline_storage, size, __ATOMIC_RELAXED);
    ASSERT(ret + size <= x86_trampoline_storage_start + trampoline_storage_size);
    return ret;
}
