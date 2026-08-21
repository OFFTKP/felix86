// clang-format off
// Test our shmat implementation
#include <catch2/catch_test_macros.hpp>
#include <sys/shm.h>
#include <sys/types.h>
#include "felix86/common/log.hpp"
#include "felix86/hle/mmap.hpp"

#define SUCCESS_MESSAGE() SUCCESS("Test passed: %s", Catch::getResultCapture().getCurrentTestName().c_str())

struct GuestRegionExpected {
    u64 start;
    u64 len;
    int flags;
    int prot;
};

void verifyGuestRegions(Mapper& mapper, const std::vector<GuestRegionExpected>& expected_regions) {
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
    int shmid = shmget(key, 1024, 0644 | IPC_CREAT);
    ASSERT(shmid != -1);

    u64 result = 0;
    mapper.shmat(false, shmid, (void*)0x50000, 0, &result);
    ASSERT(result = 0x50000);

    verifyGuestRegions(mapper, { 
        {result, 1024, 0, 0644 | IPC_CREAT},
    });

    mapper.shmdt(false, (void*)result);

    verifyGuestRegions(mapper, {});

    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("Simple1", "[shmat]") {
    Mapper mapper;

    key_t key;
    int shmid = shmget(key, 1024, 0644 | IPC_CREAT);
    ASSERT(shmid != -1);

    u64 result = 0;
    mapper.shmat(false, shmid, 0, 0, &result);

    verifyGuestRegions(mapper, { 
        {result, 1024, 0, 0644 | IPC_CREAT},
    });

    mapper.shmdt(false, (void*)result);

    verifyGuestRegions(mapper, {});

    SUCCESS_MESSAGE();
}
