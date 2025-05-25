#include <cstring>
#include "felix86/common/log.hpp"
#include "felix86/common/utility.hpp"

#define __user
#include "headers/drm.h"
#undef __user

struct x86_drm_version {
    u32 version_major;
    u32 version_minor;
    u32 version_patchlevel;
    u32 name_len;
    u32 name;
    u32 date_len;
    u32 date;
    u32 desc_len;
    u32 desc;

    x86_drm_version() = delete;

    x86_drm_version(const drm_version& host) {
        this->version_major = host.version_major;
        this->version_minor = host.version_minor;
        this->version_patchlevel = host.version_patchlevel;
        this->name_len = host.name_len;
        this->name = (u64)host.name;
        this->date_len = host.date_len;
        this->date = (u64)host.date;
        this->desc_len = host.desc_len;
        this->desc = (u64)host.desc;
    }

    operator drm_version() {
        drm_version ret;
        ret.version_major = this->version_major;
        ret.version_minor = this->version_minor;
        ret.version_patchlevel = this->version_patchlevel;
        ret.name_len = this->name_len;
        ret.name = (decltype(ret.name))(u64)this->name;
        ret.date_len = this->date_len;
        ret.date = (decltype(ret.date))(u64)this->date;
        ret.desc_len = this->desc_len;
        ret.desc = (decltype(ret.desc))(u64)this->desc;
        return ret;
    }
};

struct x86_drm_unique {
    u32 unique_len;
    u32 unique;

    x86_drm_unique() = delete;

    x86_drm_unique(const drm_unique& host) {
        this->unique_len = host.unique_len;
        this->unique = (u64)host.unique;
    }

    operator drm_unique() {
        drm_unique ret;
        ret.unique_len = this->unique_len;
        ret.unique = (decltype(ret.unique))(u64)this->unique;
        return ret;
    }
};

struct x86_drm_map {
    u32 offset;
    u32 size;
    drm_map_type type;
    drm_map_flags flags;
    u32 handle;
    u32 mtrr;

    x86_drm_map() = delete;

    x86_drm_map(const drm_map& host) {
        this->offset = host.offset;
        this->size = host.size;
        this->type = host.type;
        this->flags = host.flags;
        this->handle = (u64)host.handle;
        this->mtrr = host.mtrr;
    }

    operator drm_map() {
        drm_map ret;
        ret.offset = this->offset;
        ret.size = this->size;
        ret.type = this->type;
        ret.flags = this->flags;
        ret.handle = (decltype(ret.handle))(u64)this->handle;
        ret.mtrr = this->mtrr;
        return ret;
    }
};

struct x86_drm_client {
    u32 idx;
    u32 auth;
    u32 pid;
    u32 uid;
    u32 magic;
    u32 iocs;

    x86_drm_client(const drm_client& host) {
        this->idx = host.idx;
        this->auth = host.auth;
        this->pid = host.pid;
        this->uid = host.uid;
        this->magic = host.magic;
        this->iocs = host.iocs;
    }

    operator drm_client() {
        drm_client ret;
        ret.idx = this->idx;
        ret.auth = this->auth;
        ret.pid = this->pid;
        ret.uid = this->uid;
        ret.magic = this->magic;
        ret.iocs = this->iocs;
        return ret;
    }
};

struct x86_drm_stats {
    u32 count;
    struct {
        u32 value;
        drm_stat_type type;
    } data[15];

    x86_drm_stats() = delete;

    x86_drm_stats(const drm_stats& host) {
        this->count = host.count;
        for (int i = 0; i < 15; i++) {
            this->data[i].value = host.data[i].value;
            this->data[i].type = host.data[i].type;
        }
    }

    operator drm_stats() {
        drm_stats ret;
        ret.count = this->count;
        for (int i = 0; i < 15; i++) {
            ret.data[i].value = this->data[i].value;
            ret.data[i].type = this->data[i].type;
        }
        return ret;
    }
};

struct x86_drm_buf_desc {
    u32 count;
    u32 size;
    u32 low_mark;
    u32 high_mark;
    u32 flags;
    u32 agp_start;

