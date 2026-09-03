// clang-format off
// Test our mmap implementation
#include <cstdio>
#include <cstdlib>
#include <catch2/catch_test_macros.hpp>
#include <unistd.h>
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
        CATCH_REQUIRE(address == (void*)old_addr); \
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
        {0x20000, 0x10000, PROT_NONE},
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
        {0x20000, 0x20000, PROT_NONE},
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
        {0x20000, 0x30000, PROT_NONE},
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
        {mmap_min_addr(), 0x10000, PROT_NONE}
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
        {mmap_min_addr(), 0x10000, PROT_NONE}
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
        {end - 0x10000, 0x10000, PROT_NONE}
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
        {0x30000, 0x10000, PROT_NONE},
        {0x50000, 0x10000, PROT_NONE},
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
        {0x30000, 0x30000, PROT_NONE},
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
        {0x20000, 0x100000, PROT_NONE},
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
        {0x20000, 0x100000, PROT_NONE},
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
        {0x30000, 0x10000, PROT_NONE},
        {0x50000, 0x10000, PROT_NONE},
        {0x100000, 0x1000, PROT_NONE},
    });

    MMAP_AT(0x20000, 0x100000 - 0x20000, PROT_NONE, 0); // this mapping consumes the first two

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x20000 - 1},
        {0x101000, (u64)UINT32_MAX},
    });

    verifyGuestRegions(mapper, { 
        {0x20000, 0x101000 - 0x20000, PROT_NONE},
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
        {0x30000, 0x10000, PROT_NONE},
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
        {0x20000, 0x30000, PROT_NONE},
    });

    UNMAP_AT(0x30000, 0x10000);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x20000 - 1},
        {0x30000, 0x40000 - 1},
        {0x50000, (u64)UINT32_MAX},
    });

    verifyGuestRegions(mapper, { 
        {0x20000, 0x10000, PROT_NONE},
        {0x40000, 0x10000, PROT_NONE},
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
        {0x20000, 0x100000, PROT_NONE},
    });

    // Unmap more pages than we mapped, this is allowed
    UNMAP_AT(0x1F000, 0x10000);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x2F000 - 1},
        {0x120000, (u64)UINT32_MAX},
    });

    verifyGuestRegions(mapper, { 
        {0x2F000, 0x120000 - 0x2F000, PROT_NONE},
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
        {0x20000, 0x100000, PROT_NONE},
    });

    // Unmap more pages than we mapped, this is allowed
    UNMAP_AT(0x115000, 0x10000);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x20000 - 1},
        {0x115000, (u64)UINT32_MAX},
    });

    verifyGuestRegions(mapper, { 
        {0x20000, 0x115000 - 0x20000, PROT_NONE},
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
        {(u64)address, 0x100000, PROT_NONE},
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
        {0x10000, 0x63000, PROT_NONE},
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
        {0x12000, 0x60000, PROT_NONE},
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
        {0x40000, 0x20000, PROT_NONE},
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
        {0x85800000, 0x8d400000-0x85800000, PROT_NONE},
        {0xFFF00000, 0xFFF7d000-0xFFF00000, PROT_READ | PROT_WRITE},
        {0xFFF7d000, 0x100000000-0xFFF7d000, PROT_NONE},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MremapFixedOntoExistingMapping", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x13000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x40000, 0x10000, PROT_NONE, 0);
    MREMAP_AT(0x13000, 0x10000, 0x40000, 0x10000, MREMAP_FIXED | MREMAP_MAYMOVE);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x3ffff},
        {0x50000, (u64)UINT32_MAX},
    });

    verifyGuestRegions(mapper, {
        {0x40000, 0x10000, PROT_NONE},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MremapMayMoveInPlaceIn32BitSpace", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x40000, 0x2000, PROT_NONE, 0);

    void* moved = mapper.remap(false, (void*)0x40000, 0x2000, 0x2000, MREMAP_MAYMOVE, nullptr);
    CATCH_REQUIRE(moved == (void*)0x40000);

    verifyGuestRegions(mapper, {
        {0x40000, 0x2000, PROT_NONE},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MremapOutOf32BitSpaceFreesTheFreelist", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    void* high = mmap(nullptr, 0x2000, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    CATCH_REQUIRE(high != MAP_FAILED);
    CATCH_REQUIRE((u64)high > UINT32_MAX);
    CATCH_REQUIRE(munmap(high, 0x2000) == 0);

    MMAP_AT(0x40000, 0x2000, PROT_NONE, 0);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x3ffff},
        {0x42000, (u64)UINT32_MAX},
    });

    void* moved = mapper.remap(false, (void*)0x40000, 0x2000, 0x2000, MREMAP_FIXED | MREMAP_MAYMOVE, high);
    CATCH_REQUIRE(moved == high);

    verifyRegions(mapper, {
        {mmap_min_addr(), (u64)UINT32_MAX},
    });

    munmap(high, 0x2000);
    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MremapUnmappedSourceReturnsError", "[mmap32]") {
    Mapper mapper;
    g_mode32 = true;

    void* result = mapper.remap(g_mode32, (void*)0x40000, 0x1000, 0x2000, MREMAP_MAYMOVE, nullptr);
    CATCH_REQUIRE(result == MAP_FAILED);

    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("FreelistTopOfAddressSpace", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0xffffe000, 0x1000, PROT_NONE, 0);
    verifyRegions(mapper, {{(u32)mmap_min_addr(), 0xffffdfff}, {0xfffff000, UINT32_MAX}});

    MMAP_AT(0xffffc000, 0x4000, PROT_NONE, 0);
    verifyRegions(mapper, {{(u32)mmap_min_addr(), 0xffffbfff}});

    UNMAP_AT(0xffffc000, 0x4000);
    verifyRegions(mapper, {{(u32)mmap_min_addr(), UINT32_MAX}});

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("FreelistFullThenUnmap", "[mmap32]") {
    Mapper mapper;
    g_mode32 = true;

    u64 min = mmap_min_addr();
    CATCH_REQUIRE(mapper.allocate(min, (u64)UINT32_MAX + 1 - min) == (void*)min);
    verifyRegions(mapper, {});

    CATCH_REQUIRE(mapper.unmap(true, (void*)0x100000, 0x1000) == 0);
    verifyRegions(mapper, {{0x100000, 0x100fff}});

    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Simple1", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x20000, 0x10000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x20000, 0x10000, PROT_NONE},
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
        {0x20000, 0x20000, PROT_NONE},
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
        {0x20000, 0x30000, PROT_NONE},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("FirstPages", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;

    MMAP_AT(mmap_min_addr(), 0x10000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {mmap_min_addr(), 0x10000, PROT_NONE}
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
        {mmap_min_addr(), 0x10000, PROT_NONE}
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
        {end - 0x10000, 0x10000, PROT_NONE}
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
        {0x30000, 0x10000, PROT_NONE},
        {0x50000, 0x10000, PROT_NONE},
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
        {0x30000, 0x30000, PROT_NONE},
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
        {0x20000, 0x100000, PROT_NONE},
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
        {0x20000, 0x100000, PROT_NONE},
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
        {0x30000, 0x10000, PROT_NONE},
        {0x50000, 0x10000, PROT_NONE},
        {0x100000, 0x1000, PROT_NONE},
    });

    MMAP_AT(0x20000, 0x100000 - 0x20000, PROT_NONE, 0); // this mapping consumes the first two

    verifyGuestRegions(mapper, { 
        {0x20000, 0x101000 - 0x20000, PROT_NONE},
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
        {0x30000, 0x10000, PROT_NONE},
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
        {0x20000, 0x30000, PROT_NONE},
    });

    UNMAP_AT(0x30000, 0x10000);

    verifyGuestRegions(mapper, { 
        {0x20000, 0x10000, PROT_NONE},
        {0x40000, 0x10000, PROT_NONE},
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
        {0x20000, 0x100000, PROT_NONE},
    });

    // Unmap more pages than we mapped, this is allowed
    UNMAP_AT(0x1F000, 0x10000);

    verifyGuestRegions(mapper, { 
        {0x2F000, 0x100000 - 0x0F000, PROT_NONE},
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
        {0x20000, 0x100000, PROT_NONE},
    });

    // Unmap more pages than we mapped, this is allowed
    UNMAP_AT(0x115000, 0x10000);

    verifyGuestRegions(mapper, { 
        {0x20000, 0x115000 - 0x20000, PROT_NONE},
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
        {(u64)address, 0x100000, PROT_NONE},
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
        {0x10000, 0x63000, PROT_NONE},
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
        {0x12000, 0x60000, PROT_NONE},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("OverwriteFixedNonMergeableNeighbor", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x30000, 0x4000, PROT_NONE, 0);
    MMAP_AT(0x34000, 0x3000, PROT_READ, 0);

    verifyGuestRegions(mapper, {
        {0x30000, 0x4000, PROT_NONE},
        {0x34000, 0x3000, PROT_READ},
    });

    MMAP_AT(0x34000, 0x6000, PROT_NONE, 0);

    verifyGuestRegions(mapper, {
        {0x30000, 0xa000, PROT_NONE},
    });

    CATCH_REQUIRE(mapper.total_mapped_memory() == 0xa000);

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
        {0x40000, 0x10000, PROT_NONE},
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
        {0x40000, 0x20000, PROT_READ},
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
        {0x13000, 0x10000, PROT_NONE},
        {0x40000, 0x10000, PROT_NONE},
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
        {0x10000, 0x10000, PROT_NONE},
    });

    MREMAP(0x10000, 0x10000, 0x20000, 0);

    verifyGuestRegions(mapper, { 
        {0x10000, 0x20000, PROT_NONE},
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
        {0x10000, 0x10000, PROT_NONE},
    });

    MREMAP_AT(0x10000, 0x10000, 0x40000, 0x20000, MREMAP_FIXED | MREMAP_MAYMOVE);

    verifyGuestRegions(mapper, { 
        {0x40000, 0x20000, PROT_NONE},
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
        {0x10000, 0x20000, PROT_NONE},
    });

    MREMAP(0x10000, 0x20000, 0x10000, 0);

    verifyGuestRegions(mapper, { 
        {0x10000, 0x10000, PROT_NONE},
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
        {0x10000, 0x20000, PROT_NONE},
    });

    MREMAP_AT(0x10000, 0x20000, 0x40000, 0x10000, MREMAP_FIXED | MREMAP_MAYMOVE);

    verifyGuestRegions(mapper, { 
        {0x40000, 0x10000, PROT_NONE},
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
        {0x10000, 0x30000, PROT_NONE},
    });

    MREMAP_AT(0x20000, 0x10000, 0x60000, 0x10000, MREMAP_FIXED | MREMAP_MAYMOVE);

    verifyGuestRegions(mapper, { 
        {0x10000, 0x10000, PROT_NONE},
        {0x30000, 0x10000, PROT_NONE},
        {0x60000, 0x10000, PROT_NONE},
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
        {0x10000, 0x40000, PROT_NONE},
    });

    MREMAP_AT(0x20000, 0x20000, 0x60000, 0x10000, MREMAP_FIXED | MREMAP_MAYMOVE);

    verifyGuestRegions(mapper, { 
        {0x10000, 0x10000, PROT_NONE},
        {0x40000, 0x10000, PROT_NONE},
        {0x60000, 0x10000, PROT_NONE},
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
        {0x10000, 0x30000, PROT_NONE},
    });

    MREMAP_AT(0x20000, 0x10000, 0x60000, 0x20000, MREMAP_FIXED | MREMAP_MAYMOVE);

    verifyGuestRegions(mapper, { 
        {0x10000, 0x10000, PROT_NONE},
        {0x30000, 0x10000, PROT_NONE},
        {0x60000, 0x20000, PROT_NONE},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MremapShrinkMiddle", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x10000, 0x30000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x10000, 0x30000, PROT_NONE},
    });

    MREMAP(0x20000, 0x10000, 0x1000, 0);

    verifyGuestRegions(mapper, { 
        {0x10000, 0x11000, PROT_NONE},
        {0x30000, 0x10000, PROT_NONE},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MremapShrinkDifferentPriv", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x10000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x20000, 0x10000, PROT_WRITE, 0);
    MMAP_AT(0x30000, 0x10000, PROT_READ, 0);

    verifyGuestRegions(mapper, { 
        {0x10000, 0x10000, PROT_NONE},
        {0x20000, 0x10000, PROT_WRITE},
        {0x30000, 0x10000, PROT_READ},
    });

    MREMAP(0x10000, 0x30000, 0x1000, 0);

    verifyGuestRegions(mapper, {
        {0x10000, 0x1000, PROT_NONE},     
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
        {0x10000, 0x10000, PROT_NONE},
        {0x20000, 0x10000, PROT_WRITE},
        {0x30000, 0x10000, PROT_READ},
        {0x40000, 0x10000, PROT_NONE},
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
        {0x10000, 0x30000, PROT_NONE},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("PlacementFlagsDoNotAffectMerging", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x30000, 0x10000, PROT_READ, 0);

    void* second = mapper.map(g_mode32, (void*)0x40000, 0x10000, PROT_READ, FCOMMON | MAP_FIXED_NOREPLACE, -1, 0);
    CATCH_REQUIRE(second == (void*)0x40000);
    unmap_me.push_back({0x40000, 0x10000});

    verifyGuestRegions(mapper, {
        {0x30000, 0x20000, PROT_READ},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("PopulateDoesNotAffectMerging", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x30000, 0x10000, PROT_READ, 0);
    MMAP_AT(0x40000, 0x10000, PROT_READ, MAP_POPULATE);

    verifyGuestRegions(mapper, {
        {0x30000, 0x20000, PROT_READ},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("FileOffsetAffectsMerging", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    char path[] = "/tmp/felix86_mmap_testXXXXXX";
    int fd = mkstemp(path);
    CATCH_REQUIRE(fd != -1);
    CATCH_REQUIRE(ftruncate(fd, 0x10000) == 0);
    unlink(path);

    int flags = MAP_PRIVATE | MAP_FIXED;
    CATCH_REQUIRE(mapper.map(g_mode32, (void*)0x30000, 0x1000, PROT_READ, flags, fd, 0) == (void*)0x30000);
    CATCH_REQUIRE(mapper.map(g_mode32, (void*)0x31000, 0x1000, PROT_READ, flags, fd, 0xF000) == (void*)0x31000);
    unmap_me.push_back({0x30000, 0x2000});

    auto regions = mapper.get_guest_regions();
    CATCH_REQUIRE(regions.size() == 2);
    CATCH_REQUIRE(regions[0].start == 0x30000);
    CATCH_REQUIRE(regions[0].end == 0x31000);
    CATCH_REQUIRE(regions[1].start == 0x31000);
    CATCH_REQUIRE(regions[1].end == 0x32000);

    close(fd);
    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("SameFileThroughDifferentFdsMerges", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    char path[] = "/tmp/felix86_mmap_testXXXXXX";
    int fd = mkstemp(path);
    CATCH_REQUIRE(fd != -1);
    CATCH_REQUIRE(ftruncate(fd, 0x10000) == 0);
    int other_fd = dup(fd);
    CATCH_REQUIRE(other_fd != -1);
    unlink(path);

    int flags = MAP_PRIVATE | MAP_FIXED;
    CATCH_REQUIRE(mapper.map(g_mode32, (void*)0x30000, 0x1000, PROT_READ, flags, fd, 0) == (void*)0x30000);
    CATCH_REQUIRE(mapper.map(g_mode32, (void*)0x31000, 0x1000, PROT_READ, flags, other_fd, 0x1000) == (void*)0x31000);
    unmap_me.push_back({0x30000, 0x2000});

    auto regions = mapper.get_guest_regions();
    CATCH_REQUIRE(regions.size() == 1);
    CATCH_REQUIRE(regions[0].start == 0x30000);
    CATCH_REQUIRE(regions[0].end == 0x32000);

    close(fd);
    close(other_fd);
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
        {0x10000, 0x10000, PROT_NONE},
        {0x30000, 0x10000, PROT_NONE},
        {0x50000, 0x10000, PROT_NONE},
    });

    MMAP_AT(0x20000, 0x30000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x10000, 0x50000, PROT_NONE},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MremapDoNotUnmapMiddle", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    MMAP_AT(0x10000, 0x30000, PROT_NONE, 0);
    MREMAP_AT(0x20000, 0x10000, 0x60000, 0x10000, MREMAP_FIXED | MREMAP_MAYMOVE | MREMAP_DONTUNMAP);

    verifyGuestRegions(mapper, {
        {0x10000, 0x30000, PROT_NONE},
        {0x60000, 0x10000, PROT_NONE},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("SplitFileMappingAdjustsOffset", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    char path[] = "/tmp/felix86_mmap_testXXXXXX";
    int fd = mkstemp(path);
    CATCH_REQUIRE(fd != -1);
    CATCH_REQUIRE(ftruncate(fd, 0x10000) == 0);
    unlink(path);

    int flags = MAP_PRIVATE | MAP_FIXED;
    CATCH_REQUIRE(mapper.map(g_mode32, (void*)0x30000, 0x3000, PROT_READ, flags, fd, 0) == (void*)0x30000);
    CATCH_REQUIRE(mapper.map(g_mode32, (void*)0x31000, 0x1000, PROT_READ, FCOMMON_FIXED, -1, 0) == (void*)0x31000);
    unmap_me.push_back({0x30000, 0x3000});

    auto regions = mapper.get_guest_regions();
    CATCH_REQUIRE(regions.size() == 3);
    CATCH_REQUIRE(regions[0].start == 0x30000);
    CATCH_REQUIRE(regions[0].offset == 0);
    CATCH_REQUIRE(regions[2].start == 0x32000);
    CATCH_REQUIRE(regions[2].offset == 0x2000);

    close(fd);
    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("TrimFileMappingStartAdjustsOffset", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    char path[] = "/tmp/felix86_mmap_testXXXXXX";
    int fd = mkstemp(path);
    CATCH_REQUIRE(fd != -1);
    CATCH_REQUIRE(ftruncate(fd, 0x10000) == 0);
    unlink(path);

    int flags = MAP_PRIVATE | MAP_FIXED;
    CATCH_REQUIRE(mapper.map(g_mode32, (void*)0x31000, 0x2000, PROT_READ, flags, fd, 0x1000) == (void*)0x31000);
    unmap_me.push_back({0x31000, 0x2000});

    UNMAP_AT(0x30000, 0x2000);

    auto regions = mapper.get_guest_regions();
    CATCH_REQUIRE(regions.size() == 1);
    CATCH_REQUIRE(regions[0].start == 0x32000);
    CATCH_REQUIRE(regions[0].end == 0x33000);
    CATCH_REQUIRE(regions[0].offset == 0x2000);

    close(fd);
    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MremapMoveMiddleOfFileMapping", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    char path[] = "/tmp/felix86_mmap_testXXXXXX";
    int fd = mkstemp(path);
    CATCH_REQUIRE(fd != -1);
    CATCH_REQUIRE(ftruncate(fd, 0x10000) == 0);
    unlink(path);

    int flags = MAP_PRIVATE | MAP_FIXED;
    CATCH_REQUIRE(mapper.map(g_mode32, (void*)0x30000, 0x3000, PROT_READ, flags, fd, 0) == (void*)0x30000);
    unmap_me.push_back({0x30000, 0x3000});

    MREMAP_AT(0x31000, 0x1000, 0x50000, 0x1000, MREMAP_FIXED | MREMAP_MAYMOVE);

    auto regions = mapper.get_guest_regions();
    CATCH_REQUIRE(regions.size() == 3);
    CATCH_REQUIRE(regions[1].start == 0x32000);
    CATCH_REQUIRE(regions[1].offset == 0x2000);
    CATCH_REQUIRE(regions[2].start == 0x50000);
    CATCH_REQUIRE(regions[2].offset == 0x1000);

    close(fd);
    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("FileOffsetMergeDirection", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    char path[] = "/tmp/felix86_mmap_testXXXXXX";
    int fd = mkstemp(path);
    CATCH_REQUIRE(fd != -1);
    CATCH_REQUIRE(ftruncate(fd, 0x10000) == 0);
    unlink(path);

    int flags = MAP_PRIVATE | MAP_FIXED;
    CATCH_REQUIRE(mapper.map(g_mode32, (void*)0x31000, 0x1000, PROT_READ, flags, fd, 0x1000) == (void*)0x31000);
    CATCH_REQUIRE(mapper.map(g_mode32, (void*)0x30000, 0x1000, PROT_READ, flags, fd, 0x2000) == (void*)0x30000);
    unmap_me.push_back({0x30000, 0x2000});

    auto regions = mapper.get_guest_regions();
    CATCH_REQUIRE(regions.size() == 2);
    CATCH_REQUIRE(regions[0].offset == 0x2000);
    CATCH_REQUIRE(regions[1].offset == 0x1000);

    close(fd);
    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MergeLeadingFileMappingKeepsOffset", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    char path[] = "/tmp/felix86_mmap_testXXXXXX";
    int fd = mkstemp(path);
    CATCH_REQUIRE(fd != -1);
    CATCH_REQUIRE(ftruncate(fd, 0x10000) == 0);
    unlink(path);

    int flags = MAP_PRIVATE | MAP_FIXED;
    CATCH_REQUIRE(mapper.map(g_mode32, (void*)0x31000, 0x1000, PROT_READ, flags, fd, 0x1000) == (void*)0x31000);
    CATCH_REQUIRE(mapper.map(g_mode32, (void*)0x30000, 0x1000, PROT_READ, flags, fd, 0) == (void*)0x30000);
    unmap_me.push_back({0x30000, 0x2000});

    auto regions = mapper.get_guest_regions();
    CATCH_REQUIRE(regions.size() == 1);
    CATCH_REQUIRE(regions[0].start == 0x30000);
    CATCH_REQUIRE(regions[0].end == 0x32000);
    CATCH_REQUIRE(regions[0].offset == 0);

    close(fd);
    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MProtect1", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x20000, 0x10000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x20000, 0x10000, PROT_NONE},
    });

    mapper.protect((void*)0x20000, 0x10000, PROT_WRITE);

    verifyGuestRegions(mapper, { 
        {0x20000, 0x10000, PROT_WRITE},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MProtectLeft", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x20000, 0x20000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x20000, 0x20000, PROT_NONE},
    });

    mapper.protect((void*)0x20000, 0x10000, PROT_WRITE);

    verifyGuestRegions(mapper, { 
        {0x20000, 0x10000, PROT_WRITE},
        {0x30000, 0x10000, PROT_NONE},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MProtectRight", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x20000, 0x20000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x20000, 0x20000, PROT_NONE},
    });

    mapper.protect((void*)0x30000, 0x10000, PROT_WRITE);

    verifyGuestRegions(mapper, { 
        {0x20000, 0x10000, PROT_NONE},
        {0x30000, 0x10000, PROT_WRITE},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}


CATCH_TEST_CASE("MProtectLeftRight", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x20000, 0x20000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x20000, 0x20000, PROT_NONE},
    });

    mapper.protect((void*)0x20000, 0x10000, PROT_WRITE);
    mapper.protect((void*)0x30000, 0x10000, PROT_READ);

    verifyGuestRegions(mapper, { 
        {0x20000, 0x10000, PROT_WRITE},
        {0x30000, 0x10000, PROT_READ},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MProtectLeftRightMerge", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x20000, 0x20000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x20000, 0x20000, PROT_NONE},
    });

    mapper.protect((void*)0x20000, 0x10000, PROT_WRITE);
    mapper.protect((void*)0x30000, 0x10000, PROT_WRITE);

    verifyGuestRegions(mapper, { 
        {0x20000, 0x20000, PROT_WRITE},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MProtectLeftRightThenMerge", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x20000, 0x20000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x20000, 0x20000, PROT_NONE},
    });

    mapper.protect((void*)0x20000, 0x10000, PROT_WRITE);
    mapper.protect((void*)0x30000, 0x10000, PROT_READ);

    verifyGuestRegions(mapper, { 
        {0x20000, 0x10000, PROT_WRITE},
        {0x30000, 0x10000, PROT_READ},
    });

    mapper.protect((void*)0x20000, 0x20000, PROT_WRITE);

    verifyGuestRegions(mapper, { 
        {0x20000, 0x20000, PROT_WRITE},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MProtectMiddle", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x20000, 0x30000, PROT_NONE, 0);

    verifyGuestRegions(mapper, { 
        {0x20000, 0x30000, PROT_NONE},
    });

    mapper.protect((void*)0x30000, 0x10000, PROT_WRITE);

    verifyGuestRegions(mapper, { 
        {0x20000, 0x10000, PROT_NONE},
        {0x30000, 0x10000, PROT_WRITE},
        {0x40000, 0x10000, PROT_NONE},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MremapGrowFromInsideRegion", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x20000, 0x10000, PROT_NONE, 0);
    MMAP_AT(0x50000, 0x1000, PROT_NONE, 0);

    CATCH_REQUIRE(mapper.remap(g_mode32, (void*)0x21000, 0x1000, 0x2000, MREMAP_MAYMOVE | MREMAP_FIXED, (void*)0x60000) == (void*)0x60000);
    unmap_me.push_back({0x60000, 0x2000});

    verifyGuestRegions(mapper, {
        {0x20000, 0x1000, PROT_NONE},
        {0x22000, 0xe000, PROT_NONE},
        {0x50000, 0x1000, PROT_NONE},
        {0x60000, 0x2000, PROT_NONE},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Map64DoesNotReserveLowMemory", "[mmap]") {
    Mapper mapper;
    g_mode32 = false;

    void* high = mapper.map(false, nullptr, 0x5000, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    CATCH_REQUIRE(high != MAP_FAILED);
    CATCH_REQUIRE((u64)high > UINT32_MAX);

    u64 min = mmap_min_addr();
    void* low = mapper.map(true, (void*)min, 0x1000, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED_NOREPLACE, -1, 0);
    CATCH_REQUIRE(low == (void*)min);

    verifyRegions(mapper, {{(u32)min + 0x1000, UINT32_MAX}});

    munmap(high, 0x5000);
    munmap(low, 0x1000);
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Map64DoesNotConsumeFreelist", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    verifyRegions(mapper, {{(u32)mmap_min_addr(), UINT32_MAX}});

    void* address = nullptr;
    MMAP_AT_R(address, 0x10000, PROT_NONE, 0);
    CATCH_REQUIRE(address != MAP_FAILED);
    CATCH_REQUIRE((u64)address > UINT32_MAX);

    verifyRegions(mapper, {{(u32)mmap_min_addr(), UINT32_MAX}});

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Remap64IntoLowDoesNotFreeUnrelated", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    const u64 high = 0x200050000ull;
    CATCH_REQUIRE(mapper.map(true, (void*)0x50000, 0x1000, PROT_NONE, FCOMMON_FIXED, -1, 0) == (void*)0x50000);
    CATCH_REQUIRE(mapper.map(false, (void*)high, 0x1000, PROT_NONE, FCOMMON_FIXED, -1, 0) == (void*)high);
    unmap_me.push_back({0x50000, 0x1000});

    verifyRegions(mapper, {{(u32)mmap_min_addr(), 0x4ffff}, {0x51000, UINT32_MAX}});

    CATCH_REQUIRE(mapper.remap(false, (void*)high, 0x1000, 0x1000, MREMAP_MAYMOVE | MREMAP_FIXED, (void*)0x60000) == (void*)0x60000);
    unmap_me.push_back({0x60000, 0x1000});

    verifyRegions(mapper, {{(u32)mmap_min_addr(), 0x4ffff}, {0x51000, 0x5ffff}, {0x61000, UINT32_MAX}});

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MremapGrowInPlaceExtendsRegion", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    char path[] = "/tmp/felix86_mmap_testXXXXXX";
    int fd = mkstemp(path);
    CATCH_REQUIRE(fd != -1);
    CATCH_REQUIRE(ftruncate(fd, 0x10000) == 0);
    unlink(path);

    int flags = MAP_PRIVATE | MAP_FIXED;
    CATCH_REQUIRE(mapper.map(g_mode32, (void*)0x30000, 0x1000, PROT_READ, flags, fd, 0x1000) == (void*)0x30000);
    CATCH_REQUIRE(mapper.remap(g_mode32, (void*)0x30000, 0x1000, 0x2000, 0, (void*)0x30000) == (void*)0x30000);
    unmap_me.push_back({0x30000, 0x2000});

    auto regions = mapper.get_guest_regions();
    CATCH_REQUIRE(regions.size() == 1);
    CATCH_REQUIRE(regions[0].start == 0x30000);
    CATCH_REQUIRE(regions[0].end == 0x32000);
    CATCH_REQUIRE(regions[0].offset == 0x1000);

    close(fd);
    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MremapGrowKeepsOwnIdentity", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    char path[] = "/tmp/felix86_mmap_testXXXXXX";
    int fd = mkstemp(path);
    CATCH_REQUIRE(fd != -1);
    CATCH_REQUIRE(ftruncate(fd, 0x10000) == 0);
    unlink(path);

    CATCH_REQUIRE(mapper.map(g_mode32, (void*)0x30000, 0x1000, PROT_READ, MAP_PRIVATE | MAP_FIXED, fd, 0x1000) == (void*)0x30000);
    unmap_me.push_back({0x30000, 0x1000});
    MMAP_AT(0x50000, 0x1000, PROT_READ, 0);

    auto old_regions = mapper.get_guest_regions();
    ino_t ino = old_regions[1].ino;

    CATCH_REQUIRE(mapper.remap(g_mode32, (void*)0x50000, 0x1000, 0x2000, 0, (void*)0x50000) == (void*)0x50000);
    unmap_me.push_back({0x50000, 0x2000});

    auto regions = mapper.get_guest_regions();
    CATCH_REQUIRE(regions.size() == 2);
    CATCH_REQUIRE(regions[1].start == 0x50000);
    CATCH_REQUIRE(regions[1].end == 0x52000);
    CATCH_REQUIRE(regions[1].ino == ino);
    CATCH_REQUIRE(regions[1].offset == 0);

    close(fd);
    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MProtectSplitKeepsFileOffset", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    char path[] = "/tmp/felix86_mmap_testXXXXXX";
    int fd = mkstemp(path);
    CATCH_REQUIRE(fd != -1);
    CATCH_REQUIRE(ftruncate(fd, 0x10000) == 0);
    unlink(path);

    CATCH_REQUIRE(mapper.map(g_mode32, (void*)0x30000, 0x2000, PROT_READ, MAP_PRIVATE | MAP_FIXED, fd, 0x1000) == (void*)0x30000);
    unmap_me.push_back({0x30000, 0x2000});

    CATCH_REQUIRE(mapper.protect((void*)0x30000, 0x1000, PROT_NONE) == 0);

    auto regions = mapper.get_guest_regions();
    CATCH_REQUIRE(regions.size() == 2);
    CATCH_REQUIRE(regions[0].prot == PROT_NONE);
    CATCH_REQUIRE(regions[0].offset == 0x1000);
    CATCH_REQUIRE(regions[1].prot == PROT_READ);
    CATCH_REQUIRE(regions[1].offset == 0x2000);

    close(fd);
    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MProtectSameProtDoesNotSplit", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x20000, 0x30000, PROT_NONE, 0);

    CATCH_REQUIRE(mapper.protect((void*)0x30000, 0x10000, PROT_NONE) == 0);

    verifyGuestRegions(mapper, {
        {0x20000, 0x30000, PROT_NONE},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MremapSameSizeInPlaceDoesNotSplit", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x20000, 0x30000, PROT_NONE, 0);

    CATCH_REQUIRE(mapper.remap(g_mode32, (void*)0x30000, 0x10000, 0x10000, 0, (void*)0x30000) == (void*)0x30000);

    verifyGuestRegions(mapper, {
        {0x20000, 0x30000, PROT_NONE},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("SharedAnonymousMappingsDoNotMerge", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    int flags = MAP_ANONYMOUS | MAP_SHARED | MAP_FIXED;
    CATCH_REQUIRE(mapper.map(g_mode32, (void*)0x30000, 0x1000, PROT_READ, flags, -1, 0) == (void*)0x30000);
    CATCH_REQUIRE(mapper.map(g_mode32, (void*)0x31000, 0x1000, PROT_READ, flags, -1, 0) == (void*)0x31000);
    unmap_me.push_back({0x30000, 0x2000});

    auto regions = mapper.get_guest_regions();
    CATCH_REQUIRE(regions.size() == 2);
    CATCH_REQUIRE(regions[0].end == 0x31000);
    CATCH_REQUIRE(regions[1].start == 0x31000);

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("SharedAnonymousMappingMergesWithItself", "[mmap]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = false;

    int flags = MAP_ANONYMOUS | MAP_SHARED | MAP_FIXED;
    CATCH_REQUIRE(mapper.map(g_mode32, (void*)0x30000, 0x2000, PROT_READ, flags, -1, 0) == (void*)0x30000);
    unmap_me.push_back({0x30000, 0x2000});

    CATCH_REQUIRE(mapper.protect((void*)0x30000, 0x1000, PROT_NONE) == 0);
    CATCH_REQUIRE(mapper.get_guest_regions().size() == 2);

    CATCH_REQUIRE(mapper.protect((void*)0x30000, 0x1000, PROT_READ) == 0);

    auto regions = mapper.get_guest_regions();
    CATCH_REQUIRE(regions.size() == 1);
    CATCH_REQUIRE(regions[0].start == 0x30000);
    CATCH_REQUIRE(regions[0].end == 0x32000);

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}
