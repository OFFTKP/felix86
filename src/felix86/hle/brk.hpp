#pragma once

#include "felix86/common/types.hpp"

struct BRK {
    static void allocate(bool mode32);

    static u64 set(bool mode32, u64 new_brk);

private:
    constexpr static u64 size32 = 32 * 1024 * 1024;

    constexpr static u64 size64 = 128 * 1024 * 1024;
};