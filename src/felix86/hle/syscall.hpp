#pragma once

#include "felix86/common/log.hpp"

struct ThreadState;

void felix86_syscall(ThreadState* state);

void felix86_syscall32(ThreadState* state);

enum {
#define X(name, id) felix86_x86_32_##name = id,
#include "felix86/hle/syscalls_x86_32.inc"
#undef X
};

enum {
#define X(name, id) felix86_x86_64_##name = id,
#include "felix86/hle/syscalls_x86_64.inc"
#undef X
};

enum {
#define X(name, id) felix86_riscv64_##name = id,
#include "felix86/hle/syscalls_riscv64.inc"
#undef X
};

constexpr static bool is_x64_common(int syscall) {
#define X(name)                                                                                                                                      \
    case felix86_x86_64_##name:                                                                                                                      \
        return true;
    switch (syscall) {
#include "felix86/hle/syscalls_common_64.inc"
#undef X
    default:
        return false;
    }
#undef X
}

constexpr static bool is_x86_common(int syscall) {
#define X(name)                                                                                                                                      \
    case felix86_x86_32_##name:                                                                                                                      \
        return true;
    switch (syscall) {
#include "felix86/hle/syscalls_common_32.inc"
#undef X
    default:
        return false;
    }
#undef X
}

constexpr static int x64_to_riscv(int syscall) {
#define X(name)                                                                                                                                      \
    case felix86_x86_64_##name:                                                                                                                      \
        return felix86_riscv64_##name;
    switch (syscall) {
#include "felix86/hle/syscalls_common_64.inc"
#undef X
    default:
        ASSERT_MSG(false, "%d is not a shared syscall", syscall);
        return 0;
    }
#undef X
}

constexpr static int x86_to_riscv(int syscall) {
#define X(name)                                                                                                                                      \
    case felix86_x86_32_##name:                                                                                                                      \
        return felix86_riscv64_##name;
    switch (syscall) {
#include "felix86/hle/syscalls_common_32.inc"
#undef X
    default:
        ASSERT_MSG(false, "%d is not a shared syscall", syscall);
        return 0;
    }
#undef X
}

constexpr static const char* x64_get_name(int syscall) {
#define X(name, ...)                                                                                                                                 \
    case felix86_x86_64_##name:                                                                                                                      \
        return #name;
    switch (syscall) {
#include "felix86/hle/syscalls_x86_64.inc"
#undef X
    default:
        ASSERT_MSG(false, "%d is not a syscall", syscall);
        return nullptr;
    }
#undef X
}

constexpr static const char* x86_get_name(int syscall) {
#define X(name, ...)                                                                                                                                 \
    case felix86_x86_32_##name:                                                                                                                      \
        return #name;
    switch (syscall) {
#include "felix86/hle/syscalls_x86_32.inc"
#undef X
    default:
        ASSERT_MSG(false, "%d is not a syscall", syscall);
        return nullptr;
    }
#undef X
}

constexpr static const char* riscv_get_name(int syscall) {
#define X(name, ...)                                                                                                                                 \
    case felix86_riscv64_##name:                                                                                                                     \
        return #name;
    switch (syscall) {
#include "felix86/hle/syscalls_riscv64.inc"
#undef X
    default:
        ASSERT_MSG(false, "%d is not a syscall", syscall);
        return nullptr;
    }
#undef X
}

static_assert(x64_to_riscv(felix86_x86_64_setxattr) == felix86_riscv64_setxattr);
