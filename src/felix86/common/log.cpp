#include "felix86/common/log.hpp"

void enable_verbose() {
    g_verbose = true;
    VERBOSE("Verbose output enabled");
}

void disable_logging() {
    g_quiet = true;
}