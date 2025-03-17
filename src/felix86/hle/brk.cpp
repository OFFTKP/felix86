#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/sysinfo.h>
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/utility.hpp"
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
    u64 max_brk_size = g_brk_max_size;
    u64 initial_brk_size = BRK::size32;
    if (max_brk_size == 0) {
        max_brk_size = 256 * 1024 * 1024;
    }

    // Make our initial brk size always be <= max, if the user specified their own max
    if (max_brk_size < initial_brk_size) {
        initial_brk_size = max_brk_size;
    }

    VERBOSE("Max BRK size: %lx", max_brk_size);
    VERBOSE("Initial BRK size: %lx", initial_brk_size);

    // Allocate the max brk size with MAP_NORESERVE, and the actual brk normally, so we can expand as we go and the memory
    // doesn't get stolen by something else
    u64 base = g_brk_base_hint ? g_brk_base_hint : g_program_end;
    base &= ~0xFFF;

    // Some way to allocate and check we aren't ruining pages
    UNREACHABLE();
}

void BRK::allocate64() {
    u64 max_brk_size = g_brk_max_size;
    u64 initial_brk_size = BRK::size64;
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
    u64 base = g_brk_base_hint ? g_brk_base_hint : g_program_end;
    base &= ~0xFFF;

    u8* brk_base = nullptr;
    int attempts = 30;
    int flags = MAP_PRIVATE | MAP_NORESERVE | MAP_ANONYMOUS;
    int prot = PROT_NONE;
    while (true) {
        brk_base = (u8*)felix86_mmap((void*)base, max_brk_size, prot, flags | MAP_FIXED_NOREPLACE, -1, 0);
        if (brk_base != MAP_FAILED) {
            break;
        }

        // Try a different page
        brk_base += max_brk_size;
        attempts--;
        if (attempts == 0) {
            WARN("Ran out of attempts while trying to allocate BRK");
            brk_base = (u8*)felix86_mmap(nullptr, max_brk_size, prot, flags, -1, 0);
            ASSERT_MSG(brk_base != MAP_FAILED, "Could not allocate BRK base, try setting it to a lower amount with FELIX86_BRK_SIZE");
            break;
        }
    }

    g_current_brk = (u64)felix86_mmap(brk_base, initial_brk_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

    if ((void*)g_current_brk == MAP_FAILED) {
        ERROR("Failed to allocate memory for initial brk");
    }

    g_initial_brk = g_current_brk;
    g_current_brk_size = initial_brk_size;
    prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, g_current_brk, initial_brk_size, "current-brk");
    prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, g_current_brk, max_brk_size, "max-brk");
    VERBOSE("BRK base at %p", (void*)g_current_brk);
}