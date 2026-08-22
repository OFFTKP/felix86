#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/sysinfo.h>
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/types.hpp"
#include "felix86/hle/brk.hpp"
#include "felix86/hle/mmap.hpp"

void BRK::allocate(bool mode32) {
    u64 initial_brk_size = 0x1000;

    u64 base = g_program_end;
    base &= ~0xFFF;

    u64 base_brk =
        (u64)g_mapper->map(mode32, (void*)base, initial_brk_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED_NOREPLACE, -1, 0);
    if ((i64)base_brk < 0) {
        // We couldn't allocate it there for whatever reason
        base_brk = (u64)g_mapper->map(mode32, nullptr, initial_brk_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        ASSERT_MSG((i64)base_brk > 0, "Failed to allocate BRK");
        base = base_brk;
    } else {
        ASSERT((u64)base == base_brk);
    }

    g_current_brk = base;
    ASSERT_MSG((i64)g_current_brk >= 0, "Failed when trying to allocate the current BRK at %p", (void*)base);

    g_initial_brk = g_current_brk;
    g_current_brk_size = initial_brk_size;
    prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, g_initial_brk, initial_brk_size, "felix86-brk");
    VERBOSE("BRK base at %p", (void*)g_current_brk);
}

u64 BRK::set(bool mode32, u64 new_brk) {
    u64 result;
    if (new_brk == 0) {
        result = g_current_brk;
    } else if (new_brk < g_initial_brk) {
        result = g_current_brk;
    } else {
        // Try to allocate some more space
        u64 end_brk = g_initial_brk + g_current_brk_size;
        ASSERT(!(end_brk & 0xFFF)); // assert page aligned
        u64 aligned_brk = (new_brk + 0xFFF) & ~0xFFFull;
        if (aligned_brk < new_brk) { // brk overflows
            return g_current_brk;
        }

        if (aligned_brk > end_brk) {
            u64 size_past_end = aligned_brk - end_brk;
            int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE;
            void* new_map = g_mapper->map(mode32, (void*)end_brk, size_past_end, PROT_READ | PROT_WRITE, flags, -1, 0);
            if ((u64)new_map != end_brk) {
                result = g_current_brk;
            } else {
                g_current_brk = new_brk;
                result = new_brk;
                g_current_brk_size += size_past_end;
            }
        } else if (aligned_brk < end_brk) {
            u64 size_to_free = end_brk - aligned_brk;
            if (g_mapper->unmap(mode32, (void*)aligned_brk, size_to_free) < 0) {
                result = g_current_brk;
            } else {
                g_current_brk = new_brk;
                result = new_brk;
                g_current_brk_size -= size_to_free;
            }
        } else {
            g_current_brk = new_brk;
            result = new_brk;
        }
    }

    return result;
}
