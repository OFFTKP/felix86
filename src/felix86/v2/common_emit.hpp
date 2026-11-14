#pragma once

#include "biscuit/assembler.hpp"
#include "felix86/common/types.hpp"
#include "felix86/v2/scratch_allocator.hpp"

struct CommonEmit {
    static void writebackGuestState(biscuit::Assembler& as, ScratchAllocator& scratches);

    static void restoreGuestState(biscuit::Assembler& as, ScratchAllocator& scratches, bool sse_rounding_mode);

    static void loadRoundingMode(biscuit::Assembler& as, ScratchAllocator& scratches, bool sse);

    static void addressCacheLookup(biscuit::Assembler& as, ScratchAllocator& scratches, biscuit::GPR guest_address,
                                   void on_hit(biscuit::Assembler&, biscuit::GPR), bool mode32, bool use_ra = false);

    static void call(biscuit::Assembler& as, u64 target, bool relocatable);
};
