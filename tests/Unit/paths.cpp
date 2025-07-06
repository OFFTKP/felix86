#include <catch2/catch_test_macros.hpp>
#include <fcntl.h>
#include "felix86/hle/filesystem.hpp"
#include "fmt/format.h"

#define SUCCESS_MESSAGE() SUCCESS("Test passed: %s", Catch::getResultCapture().getCurrentTestName().c_str())

#define PROLOGUE()                                                                                                                                   \
    Config config = g_config;                                                                                                                        \
    int fd = g_rootfs_fd;                                                                                                                            \
    g_config.rootfs_path = "/tmp/felix86_paths/myrootfs";                                                                                            \
    std::filesystem::create_directories(g_config.rootfs_path);                                                                                       \
    g_rootfs_fd = open(g_config.rootfs_path.c_str(), O_PATH | O_DIRECTORY);                                                                          \
    ASSERT(g_rootfs_fd > 0)

#define EPILOGUE()                                                                                                                                   \
    ASSERT(close(g_rootfs_fd) == 0);                                                                                                                 \
    g_config = config;                                                                                                                               \
    g_rootfs_fd = fd;                                                                                                                                \
    SUCCESS_MESSAGE()

CATCH_TEST_CASE("InsideRootfs", "[paths]") {
    PROLOGUE();

    std::string my_path = "/tmp/felix86_paths/myrootfs/somedir";
    Filesystem::removeRootfsPrefix(my_path);

    CATCH_REQUIRE(my_path == "/somedir");

    EPILOGUE();
}

CATCH_TEST_CASE("IsRootfs", "[paths]") {
    PROLOGUE();

    std::string my_path = "/tmp/felix86_paths/myrootfs";
    Filesystem::removeRootfsPrefix(my_path);

    CATCH_REQUIRE(my_path == "/");

    EPILOGUE();
}

CATCH_TEST_CASE("IsRootfs2", "[paths]") {
    PROLOGUE();

    std::string my_path = "/tmp/felix86_paths/myrootfs/";
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

CATCH_TEST_CASE("ResolveSimple", "[paths]") {
    PROLOGUE();

    std::filesystem::create_directories(g_config.rootfs_path / "temp1");

    {
        FdPath fd_path = Filesystem::resolve("/temp1", true);
        CATCH_INFO(fmt::format("Error: {}", strerror(fd_path.get_errno())));
        CATCH_REQUIRE(!fd_path.is_error());
        CATCH_REQUIRE(std::string(fd_path.full_path()) == g_config.rootfs_path / "temp1");
    }
    {
        fchdir(g_rootfs_fd);
        FdPath fd_path = Filesystem::resolve("temp1", true);
        CATCH_INFO(fmt::format("Error: {}", strerror(fd_path.get_errno())));
        CATCH_REQUIRE(!fd_path.is_error());
        CATCH_REQUIRE(std::string(fd_path.full_path()) == g_config.rootfs_path / "temp1");
    }
    {
        FdPath fd_path = Filesystem::resolve(AT_FDCWD, "temp1", true);
        CATCH_INFO(fmt::format("Error: {}", strerror(fd_path.get_errno())));
        CATCH_REQUIRE(!fd_path.is_error());
        CATCH_REQUIRE(std::string(fd_path.full_path()) == g_config.rootfs_path / "temp1");
    }

    EPILOGUE();
}

CATCH_TEST_CASE("ResolveSymlinkAbsolute1", "[paths]") {
    PROLOGUE();

    std::filesystem::create_directories(g_config.rootfs_path / "temp1" / "temp1_a" / "temp1_a_a");
    symlinkat("../temp1/temp1_a/temp1_a_a", g_rootfs_fd, "temp1/link1");

    {
        FdPath fd_path = Filesystem::resolve("/temp1/link1", true);
        CATCH_INFO(fmt::format("Error: {}", strerror(fd_path.get_errno())));
        CATCH_REQUIRE(!fd_path.is_error());
        CATCH_REQUIRE(fd_path.fd() == g_rootfs_fd);
        CATCH_REQUIRE(std::string(fd_path.path()) == std::filesystem::path("temp1") / "temp1_a" / "temp1_a_a");
    }

    EPILOGUE();
}

CATCH_TEST_CASE("ResolveNull", "[paths]") {
    PROLOGUE();

    FdPath fd_path = Filesystem::resolve(AT_FDCWD, nullptr, false);
    CATCH_INFO(fmt::format("Error: {}", strerror(fd_path.get_errno())));
    CATCH_REQUIRE(!fd_path.is_error());
    CATCH_REQUIRE(fd_path.fd() == AT_FDCWD);
    CATCH_REQUIRE(fd_path.path() == nullptr);

    EPILOGUE();
}