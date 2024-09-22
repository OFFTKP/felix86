#include <stdlib.h>
#include "felix86/common/exit.h"
#include "felix86/hle/filesystem.h"

void felix86_exit(int code) {
    felix86_fs_cleanup();
    exit(code);
}