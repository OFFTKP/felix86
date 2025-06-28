#include <catch2/catch_test_macros.hpp>
#include <fcntl.h>
#include "felix86/hle/filesystem.hpp"

#define SUCCESS_MESSAGE() SUCCESS("Test passed: %s", Catch::getResultCapture().getCurrentTestName().c_str())

#define PROLOGUE()                                                                                                                                   \
    Config config = g_config;                                                                                                                        \
    int fd = g_rootfs_fd;                                                                                                                            \
    g_config.rootfs_path = "/home/someuser/myrootfs";                                                                                                \
    g_rootfs_fd = 50

#define EPILOGUE()                                                                                                                                   \
    g_config = config;                                                                                                                               \
    g_rootfs_fd = fd;                                                                                                                                \
    SUCCESS_MESSAGE()

CATCH_TEST_CASE("InsideRootfs", "[paths]") {
    PROLOGUE();

    std::string my_path = "/home/someuser/myrootfs/somedir";
    Filesystem::removeRootfsPrefix(my_path);

    CATCH_REQUIRE(my_path == "/somedir");

    EPILOGUE();
}

CATCH_TEST_CASE("IsRootfs", "[paths]") {
    PROLOGUE();

    std::string my_path = "/home/someuser/myrootfs";
    Filesystem::removeRootfsPrefix(my_path);

    CATCH_REQUIRE(my_path == "/");

    EPILOGUE();
}

CATCH_TEST_CASE("IsRootfs2", "[paths]") {
    PROLOGUE();

    std::string my_path = "/home/someuser/myrootfs/";
    Filesystem::removeRootfsPrefix(my_path);

    CATCH_REQUIRE(my_path == "/");

    EPILOGUE();
}

CATCH_TEST_CASE("OutsideRootfs", "[paths]") {
    PROLOGUE();

    std::string my_path = "/home";
    Filesystem::removeRootfsPrefix(my_path);

    CATCH_REQUIRE(my_path == "/home");

    EPILOGUE();
}

CATCH_TEST_CASE("Resolve", "[paths]") {
    PROLOGUE();

    auto path = Filesystem::resolve("/etc/drirc", true);
    CATCH_REQUIRE(path.get_str());
    CATCH_REQUIRE(std::string(path.get_str()) == "/home/someuser/myrootfs/etc/drirc");

    auto [new_fd, new_path] = Filesystem::resolve(AT_FDCWD, "/etc/drirc", true);
    CATCH_REQUIRE(new_fd == 50);
    CATCH_REQUIRE(new_path.get_str());
    CATCH_REQUIRE(std::string(new_path.get_str()) == "etc/drirc");

    EPILOGUE();
}

CATCH_TEST_CASE("ResolveNull", "[paths]") {
    PROLOGUE();

    auto [new_fd, new_path] = Filesystem::resolve(AT_FDCWD, nullptr, false);
    CATCH_REQUIRE(new_fd == AT_FDCWD);
    CATCH_REQUIRE(new_path.get_str() == nullptr);

    EPILOGUE();
}