#pragma once

#include <linux/limits.h>
#include "felix86/common/utility.hpp"

extern char squashfs_path[PATH_MAX];

int felix86_lexically_normal(char* buffer, u32 buffer_size, const char* path);

void felix86_fs_init(const char* squashfs_path, const char* executable_path);
void felix86_fs_cleanup();
u32 felix86_fs_readlinkat(u32 dirfd, const char* pathname, char* buf, u32 bufsiz);
