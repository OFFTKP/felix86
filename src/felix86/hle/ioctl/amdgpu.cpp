#include <asm/ioctl.h>
#include <sys/ioctl.h>
#include "amdgpu.hpp"
#include "felix86/common/log.hpp"
#include "felix86/hle/ioctl/common.hpp"
#define __user
#include "headers/amdgpu_drm.h"

int ioctl32_amdgpu(int fd, u32 cmd, u32 args) {
    switch (_IOC_NR(cmd)) {
        MARSHAL_CASE(DRM_IOCTL_AMDGPU_GEM_METADATA, drm_amdgpu_gem_metadata);

        SIMPLE_CASE(DRM_IOCTL_AMDGPU_GEM_CREATE);
        SIMPLE_CASE(DRM_IOCTL_AMDGPU_GEM_MMAP);
        SIMPLE_CASE(DRM_IOCTL_AMDGPU_CTX);
        SIMPLE_CASE(DRM_IOCTL_AMDGPU_BO_LIST);
        SIMPLE_CASE(DRM_IOCTL_AMDGPU_CS);
        SIMPLE_CASE(DRM_IOCTL_AMDGPU_INFO);
        SIMPLE_CASE(DRM_IOCTL_AMDGPU_GEM_WAIT_IDLE);
        SIMPLE_CASE(DRM_IOCTL_AMDGPU_GEM_VA);
        SIMPLE_CASE(DRM_IOCTL_AMDGPU_WAIT_CS);
        SIMPLE_CASE(DRM_IOCTL_AMDGPU_GEM_OP);
        SIMPLE_CASE(DRM_IOCTL_AMDGPU_GEM_USERPTR);
        SIMPLE_CASE(DRM_IOCTL_AMDGPU_WAIT_FENCES);
        SIMPLE_CASE(DRM_IOCTL_AMDGPU_VM);
        SIMPLE_CASE(DRM_IOCTL_AMDGPU_FENCE_TO_HANDLE);
        SIMPLE_CASE(DRM_IOCTL_AMDGPU_SCHED);

    default: {
        ERROR("Unknown amdgpu ioctl cmd: %x", cmd);
        return -1;
    }
    }
}