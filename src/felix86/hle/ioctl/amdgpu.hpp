#include <cstring>
#include "felix86/common/types.hpp"

#define __user
#include "headers/amdgpu_drm.h"
#undef __user

int ioctl32_amdgpu(int fd, u32 cmd, u32 args);
