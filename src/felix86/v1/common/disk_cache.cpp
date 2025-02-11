#include <algorithm>
#include <filesystem>
#include <fstream>
#include <vector>
#include "felix86/common/disk_cache.hpp"
#include "felix86/common/log.hpp"

std::vector<std::string> files;
std::filesystem::path functions_dir;

bool initialized = false;

void initialize() {
    if (initialized) {
        return;
    }

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

    if (!std::filesystem::exists(cache_dir)) {
        std::filesystem::create_directories(cache_dir);
    }

    functions_dir = cache_dir / "functions";
    if (!std::filesystem::exists(functions_dir)) {
        std::filesystem::create_directories(functions_dir);
    }

    for (const auto& entry : std::filesystem::directory_iterator(functions_dir)) {
        files.push_back(entry.path().filename().string());
    }

    initialized = true;
}

bool DiskCache::Has(const std::string& key) {
    initialize();
    return std::find(files.begin(), files.end(), key) != files.end();
}

std::vector<u8> DiskCache::Read(const std::string& key) {
    initialize();

    std::filesystem::path file_path = functions_dir / key;
    ASSERT(std::filesystem::exists(file_path));
    ASSERT(std::filesystem::is_regular_file(file_path));

    std::ifstream file(file_path, std::ios::binary);
    ASSERT(file.is_open());

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<u8> data;
    data.resize(size);
    file.read(reinterpret_cast<char*>(data.data()), size);

    return data;
}

void DiskCache::Write(const std::string& key, void* data, size_t size) {
    initialize();

    std::filesystem::path file_path = functions_dir / key;
    std::ofstream file(file_path, std::ios::binary);
    ASSERT(file.is_open());

    file.write(reinterpret_cast<char*>(data), size);
}

void DiskCache::Clear() {
    initialize();

    for (const auto& entry : std::filesystem::directory_iterator(functions_dir)) {
        std::filesystem::remove(entry.path());
    }
}