    x86_drm_buf_desc() = delete;

    x86_drm_buf_desc(const drm_buf_desc& host) {
        this->count = host.count;
        this->size = host.size;
        this->low_mark = host.low_mark;
        this->high_mark = host.high_mark;
        this->flags = host.flags;
        this->agp_start = host.agp_start;
    }

    operator drm_buf_desc() {
        drm_buf_desc ret;
        ret.count = this->count;
        ret.size = this->size;
        ret.low_mark = this->low_mark;
        ret.high_mark = this->high_mark;
        ret.flags = (decltype(ret.flags))this->flags;
        ret.agp_start = this->agp_start;
        return ret;
    }
};

struct x86_drm_buf_info {
    u32 count;
    u32 list;

    x86_drm_buf_info() = delete;

    x86_drm_buf_info(const drm_buf_info& host) {
        this->count = host.count;
        this->list = (u64)host.list;
    }

    operator drm_buf_info() {
        drm_buf_info ret;
        ret.count = this->count;
        ret.list = (decltype(ret.list))(u64)this->list;
        return ret;
    }
};

struct x86_drm_buf_free {
    u32 count;
    u32 list;

    x86_drm_buf_free() = delete;

    x86_drm_buf_free(const drm_buf_free& host) {
        this->count = host.count;
        this->list = (u64)host.list;
    }

    operator drm_buf_free() {
        drm_buf_free ret;
        ret.count = this->count;
        ret.list = (decltype(ret.list))(u64)this->list;
        return ret;
    }
};

struct x86_drm_buf_map {
    u32 count;
    u32 virt;
    u32 list;

    x86_drm_buf_map() = delete;

    x86_drm_buf_map(const drm_buf_map& host) {
        this->count = host.count;
        this->virt = (u64)host.virt;
        this->list = (u64)host.list;
    }

    operator drm_buf_map() {
        drm_buf_map ret;
        ret.count = this->count;
        ret.virt = (decltype(ret.virt))(u64)this->virt;
        ret.list = (decltype(ret.list))(u64)this->list;

        // drm_buf_map::list is a drm_buf_pub*, could have slightly different layout for example if the last void* is not followed
        // by zeroes in memory then reading it as a u64 would cause garbage in the upper bits
        u32 upper32;
        ::memcpy(&upper32, (u8*)ret.list->address + 4, 4);
        ASSERT(upper32 == 0);

        return ret;
    }
};

struct x86_drm_ctx_priv_map {
    u32 ctx_id;
    u32 handle;

    x86_drm_ctx_priv_map() = delete;

    x86_drm_ctx_priv_map(const drm_ctx_priv_map& host) {
        this->ctx_id = host.ctx_id;
        this->handle = (u64)host.handle;
    }

    operator drm_ctx_priv_map() {
        drm_ctx_priv_map ret;
        ret.ctx_id = this->ctx_id;
        ret.handle = (decltype(ret.handle))(u64)this->handle;
        return ret;
    }
};

struct x86_drm_ctx_res {
    u32 count;
    u32 contexts;

    x86_drm_ctx_res() = delete;

    x86_drm_ctx_res(const drm_ctx_res& host) {
        this->count = host.count;
        this->contexts = (u64)host.contexts;
    }

    operator drm_ctx_res() {
        drm_ctx_res ret;
        ret.count = this->count;
        ret.contexts = (decltype(ret.contexts))(u64)this->contexts;
        return ret;
    }
};

struct x86_drm_dma {
    u32 context;
    u32 send_count;
    u32 send_indices;
    u32 send_sizes;
    drm_dma_flags flags;
    u32 request_count;
    u32 request_size;
    u32 request_indices;
    u32 request_sizes;
    u32 granted_count;

    x86_drm_dma() = delete;

    x86_drm_dma(const drm_dma& host) {
        this->context = host.context;
        this->send_count = host.send_count;
        this->send_indices = (u64)host.send_indices;
        this->send_sizes = (u64)host.send_sizes;
        this->flags = host.flags;
        this->request_count = host.request_count;
        this->request_size = host.request_size;
        this->request_indices = (u64)host.request_indices;
        this->request_sizes = (u64)host.request_sizes;
        this->granted_count = host.granted_count;
    }

