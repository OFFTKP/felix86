#include <catch2/catch_test_macros.hpp>
#include <fcntl.h>
#include "felix86/hle/filesystem.hpp"
#include "fmt/format.h"

#define SUCCESS_MESSAGE() SUCCESS("Test passed: %s", Catch::getResultCapture().getCurrentTestName().c_str())

#define PROLOGUE()                                                                                                                                   \
    Config config = g_config;                                                                                                                        \
    int fd = g_rootfs_fd;                                                                                                                            \
    g_config.rootfs_path = "/tmp/felix86_paths/myrootfs";                                                                                            \
    g_process_globals.mount_paths.push_back(g_config.rootfs_path);                                                                                   \
    std::filesystem::create_directories(g_config.rootfs_path);                                                                                       \
    g_rootfs_fd = open(g_config.rootfs_path.c_str(), O_PATH | O_DIRECTORY);                                                                          \
    ASSERT(g_rootfs_fd > 0)

#define EPILOGUE()                                                                                                                                   \
    ASSERT(close(g_rootfs_fd) == 0);                                                                                                                 \
    g_config = config;                                                                                                                               \
    g_process_globals.mount_paths.clear();                                                                                                           \
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

CATCH_TEST_CASE("Getcwd trusted", "[paths]") {
    PROLOGUE();

    // When real cwd is outside the rootfs (such as /tmp/felix86_paths/trusted in this case)
    // but it's a trusted dir, we transform it to the equivalent that is inside the rootfs
    g_fake_mounts.clear();
    std::filesystem::path trusted_dir = std::filesystem::path("/tmp/felix86_paths") / "trusted";
    std::filesystem::create_directories(trusted_dir);
    CATCH_REQUIRE(Filesystem::TrustFolder(trusted_dir));
    CATCH_REQUIRE(g_fake_mounts.size() == 1);
    char buffer[PATH_MAX];
    const char* old_cwd = getcwd(buffer, PATH_MAX);
    CATCH_REQUIRE(chdir(trusted_dir.c_str()) == 0); // this is what happens when you run something inside a trusted dir

    char buffer2[PATH_MAX];
    int result = Filesystem::Getcwd(buffer2, PATH_MAX);
    CATCH_REQUIRE(result > 0);
    std::filesystem::path emulated_cwd = buffer2;
    std::filesystem::path trusted_dst = g_fake_mounts[0].dst_path;
    CATCH_REQUIRE(emulated_cwd == trusted_dst);
    g_fake_mounts.clear();
    CATCH_REQUIRE(chdir(old_cwd) == 0);

    EPILOGUE();
}

CATCH_TEST_CASE("Getcwd trusted 2", "[paths]") {
    PROLOGUE();

    g_fake_mounts.clear();
    std::filesystem::path trusted_dir = std::filesystem::path("/tmp/felix86_paths") / "trusted";
    std::filesystem::create_directories(trusted_dir / "subdir1" / "subdir2");
    CATCH_REQUIRE(Filesystem::TrustFolder(trusted_dir));
    CATCH_REQUIRE(g_fake_mounts.size() == 1);
    char buffer[PATH_MAX];
    const char* old_cwd = getcwd(buffer, PATH_MAX);
    CATCH_REQUIRE(chdir((trusted_dir / "subdir1" / "subdir2").c_str()) == 0); // this is what happens when you run something inside a trusted dir

    char buffer2[PATH_MAX];
    int result = Filesystem::Getcwd(buffer2, PATH_MAX);
    CATCH_REQUIRE(result > 0);
    std::filesystem::path emulated_cwd = buffer2;
    std::filesystem::path trusted_dst = g_fake_mounts[0].dst_path;
    CATCH_REQUIRE(emulated_cwd == trusted_dst / "subdir1" / "subdir2");
    g_fake_mounts.clear();
    CATCH_REQUIRE(chdir(old_cwd) == 0);

    EPILOGUE();
}

CATCH_TEST_CASE("Trusted escape", "[paths]") {
    PROLOGUE();

    g_fake_mounts.clear();
    CATCH_REQUIRE(Filesystem::FakeMount("/run", g_config.rootfs_path / "run"));
    std::filesystem::path trusted_dir = std::filesystem::path("/tmp/felix86_paths") / "trusted";
    std::filesystem::create_directories(trusted_dir / "subdir1" / "subdir2");
    CATCH_REQUIRE(Filesystem::TrustFolder(trusted_dir));

    FakeMountNode node = g_fake_mounts[1];
    FdPath resolved = Filesystem::resolve((node.dst_path / "subdir1").c_str(), true);
    CATCH_REQUIRE(!resolved.is_error());
    CATCH_REQUIRE(resolved.full_path() == trusted_dir / "subdir1");

    FdPath resolved2 = Filesystem::resolve((node.dst_path / "subdir1" / ".." / "..").c_str(), true);
    CATCH_REQUIRE(!resolved2.is_error());

    // TODO: remove trailing slash
    CATCH_REQUIRE(resolved2.full_path() == node.dst_path.parent_path() / "");

    g_fake_mounts.clear();

    EPILOGUE();
}

