#pragma once

#include "felix86/common/types.hpp"

struct ThunkStorage {
    ThunkStorage();

    u8* allocateRiscvStorage(u64 size);

    u8* allocateX86Storage(u64 size);

    u8* getRiscvStorageStart() {
        return riscv_trampoline_storage_start;
    }

    u8* getX86StorageStart() {
        return x86_trampoline_storage_start;
    }

    u8* getRiscvStorageEnd();

    u8* getX86StorageEnd();

private:
    u8* riscv_trampoline_storage{};
    u8* riscv_trampoline_storage_start{};
    u8* x86_trampoline_storage{};
    u8* x86_trampoline_storage_start{};
};
