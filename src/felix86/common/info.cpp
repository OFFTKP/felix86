#include "felix86/common/info.hpp"

extern const char* g_git_hash;

const char* get_version_full() {
    static std::string version = "felix86 25.05 (" + std::string(g_git_hash) + ")";
    return version.c_str();
}
