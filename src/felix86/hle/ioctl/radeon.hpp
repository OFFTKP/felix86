#include "felix86/common/utility.hpp"

#define __user
#include "headers/radeon_drm.h"
#undef __user

struct x86_drm_radeon_init_t {
    u32 func;
    u32 sarea_priv_offset;
    u32 is_pci;
    u32 cp_mode;
    u32 gart_size;
    u32 ring_size;
    u32 usec_timeout;
    u32 fb_bpp;
    u32 front_offset;
    u32 front_pitch;
    u32 back_offset;
    u32 back_pitch;
    u32 depth_bpp;
    u32 depth_offset;
    u32 depth_pitch;
    u32 fb_offset;
    u32 mmio_offset;
    u32 ring_offset;
    u32 ring_rptr_offset;
    u32 buffers_offset;
    u32 gart_textures_offset;

    x86_drm_radeon_init_t() = delete;

    x86_drm_radeon_init_t(const drm_radeon_init_t& host) {
        this->func = host.func;
        this->sarea_priv_offset = host.sarea_priv_offset;
        this->is_pci = host.is_pci;
        this->cp_mode = host.cp_mode;
        this->gart_size = host.gart_size;
        this->ring_size = host.ring_size;
        this->usec_timeout = host.usec_timeout;
        this->fb_bpp = host.fb_bpp;
        this->front_offset = host.front_offset;
        this->front_pitch = host.front_pitch;
        this->back_offset = host.back_offset;
        this->back_pitch = host.back_pitch;
        this->depth_bpp = host.depth_bpp;
        this->depth_offset = host.depth_offset;
        this->depth_pitch = host.depth_pitch;
        this->fb_offset = host.fb_offset;
        this->mmio_offset = host.mmio_offset;
        this->ring_offset = host.ring_offset;
        this->ring_rptr_offset = host.ring_rptr_offset;
        this->buffers_offset = host.buffers_offset;
        this->gart_textures_offset = host.gart_textures_offset;
    }

    operator drm_radeon_init_t() {
        drm_radeon_init_t ret;
        ret.func = (decltype(ret.func))this->func;
        ret.sarea_priv_offset = this->sarea_priv_offset;
        ret.is_pci = this->is_pci;
        ret.cp_mode = this->cp_mode;
        ret.gart_size = this->gart_size;
        ret.ring_size = this->ring_size;
        ret.usec_timeout = this->usec_timeout;
        ret.fb_bpp = this->fb_bpp;
        ret.front_offset = this->front_offset;
        ret.front_pitch = this->front_pitch;
        ret.back_offset = this->back_offset;
        ret.back_pitch = this->back_pitch;
        ret.depth_bpp = this->depth_bpp;
        ret.depth_offset = this->depth_offset;
        ret.depth_pitch = this->depth_pitch;
        ret.fb_offset = this->fb_offset;
        ret.mmio_offset = this->mmio_offset;
        ret.ring_offset = this->ring_offset;
        ret.ring_rptr_offset = this->ring_rptr_offset;
        ret.buffers_offset = this->buffers_offset;
        ret.gart_textures_offset = this->gart_textures_offset;
        return ret;
    }
};

struct x86_drm_radeon_clear_t {
    u32 flags;
    u32 clear_color;
    u32 clear_depth;
    u32 color_mask;
    u32 depth_mask;
    u32 depth_boxes;

    x86_drm_radeon_clear_t() = delete;

    x86_drm_radeon_clear_t(const drm_radeon_clear_t& host) {
        this->flags = host.flags;
        this->clear_color = host.clear_color;
        this->clear_depth = host.clear_depth;
        this->color_mask = host.color_mask;
        this->depth_mask = host.depth_mask;
        this->depth_boxes = (u64)host.depth_boxes;
    }

    operator drm_radeon_clear_t() {
        drm_radeon_clear_t ret;
        ret.flags = this->flags;
        ret.clear_color = this->clear_color;
        ret.clear_depth = this->clear_depth;
        ret.color_mask = this->color_mask;
        ret.depth_mask = this->depth_mask;
        ret.depth_boxes = (decltype(ret.depth_boxes))(u64)this->depth_boxes;
        return ret;
    }
};

struct x86_drm_radeon_stipple_t {
    u32 mask;

    x86_drm_radeon_stipple_t() = delete;

    x86_drm_radeon_stipple_t(const drm_radeon_stipple_t& host) {
        this->mask = (u64)host.mask;
    }

    operator drm_radeon_stipple_t() {
        drm_radeon_stipple_t ret;
        ret.mask = (decltype(ret.mask))(u64)this->mask;
        return ret;
    }
};

struct x86_drm_radeon_texture_t {
    u32 offset;
    u32 pitch;
    u32 format;
    u32 width;
    u32 height;
    u32 image;

