#include <cstring>
#include <filesystem>
#include "felix86/hle/filesystem.h"

extern "C" int felix86_lexically_normal(char* buffer, u32 buffer_size, const char* path) {
    std::filesystem::path normal_path = std::filesystem::path(path).lexically_normal();
    snprintf(buffer, buffer_size, "%s", normal_path.c_str());
    return normal_path.is_absolute();
}