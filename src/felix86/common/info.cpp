#include "felix86/common/info.hpp"

extern const char* g_git_hash;

#define YEAR "25"
#define MONTH "09"

const char* get_version_full() {
    static std::string version = "felix86 " YEAR "." MONTH + (std::string(g_git_hash) == "?" ? "" : " (" + std::string(g_git_hash) + ")");
    return version.c_str();
}
