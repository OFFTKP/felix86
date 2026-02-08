#pragma once

#include "biscuit/assembler.hpp"
#include "felix86/common/types.hpp"

struct Seccomp {
    static bool setFilter(u32 flags, void* args, u64 rip);
    static void emitFilters(biscuit::Assembler& as);
    static bool hasFilters();
};