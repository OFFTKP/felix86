#include <filesystem>
#include <vector>
#include <fcntl.h>
#include <fmt/format.h>
#include <spawn.h>
#include <sys/wait.h>
#include "felix86/common/binfmt.hpp"
#include "felix86/common/config.hpp"
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/sudo.hpp"

bool unregister_binfmt_misc(const std::string& name) {
    ASSERT(!name.empty());

    // These are the directories systemd looks in
    std::vector<std::filesystem::path> dirs = {
        "/etc/binfmt.d",
        "/run/binfmt.d",
        "/usr/local/lib/binfmt.d",
        "/usr/lib/binfmt.d",
    };

    for (auto& dir : dirs) {
        std::error_code ec;
        std::filesystem::path path = dir / (name + ".conf");
        if (std::filesystem::exists(path, ec)) {
            std::filesystem::remove(path);
        }
    }

    std::filesystem::path path = std::filesystem::path("/proc/sys/fs/binfmt_misc") / name;
    if (!std::filesystem::exists(path)) {
        return false;
    }

    FILE* fp = fopen(path.c_str(), "w");
    if (!fp) {
        return false;
    }

    if (fwrite("-1", 1, 2, fp) != 2) {
        fclose(fp);
        return false;
    }

    fclose(fp);
    return true;
}

bool detect_binfmt_misc() {
    // Run an x86-64 program and set __FELIX86_TEST_BINFMT_MISC which will make felix86 immediately return 0x42.
    // If the return value is 0x42 that means felix86 was invoked and thus binfmt_misc is correctly installed.
    // If anything else is returned it means we didn't run it through binfmt_misc thus it's not installed.
    std::error_code ec;
    std::filesystem::path path = g_config.rootfs_path / "bin/env";
    if (std::filesystem::exists(path, ec) && std::filesystem::is_regular_file(path, ec)) {
        pid_t pid;
        int status;

        std::vector<char*> envs;

        char** env = environ;
        while (*env) {
            envs.push_back(*env);
            env++;
        }

        char buf[256] = "__FELIX86_TEST_BINFMT_MISC=1";
        envs.push_back(buf);
        envs.push_back(nullptr);

        std::vector<const char*> args = {
            path.c_str(),
            nullptr,
        };

        int devnull = open("/dev/null", O_WRONLY);
        if (devnull == -1) {
            return false;
        }

        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_adddup2(&actions, devnull, STDOUT_FILENO);
        posix_spawn_file_actions_adddup2(&actions, devnull, STDERR_FILENO);

        if (posix_spawn(&pid, path.c_str(), &actions, NULL, (char**)args.data(), (char**)envs.data()) != 0) {
            WARN("posix_spawn failed: %d", errno);
            return false;
        }

        int exit_status = 0;
        if (waitpid(pid, &status, 0) == -1) {
            exit_status = -1;
        } else {
            if (WIFEXITED(status)) {
                exit_status = WEXITSTATUS(status);
            } else {
                exit_status = -1;
            }
        }

        close(devnull);
        posix_spawn_file_actions_destroy(&actions);

        // $ROOTFS/bin/env was run through felix86, thus binfmt_misc is installed
        return exit_status == 0x42;
    } else {
        WARN("rootfs/bin/env not found?");
        return false;
    }
}

