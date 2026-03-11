#pragma once

#include "felix86/common/types.hpp"

struct ThreadState;

struct XmmReg {
    u64 data[4] = {0, 0, 0, 0};
};
static_assert(sizeof(XmmReg) == 32);

struct Xmm128 {
    u64 val[2];

    Xmm128(const XmmReg& other) {
        val[0] = other.data[0];
        val[1] = other.data[1];
    }

    operator XmmReg() const {
        XmmReg ret{};
        ret.data[0] = val[0];
        ret.data[1] = val[1];
        return ret;
    }
};

typedef struct __attribute__((packed)) {
    u64 significand;
    u16 exponent;
} Float80;

struct fsave_frame_16 {
    u16 cw;
    u16 sw;
    u16 tw;
    u16 fip;
    u16 fcs;
    u16 fdp;
    u16 fds;
    Float80 st[8];
};
static_assert(sizeof(fsave_frame_16) == 94);

struct fsave_frame_32 {
    u16 cw = 0;
    alignas(u32) u16 sw = 0;
    alignas(u32) u16 tw = 0;
    u32 fip = 0;
    u32 unused = 0;
    u32 fdp = 0;
    u32 fds = 0;
    Float80 st[8];
};
static_assert(sizeof(fsave_frame_32) == 108);

struct x64_fpx_sw_bytes {
    u32 magic1;
    u32 extended_size;
    u64 xfeatures;
    u32 xstate_size;
    u32 padding[7];
};

struct fxsave_st {
    u8 st[10];
    u8 reserved[6];
};

struct fxsave_frame {
    u16 fcw;
    u16 fsw;
    u8 ftw;
    u8 reserved;
    u16 fop;
    u64 fip;
    u64 fdp;
    u32 mxcsr;
    u32 mxcsr_mask;
    fxsave_st st[8];
    Xmm128 xmms[16];
    u64 reserved_final[6];
    x64_fpx_sw_bytes sw_reserved;
};
static_assert(sizeof(fxsave_frame) == 512);

struct xsave_header {
    u64 xstate_bv;
    u64 xcomp_bv;
    u8 reserved[48];
};
static_assert(sizeof(xsave_header) == 64);

struct ymm_hi {
    u64 data[2 * 16];
};

void felix86_fsave_16(ThreadState* state, void* address);

void felix86_fsave_32(ThreadState* state, void* address);

void felix86_fxsave(ThreadState* state, void* address);

void felix86_xsave(ThreadState* state, void* address);

void felix86_frstor_16(ThreadState* state, void* address);

void felix86_frstor_32(ThreadState* state, void* address);

void felix86_fxrstor(ThreadState* state, void* address);

void felix86_xrstor(ThreadState* state, void* address);

bool felix86_xsave_contains_ymms();