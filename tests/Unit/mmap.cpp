// clang-format off
// Test our 32-bit mmap implementation
#include <catch2/catch_test_macros.hpp>
#include <sys/mman.h>
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/hle/mmap.hpp"

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

#define MMAP_AT(addr, size) \
    do { \
        void* address = mapper.map((void*)(addr), size, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0); \
        CATCH_REQUIRE(address == (void*)(u64)(addr)); \
        unmap_me.push_back({(u64)(addr), size}); \
    } while (0)

#define MREMAP_AT(old_addr, old_size, new_addr, new_size) \
    do {\
        void* address = mapper.remap((void*)old_addr, old_size, new_size, MREMAP_FIXED | MREMAP_MAYMOVE, (void*)new_addr); \
        CATCH_REQUIRE(address == (void*)(u64)(new_addr)); \
        unmap_me.push_back({(u64)(new_addr), new_size}); \
    } while(0)

#define MMAP_AT_R(size) \
    do { \
        void* address = mapper.map((void*)(0), size, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0); \
        unmap_me.push_back({(u64)(address), size}); \
    } while (0)

// Doesn't need to erase from unmap_me
#define UNMAP_AT(addr, size) \
    do { \
        int result = mapper.unmap((void*)(addr), size); \
        CATCH_REQUIRE(result == 0); \
    } while(0)

#define MUNMAP_ALL() \
    do { \
        for (size_t i = 0; i < unmap_me.size(); i++) { \
            auto [addr, size] = unmap_me[i]; \
            munmap((void*)(u64)addr, size); \
        } \
    } while (0)

void verifyRegions(Mapper& mapper, const std::vector<std::pair<u32, u32>>& expected_regions) {
    auto actual_regions = mapper.getRegions();
    CATCH_REQUIRE(expected_regions == actual_regions);
}