void binfmt_misc(bool is_register) {
    if (geteuid() != 0) {
        printf("I need root permissions to register felix86 in binfmt_misc, please re-run with root permissions as `sudo -E felix86 -b`\n");
        exit(1);
    }

    std::string config_dir = Config::getConfigDir();
    if (config_dir == "/root") {
        WARN("Config dir is /root, did you forget to pass the environment variables to felix86? Re-run as `sudo --preserve-env=HOME felix86 -b` if "
             "this was not intended");
    }

    Config::initialize(true /* ignore envs, because we save the config later */);

    char exe_path[4096];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len == -1) {
        perror("readlink");
    }
    exe_path[len] = '\0';

    std::string registration_string_x64 = fmt::format(
        R"!(:felix86-x86_64:M:0:\x7fELF\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\x3e\x00:\xff\xff\xff\xff\xff\xfe\xfe\x00\x00\x00\x00\xff\xff\xff\xff\xff\xfe\xff\xff\xff:{}:OCF)!",
        exe_path);
    std::string registration_string_i386 = fmt::format(
        R"!(:felix86-i386:M:0:\x7fELF\x01\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\x03\x00:\xff\xff\xff\xff\xff\xfe\xfe\x00\x00\x00\x00\xff\xff\xff\xff\xff\xfe\xff\xff\xff:{}:OCF)!",
        exe_path);

    if (!is_register) {
        if (unregister_binfmt_misc("felix86-x86_64")) {
            printf("Unregistered felix86 from binfmt_misc for x86-64 apps\n");
        }

        if (unregister_binfmt_misc("felix86-i386")) {
            printf("Unregistered felix86 from binfmt_misc for i386 apps\n");
        }

        g_config.binfmt_misc_installed = false;
        Config::save(g_config.path(), g_config);
        printf("felix86 successfully unregistered from binfmt_misc\n");
    } else {
        // Unregister if already registered
        unregister_binfmt_misc("felix86-x86_64");
        unregister_binfmt_misc("felix86-i386");

        FILE* fp = fopen("/proc/sys/fs/binfmt_misc/register", "w");

        if (!fp) {
            ERROR("Failed to open /proc/sys/fs/binfmt_misc/register");
        }

        if (fwrite(registration_string_x64.c_str(), 1, registration_string_x64.size(), fp) != registration_string_x64.size()) {
            fclose(fp);
            ERROR("Failed to register for x86-64");
        }

        fclose(fp);

        fp = fopen("/proc/sys/fs/binfmt_misc/register", "w");

        if (fwrite(registration_string_i386.c_str(), 1, registration_string_i386.size(), fp) != registration_string_i386.size()) {
            fclose(fp);
            ERROR("Failed to register for i386");
        }

        fclose(fp);

        // /proc/sys/fs stuff makes it temporary, we need to install it here to make it permanent via systemd
        // Register it in the first available directory in this order
        std::vector<std::filesystem::path> dirs = {
            "/etc/binfmt.d",
            "/usr/lib/binfmt.d",
            "/usr/local/lib/binfmt.d",
            "/run/binfmt.d",
        };

        bool registered = false;
        for (auto& dir : dirs) {
            if (std::filesystem::exists(dir)) {
                {
                    std::filesystem::path x64path = dir / "felix86-x86_64.conf";
                    FILE* fp = fopen(x64path.c_str(), "w");
                    if (!fp) {
                        ERROR("Failed to open %s", x64path.c_str());
                    }
                    if (fwrite(registration_string_x64.c_str(), 1, registration_string_x64.size(), fp) != registration_string_x64.size()) {
                        fclose(fp);
                        ERROR("Failed to register in binfmt.d for x86-64");
                    }
                    fclose(fp);
                }
                {
                    std::filesystem::path i386path = dir / "felix86-i386.conf";
                    FILE* fp = fopen(i386path.c_str(), "w");
                    if (!fp) {
                        ERROR("Failed to open %s", i386path.c_str());
                    }
                    if (fwrite(registration_string_i386.c_str(), 1, registration_string_i386.size(), fp) != registration_string_i386.size()) {
                        fclose(fp);
                        ERROR("Failed to register in binfmt.d for i386");
                    }
                    fclose(fp);
                }

                registered = true;
                break;
            }
        }

        if (!registered) {
            printf("Failed to find a binfmt.d directory to put felix86.conf in\n");
        }

        unregister_binfmt_misc("qemu-x86_64");
        unregister_binfmt_misc("qemu-i386");

        g_config.binfmt_misc_installed = true;
        Config::save(g_config.path(), g_config);

        printf("felix86 successfully registered to binfmt_misc\n");
    }
}
