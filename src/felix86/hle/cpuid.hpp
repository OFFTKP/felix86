#pragma once

#include "felix86/common/state.hpp"

struct Cpuid {
    u32 leaf = 0;
    u32 subleaf = 0;
    u32 eax = 0;
    u32 ebx = 0;
    u32 ecx = 0;
    u32 edx = 0;
};

Cpuid felix86_cpuid_impl(u32 leaf, u32 subleaf);
void felix86_cpuid(ThreadState* thread_state);