    operator drm_dma() {
        drm_dma ret;
        ret.context = this->context;
        ret.send_count = this->send_count;
        ret.send_indices = (decltype(ret.send_indices))(u64)this->send_indices;
        ret.send_sizes = (decltype(ret.send_sizes))(u64)this->send_sizes;
        ret.flags = this->flags;
        ret.request_count = this->request_count;
        ret.request_size = this->request_size;
        ret.request_indices = (decltype(ret.request_indices))(u64)this->request_indices;
        ret.request_sizes = (decltype(ret.request_sizes))(u64)this->request_sizes;
        ret.granted_count = this->granted_count;
        return ret;
    }
};

struct x86_drm_scatter_gather {
    u32 size;
    u32 handle;

    x86_drm_scatter_gather() = delete;

    x86_drm_scatter_gather(const drm_scatter_gather& host) {
        this->size = host.size;
        this->handle = host.handle;
    }

    operator drm_scatter_gather() {
        drm_scatter_gather ret;
        ret.size = this->size;
        ret.handle = this->handle;
        return ret;
    }
};

struct x86_drm_wait_vblank_request {
    drm_vblank_seq_type type;
    u32 sequence;
    u32 signal;

    x86_drm_wait_vblank_request() = delete;

    x86_drm_wait_vblank_request(const drm_wait_vblank_request& host) {
        this->type = host.type;
        this->sequence = host.sequence;
        this->signal = host.signal;
    }

    operator drm_wait_vblank_request() {
        drm_wait_vblank_request ret;
        ret.type = this->type;
        ret.sequence = this->sequence;
        ret.signal = this->signal;
        return ret;
    }
};

struct x86_drm_wait_vblank {
    x86_drm_wait_vblank_request request;
    drm_wait_vblank_reply reply; // same structure

    x86_drm_wait_vblank() = delete;
};

struct __attribute__((packed)) x86_drm_update_draw {
    drm_drawable_t handle;
    u32 type;
    u32 num;
    u64 data;

    x86_drm_update_draw() = delete;

    x86_drm_update_draw(const drm_update_draw& host) {
        this->handle = host.handle;
        this->type = host.type;
        this->num = host.num;
        this->data = host.data;
    }

    operator drm_update_draw() {
        drm_update_draw ret;
        ret.handle = this->handle;
        ret.type = this->type;
        ret.num = this->num;
        ret.data = this->data;
        return ret;
    }
};

struct __attribute__((packed)) x86_drm_mode_fb_cmd2 {
    u32 fb_id;
    u32 width;
    u32 height;
    u32 pixel_format;
    u32 flags;
    u32 handles[4];
    u32 pitches[4];
    u32 offsets[4];
    u64 modifier[4];

    x86_drm_mode_fb_cmd2() = delete;

    x86_drm_mode_fb_cmd2(const drm_mode_fb_cmd2& host) {
        this->fb_id = host.fb_id;
        this->width = host.width;
        this->height = host.height;
        this->pixel_format = host.pixel_format;
        this->flags = host.flags;
        for (int i = 0; i < 4; i++) {
            this->handles[i] = host.handles[i];
            this->pitches[i] = host.pitches[i];
            this->offsets[i] = host.offsets[i];
            this->modifier[i] = host.modifier[i];
        }
    }

    operator drm_mode_fb_cmd2() {
        drm_mode_fb_cmd2 ret;
        ret.fb_id = this->fb_id;
        ret.width = this->width;
        ret.height = this->height;
        ret.pixel_format = this->pixel_format;
        ret.flags = this->flags;
        for (int i = 0; i < 4; i++) {
            ret.handles[i] = this->handles[i];
            ret.pitches[i] = this->pitches[i];
            ret.offsets[i] = this->offsets[i];
            ret.modifier[i] = this->modifier[i];
        }
        return ret;
    }
};

int ioctl32_drm(int fd, u32 cmd, u32 args);
