#include <string>
#include "felix86/common/info.hpp"

extern const char* g_git_hash;

#define YEAR "26"
#define MONTH "03"

const char* get_version_full() {
#ifdef FELIX86_MONTHLY_RELEASE
    static std::string version = "felix86 " YEAR "." MONTH + std::string(" (release)");
#else
    static std::string version = "felix86 " YEAR "." MONTH + (std::string(g_git_hash) == "?" ? "" : " (" + std::string(g_git_hash) + ")");
#endif
    return version.c_str();
}
