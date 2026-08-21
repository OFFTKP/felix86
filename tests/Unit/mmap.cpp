// clang-format off
// Test our mmap implementation
#include <catch2/catch_test_macros.hpp>
#include <sys/mman.h>
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/types.hpp"
#include "felix86/common/utility.hpp"
#include "felix86/hle/mmap.hpp"

bool g_mode32 = false;

namespace Catch {
    template <>
    struct StringMaker<std::pair<uint32_t, uint32_t>> {
        static std::string convert(const std::pair<uint32_t, uint32_t>& range) {
            std::ostringstream oss;
            oss << "0x" << std::hex << std::setw(16) << std::setfill('0') << range.first
                << " - 0x" << std::hex << std::setw(16) << std::setfill('0') << range.second;
            return oss.str();
        }
    };
}

#define SUCCESS_MESSAGE() SUCCESS("Test passed: %s", Catch::getResultCapture().getCurrentTestName().c_str())

static const int FCOMMON = MAP_ANONYMOUS | MAP_PRIVATE;
static const int FCOMMON_FIXED = FCOMMON | MAP_FIXED;

#define MMAP_AT(addr, size, prot, flags) \
    do { \
        void* address = mapper.map(g_mode32, (void*)(addr), size, prot, FCOMMON_FIXED | flags, -1, 0); \
        CATCH_REQUIRE(address == (void*)(u64)(addr)); \
        unmap_me.push_back({(u64)(addr), size}); \
    } while (0)

#define MREMAP_AT(old_addr, old_size, new_addr, new_size, flags) \
    do {\
        void* address = mapper.remap(g_mode32, (void*)old_addr, old_size, new_size, flags, (void*)new_addr); \
        CATCH_REQUIRE(address == (void*)(u64)(new_addr)); \
        unmap_me.push_back({(u64)(new_addr), new_size}); \
    } while(0)

#define MREMAP(old_addr, old_size, new_size, flags) \
    do {\
        void* address = mapper.remap(g_mode32, (void*)old_addr, old_size, new_size, flags, 0); \
        CATCH_REQUIRE(address != (void*)-1); \
        unmap_me.push_back({(u64)address, new_size}); \
    } while(0)


#define MMAP_AT_R(address, size, prot, flags) \
    do { \
        address = mapper.map(g_mode32, (void*)(0), size, prot, FCOMMON | flags, -1, 0); \
        unmap_me.push_back({(u64)(address), size}); \
    } while (0)

// Doesn't need to erase from unmap_me
#define UNMAP_AT(addr, size) \
    do { \
        int result = mapper.unmap(g_mode32, (void*)(addr), size); \
        CATCH_REQUIRE(result == 0); \
    } while(0)

#define MUNMAP_ALL() \
    do { \
        for (size_t i = 0; i < unmap_me.size(); i++) { \
            auto [addr, size] = unmap_me[i]; \
            munmap((void*)(u64)addr, size); \
        } \
    } while (0)

struct GuestRegionExpected {
    u64 start;
    u64 len;
    int prot;
    int flags;
};

static void verifyGuestRegions(Mapper& mapper, const std::vector<GuestRegionExpected>& expected_regions) {
    auto guest_regions = mapper.get_guest_regions();
    int found_regions = 0;
    for (const auto region : expected_regions) {
        for (const auto guest_region : guest_regions) {
            bool yes = true;
            yes &= region.start == guest_region.start;
            yes &= region.start + region.len == guest_region.end;
            // Include the implicit flags here
            yes &= region.flags == guest_region.flags;
            yes &= region.prot == guest_region.prot;
            yes &= !guest_region.shmem;
            if (yes) {
                found_regions += 1;
                break;
            };
        }
    }
    CATCH_REQUIRE(found_regions == expected_regions.size());    
    CATCH_REQUIRE(guest_regions.size() == expected_regions.size());
}

void verifyRegions(Mapper& mapper, const std::vector<std::pair<u32, u32>>& expected_regions) {
    auto actual_regions = mapper.getRegions();
    CATCH_REQUIRE(expected_regions == actual_regions);
}