CATCH_TEST_CASE("Trusted escape 2", "[paths]") {
    PROLOGUE();

    g_fake_mounts.clear();
    CATCH_REQUIRE(Filesystem::FakeMount("/run", g_config.rootfs_path / "run"));
    std::filesystem::path trusted_dir = std::filesystem::path("/tmp/felix86_paths") / "trusted";
    std::filesystem::create_directories(trusted_dir / "subdir1" / "subdir2");
    CATCH_REQUIRE(Filesystem::TrustFolder(trusted_dir));

    FakeMountNode node = g_fake_mounts[1];
    FdPath resolved = Filesystem::resolve((node.dst_path / "subdir1").c_str(), true);
    CATCH_REQUIRE(!resolved.is_error());
    CATCH_REQUIRE(resolved.full_path() == trusted_dir / "subdir1");

    FdPath resolved2 = Filesystem::resolve((node.dst_path / "subdir1" / ".." / ".." / ".." / ".." / "..").c_str(), true);
    CATCH_REQUIRE(!resolved2.is_error());

    // TODO: remove trailing slash
    CATCH_REQUIRE(resolved2.full_path() == std::string("/run/user/1000/"));

    FdPath resolved3 = Filesystem::resolve((node.dst_path / "subdir1" / ".." / ".." / ".." / ".." / ".." / ".." / ".." / "..").c_str(), true);
    CATCH_REQUIRE(!resolved3.is_error());

    // TODO: remove trailing slash
    CATCH_REQUIRE(resolved3.full_path() == g_config.rootfs_path / "");

    FdPath resolved4 = Filesystem::resolve((node.dst_path / "subdir1" / ".." / ".." / ".." / ".." / ".." / ".." / ".." / ".." / "..").c_str(), true);
    CATCH_REQUIRE(!resolved4.is_error());

    // TODO: remove trailing slash
    CATCH_REQUIRE(resolved4.full_path() == g_config.rootfs_path / "");

    g_fake_mounts.clear();

    EPILOGUE();
}

CATCH_TEST_CASE("Escape root", "[paths]") {
    PROLOGUE();

    std::filesystem::create_directories(g_config.rootfs_path / "subdir");

    {
        FdPath resolved = Filesystem::resolve("/subdir", true);
        CATCH_REQUIRE(!resolved.is_error());
        CATCH_REQUIRE(resolved.full_path() == g_config.rootfs_path / "subdir");
    }

    {
        FdPath resolved = Filesystem::resolve("/subdir/..", true);
        CATCH_REQUIRE(!resolved.is_error());

        // TODO: remove trailing slash
        CATCH_REQUIRE(resolved.full_path() == g_config.rootfs_path / "");
    }

    {
        FdPath resolved = Filesystem::resolve("/subdir/../../..", true);
        CATCH_REQUIRE(!resolved.is_error());

        // TODO: remove trailing slash
        CATCH_REQUIRE(resolved.full_path() == g_config.rootfs_path / "");
    }

    {
        g_fake_mounts.clear();
        CATCH_REQUIRE(Filesystem::FakeMount("/proc", g_config.rootfs_path / "proc"));
        FdPath resolved = Filesystem::resolve("/proc", true);
        CATCH_REQUIRE(!resolved.is_error());

        // TODO: remove trailing slash
        CATCH_REQUIRE(resolved.full_path() == std::string("/proc/"));
        g_fake_mounts.clear();
    }

    {
        g_fake_mounts.clear();
        CATCH_REQUIRE(Filesystem::FakeMount("/proc", g_config.rootfs_path / "proc"));
        FdPath resolved = Filesystem::resolve("/proc/..", true);
        CATCH_REQUIRE(!resolved.is_error());

        // TODO: remove trailing slash
        CATCH_REQUIRE(resolved.full_path() == g_config.rootfs_path / "");
        g_fake_mounts.clear();
    }

    {
        g_fake_mounts.clear();
        CATCH_REQUIRE(Filesystem::FakeMount("/proc", g_config.rootfs_path / "proc"));
        FdPath resolved = Filesystem::resolve("/proc/../../../..", true);
        CATCH_REQUIRE(!resolved.is_error());

        // TODO: remove trailing slash
        CATCH_REQUIRE(resolved.full_path() == g_config.rootfs_path / "");
        g_fake_mounts.clear();
    }

    {
        g_fake_mounts.clear();
        CATCH_REQUIRE(Filesystem::FakeMount("/proc", g_config.rootfs_path / "proc"));
        FdPath resolved = Filesystem::resolve("/proc/../proc/../..", true);
        CATCH_REQUIRE(!resolved.is_error());

        // TODO: remove trailing slash
        CATCH_REQUIRE(resolved.full_path() == g_config.rootfs_path / "");
        g_fake_mounts.clear();
    }

    EPILOGUE();
}

CATCH_TEST_CASE("Proc self ns user", "[paths]") {
    PROLOGUE();

    {
        g_fake_mounts.clear();
        g_config.verbose = 1;
        CATCH_REQUIRE(Filesystem::FakeMount("/proc", g_config.rootfs_path / "proc"));
        FdPath resolved = Filesystem::resolve("/proc/self/ns/user", true);
        CATCH_REQUIRE(!resolved.is_error());

        // TODO: remove trailing slash
        CATCH_REQUIRE(resolved.full_path() == std::string("/proc/") + std::to_string(getpid()) + "/ns/user");
        g_fake_mounts.clear();
    }

    EPILOGUE();
}