#include <fcntl.h>
#include "felix86/common/elf.hpp"
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/overlay.hpp"
#include "felix86/common/state.hpp"
#include "felix86/hle/filesystem.hpp"

struct Overlay {
    std::string lib_name;
    std::filesystem::path overlayed_path;
};

// Shouldn't need mutex, we only add to this during initialization and then only iterate it
static std::vector<Overlay> overlays;

void Overlays::addOverlay(const char* lib_name, const std::filesystem::path& dest) {
    VERBOSE("Adding overlay %s -> %s", lib_name, dest.c_str());
    overlays.push_back({lib_name, dest});
}

const char* Overlays::isOverlay(int fd, const char* pathname) {
    if (!pathname) {
        return nullptr;
    }

    std::string filename = std::filesystem::path(pathname).filename();
    if (filename.empty()) {
        return nullptr;
    }

    bool mode32 = ThreadState::Get()->ctx.Mode32();
    for (auto& entry : overlays) {
        if (filename == entry.lib_name) {
            FdPath fd_path = Filesystem::resolve(fd, pathname, true);
            if (fd_path.is_error()) {
                return nullptr;
            }

            Elf::PeekResult result = Elf::Peek(fd_path.full_path());
            if ((mode32 && result == Elf::PeekResult::Elf32) || (!mode32 && result == Elf::PeekResult::Elf64)) {
                LOG("Found overlay %s -> %s", pathname, entry.overlayed_path.c_str());
                return entry.overlayed_path.c_str();
            }
        }
    }

    return nullptr;
}
