#include <biscuit/cpuinfo.hpp>
#include "felix86/common/info.hpp"

extern const char* g_git_hash;

using namespace biscuit;

const char* get_version_full() {
    static std::string version = "felix86 0.1.0." + std::string(g_git_hash);
    return version.c_str();
}
