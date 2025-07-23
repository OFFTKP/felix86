#include <cstring>
#include "felix86/common/types.hpp"

#define __user
#include "headers/amdgpu_drm.h"
#undef __user

struct x86_drm_amdgpu_gem_metadata {
    u32 handle;
    u32 op;
    struct {
        u64 flags;
        u64 tiling_info;
        u32 data_size_bytes;
        u32 data[64];
    } data;

    x86_drm_amdgpu_gem_metadata() = delete;

    operator drm_amdgpu_gem_metadata() const {
        drm_amdgpu_gem_metadata guest{};
        guest.handle = handle;
        guest.op = op;
        guest.data.flags = data.flags;
        guest.data.tiling_info = data.tiling_info;
        guest.data.data_size_bytes = data.data_size_bytes;
        memcpy(guest.data.data, data.data, sizeof(data.data));
        return guest;
    }

    x86_drm_amdgpu_gem_metadata(const struct drm_amdgpu_gem_metadata& host) {
        handle = host.handle;
        op = host.handle;
        data.flags = host.data.flags;
        data.tiling_info = host.data.tiling_info;
        data.data_size_bytes = host.data.data_size_bytes;
        memcpy(data.data, host.data.data, sizeof(data.data));
    }
};

int ioctl32_amdgpu(int fd, u32 cmd, u32 args);
