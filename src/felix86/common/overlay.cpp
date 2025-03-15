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
    overlays.push_back({lib_name, dest});
}

const char* Overlays::isOverlay(int fd, const char* pathname) {
    // char path[PATH_MAX];
    // int end = readlinkat(fd, pathname, path, PATH_MAX);
    // ASSERT(end > 0);
    // path[end] = 0;

    // for (auto& entry : overlays) {
    //     if (path == entry.real_path) {
    //         VERBOSE("Found overlay %s (%d) -> %s", pathname, fd, entry.real_path.c_str());
    //         return entry.overlayed_path.c_str();
    //     }
    // }

    // return nullptr;

    return nullptr;
}