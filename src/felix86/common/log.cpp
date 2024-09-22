#include "felix86/common/log.hpp"

bool verbose = false;
bool quiet = false;

void enable_verbose() {
    verbose = true;
    VERBOSE("Verbose output enabled");
}

void disable_logging() {
    quiet = true;
}