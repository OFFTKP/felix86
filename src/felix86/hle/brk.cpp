#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/sysinfo.h>
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/types.hpp"
#include "felix86/hle/brk.hpp"
#include "felix86/hle/mmap.hpp"

void BRK::allocate() {
    if (g_mode32) {
        return allocate32();
    } else {
        return allocate64();
    }
}

void BRK::allocate32() {
    u64 max_brk_size = g_config.max_brk_size;
    u64 initial_brk_size = g_config.init_brk_size ? g_config.init_brk_size : BRK::size32;
    if (max_brk_size == 0) {
        max_brk_size = 1 * 1024 * 1024 * 1024;
    }

    // Make our initial brk size always be <= max, if the user specified their own max
    if (max_brk_size < initial_brk_size) {
        initial_brk_size = max_brk_size;
    }

    VERBOSE("Max BRK size: %lx", max_brk_size);
    VERBOSE("Initial BRK size: %lx", initial_brk_size);

    // Allocate the max brk size with MAP_NORESERVE, and the actual brk normally, so we can expand as we go and the memory
    // doesn't get stolen by something else
    u64 base = g_config.brk_base ? g_config.brk_base : g_program_end;
    base &= ~0xFFF;

    ASSERT_MSG(base <= UINT32_MAX, "BRK hint is outside 32-bit address space for 32-bit application");

    u64 max_brk = (u64)g_mapper->map((void*)base, max_brk_size, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED_NOREPLACE, -1, 0);
    if ((i64)max_brk < 0) {
        // We couldn't allocate it there for whatever reason
        max_brk = (u64)g_mapper->map(nullptr, max_brk_size, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        ASSERT_MSG((i64)max_brk > 0, "Failed to allocate BRK");
        WARN("Failed to allocate BRK at %p, chose %p instead", (void*)base, (void*)max_brk);
        base = max_brk;
    } else {
        ASSERT((u64)base == max_brk);
    }

    g_current_brk = (u64)g_mapper->map((void*)base, initial_brk_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);
    ASSERT_MSG((i64)g_current_brk >= 0, "Failed when trying to allocate the current BRK at %p", (void*)base);

    g_initial_brk = g_current_brk;
    g_current_brk_size = initial_brk_size;
    prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, g_initial_brk, max_brk_size, "felix86-brk");
    VERBOSE("BRK base at %p", (void*)g_current_brk);
    g_max_brk_size = max_brk_size;
}

void BRK::allocate64() {
    u64 max_brk_size = g_config.max_brk_size;
    u64 initial_brk_size = g_config.init_brk_size ? g_config.init_brk_size : BRK::size64;
    if (max_brk_size == 0) {
        // Try to get max ram size from sysinfo and use that
        struct sysinfo info;
        int res = sysinfo(&info);
        if (res == 0) {
            max_brk_size = info.totalram >> 1;
        }
    }

    if (max_brk_size == 0) {
        // Somehow still 0, set to 1GiB
        max_brk_size = 1ull * 1024 * 1024 * 1024;
    }

    // Make our initial brk size always be <= max, if the user specified their own max
    if (max_brk_size < initial_brk_size) {
        initial_brk_size = max_brk_size;
    }

    VERBOSE("Max BRK size: %lx", max_brk_size);
    VERBOSE("Initial BRK size: %lx", initial_brk_size);

    // Allocate the max brk size with MAP_NORESERVE, and the actual brk normally, so we can expand as we go and the memory
    // doesn't get stolen by something else
    u64 base = g_config.brk_base ? g_config.brk_base : g_program_end;
    base &= ~0xFFF;

    u8* brk_base = nullptr;
    int attempts = 30;
    int flags = MAP_PRIVATE | MAP_NORESERVE | MAP_ANONYMOUS;
    int prot = PROT_NONE;
    while (true) {
        brk_base = (u8*)g_mapper->map((void*)base, max_brk_size, prot, flags | MAP_FIXED_NOREPLACE, -1, 0);
        if (brk_base != MAP_FAILED) {
            break;
        }

        // Try a different page
        brk_base += max_brk_size;
        attempts--;
        if (attempts == 0) {
            brk_base = (u8*)g_mapper->map(nullptr, max_brk_size, prot, flags, -1, 0);
            ASSERT_MSG(brk_base != MAP_FAILED, "Could not allocate BRK base, try setting it to a lower amount with FELIX86_BRK_SIZE");
            break;
        }
    }

    g_current_brk = (u64)g_mapper->map(brk_base, initial_brk_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    ASSERT_MSG((void*)g_current_brk != MAP_FAILED, "Failed when trying to allocate the current BRK at %p", (void*)brk_base);

    g_initial_brk = g_current_brk;
    g_current_brk_size = initial_brk_size;
    prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, g_initial_brk, max_brk_size, "felix86-brk");
    VERBOSE("BRK base at %p", (void*)g_current_brk);
    g_max_brk_size = max_brk_size;
}

u64 BRK::set(u64 new_brk) {
    u64 result;
    if (new_brk == 0) {
        result = g_current_brk;
    } else {
        g_current_brk = new_brk;
        result = new_brk;
    }

    if (g_current_brk > g_initial_brk + g_current_brk_size) {
        // Allocate more of our NORESERVE space as actual allocated pages
        u64 end_brk = g_initial_brk + g_current_brk_size;
        ASSERT(!(end_brk & 0xFFF)); // assert page aligned
        u64 new_size = (g_current_brk - g_initial_brk) * 2;
        if (g_current_brk_size < g_max_brk_size && new_size > g_max_brk_size) {
            // We would go over the max limit, set it to max instead
            new_size = g_max_brk_size;
        } else if (new_size > g_max_brk_size) {
            WARN("Trying to allocate more than the maximum BRK size, get ready for a crash!");
        }

        u64 size_past_end = new_size - g_current_brk_size;
        int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED;
        void* new_map = g_mapper->map((void*)end_brk, size_past_end, PROT_READ | PROT_WRITE, flags, -1, 0);
        if ((u64)new_map != end_brk) {
            ERROR("Failed to remap brk with new size: %lx", new_size);
        }

        WARN("Resized BRK (new size: %lx, from %lx-%lx to %lx-%lx)", new_size, g_initial_brk, end_brk, g_initial_brk, end_brk + size_past_end);
        g_current_brk_size = new_size;
    }

    return result;
}