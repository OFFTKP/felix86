#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "felix86/common/utility.h"

typedef struct {
    char* argv[256];
    int argc;
    char** envp;
    int envc;
    bool use_host_envs;
    bool print_blocks;
    bool use_interpreter;
    bool dont_optimize;
    const char* squashfs_path;
} loader_config_t;

void loader_run_elf(loader_config_t* config);

#ifdef __cplusplus
}
#endif