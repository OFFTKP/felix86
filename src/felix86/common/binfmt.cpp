#include <filesystem>
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
