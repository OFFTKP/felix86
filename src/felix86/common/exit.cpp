#include <stdlib.h>
#include "felix86/common/exit.hpp"
#include "felix86/hle/filesystem.hpp"

void felix86_exit(int code) {
    felix86_fs_cleanup();
    exit(code);
}