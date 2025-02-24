#pragma once

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

consteval int match_host(int syscall) {
#define X(name)                                                                                                                                      \
    case felix86_x86_64_##name:                                                                                                                      \
        return felix86_riscv64_##name;
    switch (syscall) {
#include "felix86/hle/syscalls_common.inc"
#undef X
    default:
        __builtin_unreachable();
    }
#undef X
}

static_assert(match_host(felix86_x86_64_setxattr) == felix86_riscv64_setxattr);

struct ThreadState;

void felix86_syscall(ThreadState* state);

void felix86_syscall_32(ThreadState* state);