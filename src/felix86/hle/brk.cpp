#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/sysinfo.h>
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/types.hpp"
#include "felix86/hle/brk.hpp"
#include "felix86/hle/mmap.hpp"

void BRK::allocate() {
    u64 initial_brk_size = 8 * 1024 * 1024;

    u64 base = g_program_end;
    base &= ~0xFFF;

    u64 base_brk =
        (u64)g_mapper->map((void*)base, initial_brk_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED_NOREPLACE, -1, 0);
    if ((i64)base_brk < 0) {
        // We couldn't allocate it there for whatever reason
        base_brk = (u64)g_mapper->map(nullptr, initial_brk_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
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

u64 BRK::set(u64 new_brk) {
    u64 result;
    if (new_brk == 0) {
        result = g_current_brk;
    } else {
        if (new_brk > g_initial_brk + g_current_brk_size) {
            // Try to allocate some more space
            u64 end_brk = g_initial_brk + g_current_brk_size;
            ASSERT(!(end_brk & 0xFFF)); // assert page aligned
            u64 new_size = g_current_brk_size + 8 * 1024 * 1024;
            u64 size_past_end = new_size - g_current_brk_size;
            int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE;
            void* new_map = g_mapper->map((void*)end_brk, size_past_end, PROT_READ | PROT_WRITE, flags, -1, 0);
            if ((u64)new_map != end_brk) {
                result = g_current_brk;
            } else {
                g_current_brk = new_brk;
                result = new_brk;
                g_current_brk_size = new_size;
            }
        } else {
            g_current_brk = new_brk;
            result = new_brk;
        }
    }

    return result;
}