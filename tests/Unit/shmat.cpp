// clang-format off
// Test our shmat implementation
#include <catch2/catch_test_macros.hpp>
#include <sys/shm.h>
#include <sys/types.h>
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
            yes &= region.flags == guest_region.flags;
            yes &= region.prot == guest_region.prot;
            yes &= guest_region.shmem;
            if (yes) {
                found_regions += 1;
                break;
            };
        }
    }
    CATCH_REQUIRE(found_regions == expected_regions.size());    
    CATCH_REQUIRE(guest_regions.size() == expected_regions.size());
}

CATCH_TEST_CASE("Simple1", "[shmat32]") {
    Mapper mapper;

    key_t key;
    int shmid = shmget(IPC_PRIVATE, 0x1000, 0644 | IPC_CREAT);
    CATCH_REQUIRE(shmid != -1);

    u64 result = 0;
    CATCH_REQUIRE(mapper.shmat(true, shmid, (void*)0x50000, 0, &result) == 0);
    CATCH_REQUIRE(result == 0x50000);

    verifyGuestRegions(mapper, { 
        {result, 0x1000, 0, 0},
    });

    CATCH_REQUIRE(mapper.shmdt(true, (void*)result) == 0);

    verifyGuestRegions(mapper, {});

    CATCH_REQUIRE(shmctl(shmid, IPC_RMID, 0) == 0);
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Simple1", "[shmat]") {
    Mapper mapper;

    key_t key;
    int shmid = shmget(IPC_PRIVATE, 0x1000, 0644 | IPC_CREAT);
    CATCH_REQUIRE(shmid != -1);

    u64 result = 0;
    CATCH_REQUIRE(mapper.shmat(false, shmid, (void*)0x50000, 0, &result) == 0);
    CATCH_REQUIRE(result == 0x50000);

    verifyGuestRegions(mapper, { 
        {result, 0x1000, 0, 0},
    });

    CATCH_REQUIRE(mapper.shmdt(false, (void*)result) == 0);

    verifyGuestRegions(mapper, {});

    CATCH_REQUIRE(shmctl(shmid, IPC_RMID, 0) == 0);
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("DistinctSegments", "[shmat]") {
    Mapper mapper;

    int first_shmid = shmget(IPC_PRIVATE, 0x1000, 0644 | IPC_CREAT);
    int second_shmid = shmget(IPC_PRIVATE, 0x2000, 0644 | IPC_CREAT);
    CATCH_REQUIRE(first_shmid != -1);
    CATCH_REQUIRE(second_shmid != -1);

    u64 first = 0;
    u64 second = 0;
    CATCH_REQUIRE(mapper.shmat(false, first_shmid, (void*)0x100005000ull, 0, &first) == 0);
    CATCH_REQUIRE(mapper.shmat(false, second_shmid, (void*)0x200005000ull, 0, &second) == 0);

    verifyGuestRegions(mapper, { 
        {first, 0x1000, 0, 0},
        {second, 0x2000, 0, 0},
    });

    CATCH_REQUIRE(mapper.shmdt(false, (void*)first) == 0);
 
    verifyGuestRegions(mapper, { 
       {second, 0x2000, 0, 0},
    });
 
    CATCH_REQUIRE(mapper.shmdt(false, (void*)second) == 0);
 
    verifyGuestRegions(mapper, {});
 
    CATCH_REQUIRE(shmctl(first_shmid, IPC_RMID, nullptr) == 0);
    CATCH_REQUIRE(shmctl(second_shmid, IPC_RMID, nullptr) == 0);
    SUCCESS_MESSAGE();
}
