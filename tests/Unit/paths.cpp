#include <catch2/catch_test_macros.hpp>
#include "felix86/hle/filesystem.hpp"

#define SUCCESS_MESSAGE() SUCCESS("Test passed: %s", Catch::getResultCapture().getCurrentTestName().c_str())

CATCH_TEST_CASE("InsideRootfs", "[paths]") {
    Config config = g_config;
    g_config.rootfs_path = "/home/someuser/myrootfs";

    std::string my_path = "/home/someuser/myrootfs/somedir";
    Filesystem::removeRootfsPrefix(my_path);

    CATCH_REQUIRE(my_path == "/somedir");
    g_config = config;
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("IsRootfs", "[paths]") {
    Config config = g_config;
    g_config.rootfs_path = "/home/someuser/myrootfs";

    std::string my_path = "/home/someuser/myrootfs";
    Filesystem::removeRootfsPrefix(my_path);

    CATCH_REQUIRE(my_path == "/");
    g_config = config;
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("IsRootfs2", "[paths]") {
    Config config = g_config;
    g_config.rootfs_path = "/home/someuser/myrootfs";

    std::string my_path = "/home/someuser/myrootfs/";
    Filesystem::removeRootfsPrefix(my_path);

    CATCH_REQUIRE(my_path == "/");
    g_config = config;
    SUCCESS_MESSAGE();
}

CATCH_TEST_CASE("OutsideRootfs", "[paths]") {
    Config config = g_config;
    g_config.rootfs_path = "/home/someuser/myrootfs";

    std::string my_path = "/home";
    Filesystem::removeRootfsPrefix(my_path);

    CATCH_REQUIRE(my_path == "/home");
    g_config = config;
    SUCCESS_MESSAGE();
}