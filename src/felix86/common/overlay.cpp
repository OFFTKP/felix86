#include <fcntl.h>
#include "felix86/common/elf.hpp"
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/overlay.hpp"

struct Overlay {
    std::string lib_name;
    std::filesystem::path overlayed_path;
};

// Shouldn't need mutex, we only add to this during initialization and then only iterate it
std::vector<Overlay> overlays;

void Overlays::addOverlay(const char* lib_name, const std::filesystem::path& dest) {
    VERBOSE("Adding overlay %s -> %s", lib_name, dest.c_str());
    overlays.push_back({lib_name, dest});
}

const char* Overlays::isOverlay(int fd, const char* pathname) {
    std::filesystem::path path = pathname;
    if (path.is_relative()) {
        if (fd != AT_FDCWD) {
            path = std::filesystem::path("/proc/self/fd/" + std::to_string(fd)) / path;
        }
    } else {
        path = g_config.rootfs_path / path.relative_path();
    }
    std::string filename = path.filename();
    for (auto& entry : overlays) {
        if (filename == entry.lib_name) {
            Elf::PeekResult result = Elf::Peek(path);
            if ((g_mode32 && result == Elf::PeekResult::Elf32) || (!g_mode32 && result == Elf::PeekResult::Elf64)) {
                LOG("Found overlay %s -> %s", pathname, entry.overlayed_path.c_str());
                return entry.overlayed_path.c_str();
            }
        }
    }

    return nullptr;
}