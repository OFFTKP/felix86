#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include "felix86/common/disk_cache.hpp"
#include "felix86/common/log.hpp"

DiskCache::DiskCache(const std::string& hash, Backend& backend) {
    std::filesystem::path cache_dir;
    const char* cache_home = getenv("XDG_CACHE_HOME");
    if (cache_home == nullptr) {
        cache_home = getenv("HOME");
        if (cache_home == nullptr) {
            throw std::runtime_error("Could not find home directory");
        }
        cache_dir = std::filesystem::path(cache_home) / ".cache" / "felix86";
    } else {
        cache_dir = std::filesystem::path(cache_home) / "felix86";
    }

    cache_dir /= hash;

    if (!std::filesystem::exists(cache_dir)) {
        std::filesystem::create_directories(cache_dir);
    }

    if (!std::filesystem::exists(cache_dir / "metadata.json")) {
        std::ofstream metadata_file(cache_dir / "metadata.json");
        metadata_file << "{}";
    }

    nlohmann::json metadata = nlohmann::json::parse(std::ifstream(cache_dir / "metadata.json"));
    for (auto& [key, value] : metadata.items()) {
    }

    // The problem is loading functions for dynlibs too
    UNIMPLEMENTED();
}