CATCH_TEST_CASE("Simple1", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x20000, 0x10000, PROT_NONE, 0);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x20000 - 1},
        {0x20000 + 0x10000, (u64)UINT32_MAX},
    });

    verifyGuestRegions(mapper, { 
        {0x20000, 0x10000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Simple2", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x20000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x30000, 0x10000, PROT_NONE, 0);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x20000 - 1},
        {0x20000 + 0x10000 + 0x10000, (u64)UINT32_MAX},
    });

    verifyGuestRegions(mapper, { 
        {0x20000, 0x20000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Simple3", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x20000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x30000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x40000, 0x10000, PROT_NONE, 0);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x20000 - 1},
        {0x20000 + 0x10000 + 0x10000 + 0x10000, (u64)UINT32_MAX},
    });

    verifyGuestRegions(mapper, { 
        {0x20000, 0x30000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("FirstPages", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(mmap_min_addr(), 0x10000, PROT_NONE, 0);

    verifyRegions(mapper, {
        {mmap_min_addr() + 0x10000, (u64)UINT32_MAX},
    });

    verifyGuestRegions(mapper, { 
        {mmap_min_addr(), 0x10000, PROT_NONE, FCOMMON_FIXED}
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("FirstPagesUnmap", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(mmap_min_addr(), 0x10000, PROT_NONE, 0);

    verifyRegions(mapper, {
        {mmap_min_addr() + 0x10000, (u64)UINT32_MAX},
    });

    verifyGuestRegions(mapper, { 
        {mmap_min_addr(), 0x10000, PROT_NONE, FCOMMON_FIXED}
    });

    UNMAP_AT(mmap_min_addr(), 0x10000);

    verifyRegions(mapper, {
        {mmap_min_addr(), (u64)UINT32_MAX},
    });

    verifyGuestRegions(mapper, {});

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("LastPages", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    u64 end = (u64)UINT32_MAX + 1;

    MMAP_AT(end - 0x10000, 0x10000, PROT_NONE, 0);

    verifyRegions(mapper, {
        {mmap_min_addr(), end - 0x10000 - 1},
    });

    verifyGuestRegions(mapper, { 
        {end - 0x10000, 0x10000, PROT_NONE, FCOMMON_FIXED}
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Split2", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x30000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x50000, 0x10000, PROT_NONE, 0);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x30000 - 1},
        {0x30000 + 0x10000, 0x50000 - 1},
        {0x50000 + 0x10000, (u64)UINT32_MAX},
    });

    verifyGuestRegions(mapper, { 
        {0x30000, 0x10000, PROT_NONE, FCOMMON_FIXED},
        {0x50000, 0x10000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Split2Pick1", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x30000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x50000, 0x10000, PROT_NONE, 0);

    // Mmap exactly in the middle of the two previous ones
    MMAP_AT(0x40000, 0x10000, PROT_NONE, 0);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x30000 - 1},
        {0x50000 + 0x10000, (u64)UINT32_MAX},
    });

    verifyGuestRegions(mapper, { 
        {0x30000, 0x30000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Overlapping1", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x30000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x20000, 0x100000, PROT_NONE, 0); // this mapping consumes the first

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x20000 - 1},
        {0x20000 + 0x100000, (u64)UINT32_MAX},
    });

    verifyGuestRegions(mapper, { 
        {0x20000, 0x100000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Overlapping2", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x30000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x50000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x20000, 0x100000, PROT_NONE, 0); // this mapping consumes the other two

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x20000 - 1},
        {0x20000 + 0x100000, (u64)UINT32_MAX},
    });

    verifyGuestRegions(mapper, { 
        {0x20000, 0x100000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Overlapping2ConsumeLast", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x30000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x50000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x100000, 0x1000, PROT_NONE, 0);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x30000 - 1},
        {0x40000, 0x50000 - 1},
        {0x60000, 0x100000 - 1}, // this test ensures this block is properly deleted
        {0x101000, (u64)UINT32_MAX},
    });

    verifyGuestRegions(mapper, { 
        {0x30000, 0x10000, PROT_NONE, FCOMMON_FIXED},
        {0x50000, 0x10000, PROT_NONE, FCOMMON_FIXED},
        {0x100000, 0x1000, PROT_NONE, FCOMMON_FIXED},
    });

    MMAP_AT(0x20000, 0x100000 - 0x20000, PROT_NONE, 0); // this mapping consumes the first two

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x20000 - 1},
        {0x101000, (u64)UINT32_MAX},
    });

    verifyGuestRegions(mapper, { 
        {0x20000, 0x101000 - 0x20000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("UnmapPerfect", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x30000, 0x10000, PROT_NONE, 0);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x30000 - 1},
        {0x40000, (u64)UINT32_MAX},
    });

    verifyGuestRegions(mapper, { 
        {0x30000, 0x10000, PROT_NONE, FCOMMON_FIXED},
    });

    UNMAP_AT(0x30000, 0x10000);

    verifyRegions(mapper, {
        {mmap_min_addr(), (u64)UINT32_MAX},
    });

    verifyGuestRegions(mapper, {});

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("UnmapMiddle", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x20000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x30000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x40000, 0x10000, PROT_NONE, 0);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x20000 - 1},
        {0x50000, (u64)UINT32_MAX},
    });

    verifyGuestRegions(mapper, { 
        {0x20000, 0x30000, PROT_NONE, FCOMMON_FIXED},
    });

    UNMAP_AT(0x30000, 0x10000);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x20000 - 1},
        {0x30000, 0x40000 - 1},
        {0x50000, (u64)UINT32_MAX},
    });

    verifyGuestRegions(mapper, { 
        {0x20000, 0x10000, PROT_NONE, FCOMMON_FIXED},
        {0x40000, 0x10000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("UnmapGreedyMin", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x20000, 0x100000, PROT_NONE, 0);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x20000 - 1},
        {0x120000, (u64)UINT32_MAX},
    });

    verifyGuestRegions(mapper, { 
        {0x20000, 0x100000, PROT_NONE, FCOMMON_FIXED},
    });

    // Unmap more pages than we mapped, this is allowed
    UNMAP_AT(0x1F000, 0x10000);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x2F000 - 1},
        {0x120000, (u64)UINT32_MAX},
    });

    verifyGuestRegions(mapper, { 
        {0x2F000, 0x120000 - 0x2F000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("UnmapGreedyMax", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x20000, 0x100000, PROT_NONE, 0);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x20000 - 1},
        {0x120000, (u64)UINT32_MAX},
    });

    verifyGuestRegions(mapper, { 
        {0x20000, 0x100000, PROT_NONE, FCOMMON_FIXED},
    });

    // Unmap more pages than we mapped, this is allowed
    UNMAP_AT(0x115000, 0x10000);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x20000 - 1},
        {0x115000, (u64)UINT32_MAX},
    });

    verifyGuestRegions(mapper, { 
        {0x20000, 0x115000 - 0x20000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MapRandom", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    void* address = 0;
    MMAP_AT_R(address, 0x100000, PROT_NONE, 0);

    // Random mmaps always pick from first page if possible
    verifyRegions(mapper, {
        {mmap_min_addr() + 0x100000, (u64)UINT32_MAX},
    });

    verifyGuestRegions(mapper, { 
        {(u64)address, 0x100000, PROT_NONE, FCOMMON},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("OverwriteFixed", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x10000, 0x200c, PROT_NONE, 0);
    MMAP_AT(0x13000, 0x34a18, PROT_NONE, 0);
    MMAP_AT(0x13000, 0x60000, PROT_NONE, 0);

    verifyRegions(mapper, {
        {mmap_min_addr() + 0x63000, (u64)UINT32_MAX},
    });

    verifyGuestRegions(mapper, { 
        {0x10000, 0x63000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("OverwriteFixed2", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x13000, 0x34a18, PROT_NONE, 0);
    MMAP_AT(0x12000, 0x60000, PROT_NONE, 0);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x11fff},
        {mmap_min_addr() + 0x62000, (u64)UINT32_MAX},
    });

    verifyGuestRegions(mapper, { 
        {0x12000, 0x60000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Mremap", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x13000, 0x10000, PROT_NONE, 0);
    MREMAP_AT(0x13000, 0x10000, 0x40000, 0x20000, MREMAP_FIXED | MREMAP_MAYMOVE);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x3ffff},
        {0x60000, (u64)UINT32_MAX},
    });

    verifyGuestRegions(mapper, { 
        {0x40000, 0x20000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MMap bug", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x85800000, 0x8d400000-0x85800000, PROT_NONE, 0);
    MMAP_AT(0xFFF00000, 0xFFFFFFFF-0xFFF00000, PROT_NONE, 0);

    void* address = mapper.map(g_mode32, (void*)0xfff00000, 0x7d000, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);
    ASSERT(address == (void*)0xfff00000);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x00000000857fffff},
        {0x000000008d400000, (u64)0x00000000ffefffff},
    });

    verifyGuestRegions(mapper, { 
        {0x85800000, 0x8d400000-0x85800000, PROT_NONE, FCOMMON_FIXED},
        {0xFFF00000, 0xFFF7d000-0xFFF00000, PROT_READ | PROT_WRITE, FCOMMON_FIXED},
        {0xFFF7d000, 0x100000000-0xFFF7d000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Simple1", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x20000, 0x10000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x20000, 0x10000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Simple2", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x20000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x30000, 0x10000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x20000, 0x20000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Simple3", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x20000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x30000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x40000, 0x10000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x20000, 0x30000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("FirstPages", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;

    MMAP_AT(mmap_min_addr(), 0x10000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {mmap_min_addr(), 0x10000, PROT_NONE, FCOMMON_FIXED}
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("FirstPagesUnmap", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(mmap_min_addr(), 0x10000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {mmap_min_addr(), 0x10000, PROT_NONE, FCOMMON_FIXED}
    });

    UNMAP_AT(mmap_min_addr(), 0x10000);

    verifyGuestRegions(mapper, {});

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("LastPages", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    u64 end = (u64)UINT32_MAX + 1;

    MMAP_AT(end - 0x10000, 0x10000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {end - 0x10000, 0x10000, PROT_NONE, FCOMMON_FIXED}
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Split2", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x30000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x50000, 0x10000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x30000, 0x10000, PROT_NONE, FCOMMON_FIXED},
        {0x50000, 0x10000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Split2Pick1", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x30000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x50000, 0x10000, PROT_NONE, 0);

    // Mmap exactly in the middle of the two previous ones
    MMAP_AT(0x40000, 0x10000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x30000, 0x30000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Overlapping1", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x30000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x20000, 0x100000, PROT_NONE, 0); // this mapping consumes the first

    verifyGuestRegions(mapper, { 
        {0x20000, 0x100000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Overlapping2", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x30000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x50000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x20000, 0x100000, PROT_NONE, 0); // this mapping consumes the other two

    verifyGuestRegions(mapper, { 
        {0x20000, 0x100000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Overlapping2ConsumeLast", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x30000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x50000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x100000, 0x1000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x30000, 0x10000, PROT_NONE, FCOMMON_FIXED},
        {0x50000, 0x10000, PROT_NONE, FCOMMON_FIXED},
        {0x100000, 0x1000, PROT_NONE, FCOMMON_FIXED},
    });

    MMAP_AT(0x20000, 0x100000 - 0x20000, PROT_NONE, 0); // this mapping consumes the first two

    verifyGuestRegions(mapper, { 
        {0x20000, 0x101000 - 0x20000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("UnmapPerfect", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x30000, 0x10000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x30000, 0x10000, PROT_NONE, FCOMMON_FIXED},
    });

    UNMAP_AT(0x30000, 0x10000);

    verifyGuestRegions(mapper, {});

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("UnmapMiddle", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x20000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x30000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x40000, 0x10000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x20000, 0x30000, PROT_NONE, FCOMMON_FIXED},
    });

    UNMAP_AT(0x30000, 0x10000);

    verifyGuestRegions(mapper, { 
        {0x20000, 0x10000, PROT_NONE, FCOMMON_FIXED},
        {0x40000, 0x10000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("UnmapGreedyMin", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x20000, 0x100000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x20000, 0x100000, PROT_NONE, FCOMMON_FIXED},
    });

    // Unmap more pages than we mapped, this is allowed
    UNMAP_AT(0x1F000, 0x10000);

    verifyGuestRegions(mapper, { 
        {0x2F000, 0x100000 - 0x0F000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("UnmapGreedyMax", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x20000, 0x100000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x20000, 0x100000, PROT_NONE, FCOMMON_FIXED},
    });

    // Unmap more pages than we mapped, this is allowed
    UNMAP_AT(0x115000, 0x10000);

    verifyGuestRegions(mapper, { 
        {0x20000, 0x115000 - 0x20000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MapRandom", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    void* address = 0;
    MMAP_AT_R(address, 0x100000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {(u64)address, 0x100000, PROT_NONE, FCOMMON},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("OverwriteFixed", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x10000, 0x200c, PROT_NONE, 0);
    MMAP_AT(0x13000, 0x34a18, PROT_NONE, 0);
    MMAP_AT(0x13000, 0x60000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x10000, 0x63000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("OverwriteFixed2", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x13000, 0x34a18, PROT_NONE, 0);
    MMAP_AT(0x12000, 0x60000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x12000, 0x60000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MremapMove", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x13000, 0x10000, PROT_NONE, 0);
    MREMAP_AT(0x13000, 0x10000, 0x40000, 0x10000, MREMAP_FIXED | MREMAP_MAYMOVE);

    verifyGuestRegions(mapper, { 
        {0x40000, 0x10000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MremapMoveKeepsFlagsAndProt", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x13000, 0x10000, PROT_READ, MAP_DENYWRITE);
    MREMAP_AT(0x13000, 0x10000, 0x40000, 0x20000, MREMAP_FIXED | MREMAP_MAYMOVE);

    verifyGuestRegions(mapper, { 
        {0x40000, 0x20000, PROT_READ, FCOMMON_FIXED | MAP_DENYWRITE},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MremapMoveDoNotUnmap", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x13000, 0x10000, PROT_NONE, 0);
    MREMAP_AT(0x13000, 0x10000, 0x40000, 0x10000, MREMAP_FIXED | MREMAP_MAYMOVE | MREMAP_DONTUNMAP);

    verifyGuestRegions(mapper, { 
        {0x40000, 0x10000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MremapGrow", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x10000, 0x10000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x10000, 0x10000, PROT_NONE, FCOMMON_FIXED},
    });

    MREMAP(0x10000, 0x10000, 0x20000, 0);

    verifyGuestRegions(mapper, { 
        {0x10000, 0x20000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MremapMoveGrow", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x10000, 0x10000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x10000, 0x10000, PROT_NONE, FCOMMON_FIXED},
    });

    MREMAP_AT(0x10000, 0x10000, 0x40000, 0x20000, MREMAP_FIXED | MREMAP_MAYMOVE);

    verifyGuestRegions(mapper, { 
        {0x40000, 0x20000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MremapShrink", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x10000, 0x20000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x10000, 0x20000, PROT_NONE, FCOMMON_FIXED},
    });

    MREMAP(0x10000, 0x20000, 0x10000, 0);

    verifyGuestRegions(mapper, { 
        {0x10000, 0x10000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MremapMoveShrink", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x10000, 0x20000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x10000, 0x20000, PROT_NONE, FCOMMON_FIXED},
    });

    MREMAP_AT(0x10000, 0x20000, 0x40000, 0x10000, MREMAP_FIXED | MREMAP_MAYMOVE);

    verifyGuestRegions(mapper, { 
        {0x40000, 0x10000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MremapMoveMiddle", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x10000, 0x30000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x10000, 0x30000, PROT_NONE, FCOMMON_FIXED},
    });

    MREMAP_AT(0x20000, 0x10000, 0x60000, 0x10000, MREMAP_FIXED | MREMAP_MAYMOVE);

    verifyGuestRegions(mapper, { 
        {0x10000, 0x10000, PROT_NONE, FCOMMON_FIXED},
        {0x30000, 0x10000, PROT_NONE, FCOMMON_FIXED},
        {0x60000, 0x10000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MremapMoveShrinkMiddle", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x10000, 0x40000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x10000, 0x40000, PROT_NONE, FCOMMON_FIXED},
    });

    MREMAP_AT(0x20000, 0x20000, 0x60000, 0x10000, MREMAP_FIXED | MREMAP_MAYMOVE);

    verifyGuestRegions(mapper, { 
        {0x10000, 0x10000, PROT_NONE, FCOMMON_FIXED},
        {0x40000, 0x10000, PROT_NONE, FCOMMON_FIXED},
        {0x60000, 0x10000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MremapMoveGrowMiddle", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x10000, 0x30000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x10000, 0x30000, PROT_NONE, FCOMMON_FIXED},
    });

    MREMAP_AT(0x20000, 0x10000, 0x60000, 0x20000, MREMAP_FIXED | MREMAP_MAYMOVE);

    verifyGuestRegions(mapper, { 
        {0x10000, 0x10000, PROT_NONE, FCOMMON_FIXED},
        {0x30000, 0x10000, PROT_NONE, FCOMMON_FIXED},
        {0x60000, 0x20000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MremapDeleteMiddle", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x10000, 0x30000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x10000, 0x30000, PROT_NONE, FCOMMON_FIXED},
    });

    MREMAP(0x20000, 0x10000, 0x1000, 0);

    verifyGuestRegions(mapper, { 
        {0x10000, 0x11000, PROT_NONE, FCOMMON_FIXED},
        {0x30000, 0x10000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MremapOverwriteDelete", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x10000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x20000, 0x10000, PROT_WRITE, 0);
    MMAP_AT(0x30000, 0x10000, PROT_READ, 0);

    verifyGuestRegions(mapper, { 
        {0x10000, 0x10000, PROT_NONE, FCOMMON_FIXED},
        {0x20000, 0x10000, PROT_WRITE, FCOMMON_FIXED},
        {0x30000, 0x10000, PROT_READ, FCOMMON_FIXED},
    });

    MREMAP(0x10000, 0x30000, 0x1000, 0);

    verifyGuestRegions(mapper, {
        {0x10000, 0x1000, PROT_NONE, FCOMMON_FIXED},     
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("DifferentNeighborProt", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x10000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x20000, 0x10000, PROT_WRITE, 0);
    MMAP_AT(0x30000, 0x10000, PROT_READ, 0);
    MMAP_AT(0x40000, 0x10000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x10000, 0x10000, PROT_NONE, FCOMMON_FIXED},
        {0x20000, 0x10000, PROT_WRITE, FCOMMON_FIXED},
        {0x30000, 0x10000, PROT_READ, FCOMMON_FIXED},
        {0x40000, 0x10000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("DifferentNeighborFlags", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x10000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x20000, 0x10000, PROT_NONE, MAP_DENYWRITE);
    MMAP_AT(0x30000, 0x10000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x10000, 0x10000, PROT_NONE, FCOMMON_FIXED},
        {0x20000, 0x10000, PROT_NONE, FCOMMON_FIXED | MAP_DENYWRITE},
        {0x30000, 0x10000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MergeChain", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x10000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x30000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x50000, 0x10000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x10000, 0x10000, PROT_NONE, FCOMMON_FIXED},
        {0x30000, 0x10000, PROT_NONE, FCOMMON_FIXED},
        {0x50000, 0x10000, PROT_NONE, FCOMMON_FIXED},
    });

    MMAP_AT(0x20000, 0x30000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x10000, 0x50000, PROT_NONE, FCOMMON_FIXED},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}