CATCH_TEST_CASE("Simple1", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x20000, 0x10000);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x20000 - 1},
        {0x20000 + 0x10000, Mapper::addressSpaceEnd32},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Simple2", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x20000, 0x10000);
    MMAP_AT(0x30000, 0x10000);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x20000 - 1},
        {0x20000 + 0x10000 + 0x10000, Mapper::addressSpaceEnd32},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Simple3", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x20000, 0x10000);
    MMAP_AT(0x30000, 0x10000);
    MMAP_AT(0x40000, 0x10000);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x20000 - 1},
        {0x20000 + 0x10000 + 0x10000 + 0x10000, Mapper::addressSpaceEnd32},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("FirstPages", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(mmap_min_addr(), 0x10000);

    verifyRegions(mapper, {
        {mmap_min_addr() + 0x10000, Mapper::addressSpaceEnd32},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}


CATCH_TEST_CASE("FirstPagesUnmap", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(mmap_min_addr(), 0x10000);

    verifyRegions(mapper, {
        {mmap_min_addr() + 0x10000, Mapper::addressSpaceEnd32},
    });

    UNMAP_AT(mmap_min_addr(), 0x10000);

    verifyRegions(mapper, {
        {mmap_min_addr(), Mapper::addressSpaceEnd32},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("LastPages", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    u64 end = Mapper::addressSpaceEnd32 + 1;

    MMAP_AT(end - 0x10000, 0x10000);

    verifyRegions(mapper, {
        {mmap_min_addr(), end - 0x10000 - 1},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Split2", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x30000, 0x10000);
    MMAP_AT(0x50000, 0x10000);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x30000 - 1},
        {0x30000 + 0x10000, 0x50000 - 1},
        {0x50000 + 0x10000, Mapper::addressSpaceEnd32},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Split2Pick1", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x30000, 0x10000);
    MMAP_AT(0x50000, 0x10000);

    // Mmap exactly in the middle of the two previous ones
    MMAP_AT(0x40000, 0x10000);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x30000 - 1},
        {0x50000 + 0x10000, Mapper::addressSpaceEnd32},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Overlapping1", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x30000, 0x10000);
    MMAP_AT(0x20000, 0x100000); // this mapping consumes the first

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x20000 - 1},
        {0x20000 + 0x100000, Mapper::addressSpaceEnd32},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Overlapping2", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x30000, 0x10000);
    MMAP_AT(0x50000, 0x10000);
    MMAP_AT(0x20000, 0x100000); // this mapping consumes the other two

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x20000 - 1},
        {0x20000 + 0x100000, Mapper::addressSpaceEnd32},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Overlapping2ConsumeLast", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x30000, 0x10000);
    MMAP_AT(0x50000, 0x10000);
    MMAP_AT(0x100000, 0x1000);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x30000 - 1},
        {0x40000, 0x50000 - 1},
        {0x60000, 0x100000 - 1}, // this test ensures this block is properly deleted
        {0x101000, Mapper::addressSpaceEnd32},
    });

    MMAP_AT(0x20000, 0x100000 - 0x20000); // this mapping consumes the first two

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x20000 - 1},
        {0x101000, Mapper::addressSpaceEnd32},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("UnmapPerfect", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x30000, 0x10000);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x30000 - 1},
        {0x40000, Mapper::addressSpaceEnd32},
    });

    UNMAP_AT(0x30000, 0x10000);

    verifyRegions(mapper, {
        {mmap_min_addr(), Mapper::addressSpaceEnd32},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("UnmapPerfect2", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x20000, 0x10000);
    MMAP_AT(0x30000, 0x10000);
    MMAP_AT(0x40000, 0x10000);
    
    verifyRegions(mapper, {
        {mmap_min_addr(), 0x20000 - 1},
        {0x50000, Mapper::addressSpaceEnd32},
    });

    UNMAP_AT(0x30000, 0x10000);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x20000 - 1},
        {0x30000, 0x40000 - 1},
        {0x50000, Mapper::addressSpaceEnd32},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("UnmapMiddle", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x20000, 0x10000);
    MMAP_AT(0x30000, 0x10000);
    MMAP_AT(0x40000, 0x10000);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x20000 - 1},
        {0x50000, Mapper::addressSpaceEnd32},
    });

    UNMAP_AT(0x30000, 0x10000);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x20000 - 1},
        {0x30000, 0x40000 - 1},
        {0x50000, Mapper::addressSpaceEnd32},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("UnmapGreedyMin", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x20000, 0x100000);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x20000 - 1},
        {0x120000, Mapper::addressSpaceEnd32},
    });

    // Unmap more pages than we mapped, this is allowed
    UNMAP_AT(0x1F000, 0x10000);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x2F000 - 1},
        {0x120000, Mapper::addressSpaceEnd32},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("UnmapGreedyMax", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x20000, 0x100000);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x20000 - 1},
        {0x120000, Mapper::addressSpaceEnd32},
    });

    // Unmap more pages than we mapped, this is allowed
    UNMAP_AT(0x115000, 0x10000);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x20000 - 1},
        {0x115000, Mapper::addressSpaceEnd32},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("MapRandom", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT_R(0x100000);

    // Random mmaps always pick from first page if possible
    verifyRegions(mapper, {
        {mmap_min_addr() + 0x100000, Mapper::addressSpaceEnd32},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("OverwriteFixed", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x10000, 0x200c);
    MMAP_AT(0x13000, 0x34a18);
    MMAP_AT(0x13000, 0x60000);

    verifyRegions(mapper, {
        {mmap_min_addr() + 0x63000, Mapper::addressSpaceEnd32},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("OverwriteFixed2", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x13000, 0x34a18);
    MMAP_AT(0x12000, 0x60000);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x11fff},
        {mmap_min_addr() + 0x62000, Mapper::addressSpaceEnd32},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Mremap", "[mmap32]") {
    std::vector<std::pair<u32, u32>> unmap_me;
    Mapper mapper;
    g_mode32 = true;

    MMAP_AT(0x13000, 0x10000);
    MREMAP_AT(0x13000, 0x10000, 0x40000, 0x20000);

    verifyRegions(mapper, {
        {mmap_min_addr(), 0x3ffff},
        {0x60000, Mapper::addressSpaceEnd32},
    });

    MUNMAP_ALL();
    SUCCESS_MESSAGE();
}