    x86_drm_radeon_texture_t() = delete;

    x86_drm_radeon_texture_t(const drm_radeon_texture_t& host) {
        this->offset = host.offset;
        this->pitch = host.pitch;
        this->format = host.format;
        this->width = host.width;
        this->height = host.height;
        this->image = (u64)host.image;
    }

    operator drm_radeon_texture_t() {
        drm_radeon_texture_t ret;
        ret.offset = this->offset;
        ret.pitch = this->pitch;
        ret.format = this->format;
        ret.width = this->width;
        ret.height = this->height;
        ret.image = (decltype(ret.image))(u64)this->image;
        return ret;
    }
};

struct x86_drm_radeon_vertex2_t {
    u32 idx;
    u32 discard;
    u32 nr_states;
    u32 state;
    u32 nr_prims;
    u32 prim;

    x86_drm_radeon_vertex2_t() = delete;

    x86_drm_radeon_vertex2_t(const drm_radeon_vertex2_t& host) {
        this->idx = host.idx;
        this->discard = host.discard;
        this->nr_states = host.nr_states;
        this->state = (u64)host.state;
        this->nr_prims = host.nr_prims;
        this->prim = (u64)host.prim;
    }

    operator drm_radeon_vertex2_t() {
        drm_radeon_vertex2_t ret;
        ret.idx = this->idx;
        ret.discard = this->discard;
        ret.nr_states = this->nr_states;
        ret.state = (decltype(ret.state))(u64)this->state;
        ret.nr_prims = this->nr_prims;
        ret.prim = (decltype(ret.prim))(u64)this->prim;
        return ret;
    }
};

struct x86_drm_radeon_cmd_buffer_t {
    u32 bufsz;
    u32 buf;
    u32 nbox;
    u32 boxes;

    x86_drm_radeon_cmd_buffer_t() = delete;

    x86_drm_radeon_cmd_buffer_t(const drm_radeon_cmd_buffer_t& host) {
        this->bufsz = host.bufsz;
        this->buf = (u64)host.buf;
        this->nbox = host.nbox;
        this->boxes = (u64)host.boxes;
    }

    operator drm_radeon_cmd_buffer_t() {
        drm_radeon_cmd_buffer_t ret;
        ret.bufsz = this->bufsz;
        ret.buf = (decltype(ret.buf))(u64)this->buf;
        ret.nbox = this->nbox;
        ret.boxes = (decltype(ret.boxes))(u64)this->boxes;
        return ret;
    }
};

struct x86_drm_radeon_getparam_t {
    u32 param;
    u32 value;

    x86_drm_radeon_getparam_t() = delete;

    x86_drm_radeon_getparam_t(const drm_radeon_getparam_t& host) {
        this->param = host.param;
        this->value = (u64)host.value;
    }

    operator drm_radeon_getparam_t() {
        drm_radeon_getparam_t ret;
        ret.param = this->param;
        ret.value = (decltype(ret.value))(u64)this->value;
        return ret;
    }
};

struct x86_drm_radeon_mem_alloc_t {
    u32 region;
    u32 alignment;
    u32 size;
    u32 region_offset;

    x86_drm_radeon_mem_alloc_t() = delete;

    x86_drm_radeon_mem_alloc_t(const drm_radeon_mem_alloc_t& host) {
        this->region = host.region;
        this->alignment = host.alignment;
        this->size = host.size;
        this->region_offset = (u64)host.region_offset;
    }

    operator drm_radeon_mem_alloc_t() {
        drm_radeon_mem_alloc_t ret;
        ret.region = this->region;
        ret.alignment = this->alignment;
        ret.size = this->size;
        ret.region_offset = (decltype(ret.region_offset))(u64)this->region_offset;
        return ret;
    }
};

struct x86_drm_radeon_irq_emit_t {
    u32 irq_seq;

    x86_drm_radeon_irq_emit_t() = delete;

    x86_drm_radeon_irq_emit_t(const drm_radeon_irq_emit_t& host) {
        this->irq_seq = (u64)host.irq_seq;
    }

    operator drm_radeon_irq_emit_t() {
        drm_radeon_irq_emit_t ret;
        ret.irq_seq = (decltype(ret.irq_seq))(u64)this->irq_seq;
        return ret;
    }
};

struct __attribute__((packed)) x86_drm_radeon_setparam_t {
    u32 param;
    u64 value;

    x86_drm_radeon_setparam_t() = delete;

    x86_drm_radeon_setparam_t(const drm_radeon_setparam_t& host) {
        this->param = host.param;
        this->value = host.value;
    }

    operator drm_radeon_setparam_t() {
        drm_radeon_setparam_t ret;
        ret.param = this->param;
        ret.value = this->value;
        return ret;
    }
};

int ioctl32_radeon(int fd, u32 cmd, u32 args);
