#include <catch2/catch_test_macros.hpp>
#include "felix86/hle/filesystem.hpp"

CATCH_TEST_CASE("InsideRootfs", "[paths]") {
    g_config.rootfs_path = "/home/someuser/myrootfs";

    std::string my_path = "/home/someuser/myrootfs/somedir";
    Filesystem::removeRootfsPrefix(my_path);

    CATCH_REQUIRE(my_path == "/somedir");
}

CATCH_TEST_CASE("IsRootfs", "[paths]") {
    g_config.rootfs_path = "/home/someuser/myrootfs";

    std::string my_path = "/home/someuser/myrootfs";
    Filesystem::removeRootfsPrefix(my_path);

    CATCH_REQUIRE(my_path == "/");
}

CATCH_TEST_CASE("IsRootfs2", "[paths]") {
    g_config.rootfs_path = "/home/someuser/myrootfs";

    std::string my_path = "/home/someuser/myrootfs/";
    Filesystem::removeRootfsPrefix(my_path);

    CATCH_REQUIRE(my_path == "/");
}

CATCH_TEST_CASE("OutsideRootfs", "[paths]") {
    g_config.rootfs_path = "/home/someuser/myrootfs";

    std::string my_path = "/home";
    Filesystem::removeRootfsPrefix(my_path);

    CATCH_REQUIRE(my_path == "/home");
}