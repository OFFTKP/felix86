#include "felix86/common/feature.hpp"
#include "felix86/common/state.hpp"
#include "felix86/common/xsave.hpp"

void felix86_fsave_16(const UserContext& ctx, void* address) {
    bool is_mmx = (x87State)ctx.x87_state == x87State::MMX;
    fsave_frame_16* data = (fsave_frame_16*)address;
    for (int i = 0; i < 8; i++) {
        if (is_mmx) {
            u16 ones = 0xFFFF;
            memcpy(&data->st[i], &ctx.fp[i], sizeof(double));
            memcpy(&data->st[i].exponent, &ones, sizeof(u16));
        } else {
            Float80 f80 = f64_to_80(ctx.fp[i]);
            memcpy(&data->st[i], &f80, sizeof(Float80));
        }
    }

    data->cw = ctx.fpu_cw;
    data->tw = ctx.fpu_tw;
    data->sw = (ctx.fpu_top << 11) | (ctx.fpu_sw & ~(0b111 << 11));

    // We use this reserved bit in FCW to signify we stored the registers as MMX and thus
    // will not need f80->f64 conversion if loaded with frstor
    if (is_mmx) {
        data->cw |= 0x8000;
    }
}

void felix86_fsave_32(const UserContext& ctx, void* address) {
    bool is_mmx = (x87State)ctx.x87_state == x87State::MMX;
    fsave_frame_32* data = (fsave_frame_32*)address;
    for (int i = 0; i < 8; i++) {
        if (is_mmx) {
            u16 ones = 0xFFFF;
            memcpy(&data->st[i], &ctx.fp[i], sizeof(double));
            memcpy(&data->st[i].exponent, &ones, sizeof(u16));
        } else {
            Float80 f80 = f64_to_80(ctx.fp[i]);
            memcpy(&data->st[i], &f80, sizeof(Float80));
        }
    }

    data->cw = ctx.fpu_cw;
    data->tw = ctx.fpu_tw;
    data->sw = (ctx.fpu_top << 11) | (ctx.fpu_sw & ~(0b111 << 11));

    // We use this reserved bit in FCW to signify we stored the registers as MMX and thus
    // will not need f80->f64 conversion if loaded with frstor
    if (is_mmx) {
        data->cw |= 0x8000;
    }
}

void felix86_frstor_16(UserContext& ctx, void* address) {
    fsave_frame_16* data = (fsave_frame_16*)address;

    ctx.fpu_top = (data->sw >> 11) & 0b111;
    ctx.fpu_cw = data->cw;
    ctx.fpu_tw = data->tw;
    ctx.fpu_sw = data->sw;
    ctx.rmode_x87 = rounding_mode(x86RoundingMode((ctx.fpu_cw >> 10) & 0b11));

    for (int i = 0; i < 8; i++) {
        if (ctx.fpu_cw & 0x8000) {
            memcpy(&ctx.fp[i], &data->st[i], sizeof(double));
        } else {
            double f64 = f80_to_64(&data->st[i]);
            memcpy(&ctx.fp[i], &f64, sizeof(double));
        }
    }
}

void felix86_frstor_32(UserContext& ctx, void* address) {
    fsave_frame_32* data = (fsave_frame_32*)address;

    ctx.fpu_top = (data->sw >> 11) & 0b111;
    ctx.fpu_cw = data->cw;
    ctx.fpu_tw = data->tw;
    ctx.fpu_sw = data->sw;
    ctx.rmode_x87 = rounding_mode(x86RoundingMode((ctx.fpu_cw >> 10) & 0b11));

    for (int i = 0; i < 8; i++) {
        if (ctx.fpu_cw & 0x8000) {
            memcpy(&ctx.fp[i], &data->st[i], sizeof(double));
        } else {
            double f64 = f80_to_64(&data->st[i]);
            memcpy(&ctx.fp[i], &f64, sizeof(double));
        }
    }
}

void felix86_fxsave(const UserContext& ctx, void* address, bool save_x87, bool save_xmm, bool save_mxcsr) {
    bool is_mmx = (x87State)ctx.x87_state == x87State::MMX;
    bool is_x87 = (x87State)ctx.x87_state == x87State::x87;
    fxsave_frame* data = (fxsave_frame*)address;
    bool mode32 = ThreadState::Get()->ctx.Mode32();
    if (save_xmm) {
        for (int i = 0; i < (mode32 ? 8 : 16); i++) {
            data->xmms[i] = ctx.xmm[i];
        }
    }

    if (save_x87) {
        for (int i = 0; i < 8; i++) {
            if (is_x87) {
                Float80 f80 = f64_to_80(ctx.fp[i]);
                memcpy(&data->st[i].st[0], &f80, sizeof(Float80));
            } else {
                if (!is_mmx) {
                    WARN("Unknown x87 state during fxsave");
                }
                u16 ones = 0xFFFF;
                memcpy(&data->st[i].st[0], &ctx.fp[i], sizeof(double));
                memcpy(&data->st[i].st[8], &ones, sizeof(u16));
            }
        }

        // Construct abridged FTW
        data->ftw = 0;
        for (int i = 0; i < 8; i++) {
            u16 mask = 0b11 << (i * 2);
            bool empty = (mask & ctx.fpu_tw) == mask;
            if (!empty) {
                data->ftw |= 1 << i;
            }
        }

        data->fcw = ctx.fpu_cw;
        data->fsw = (ctx.fpu_top << 11) | (ctx.fpu_sw & ~(0b111 << 11));

        // We use this reserved bit in FCW to signify we stored the registers as MMX and thus
        // will not need f80->f64 conversion if loaded with fxrstor
        if (is_mmx) {
            data->fcw |= 0x8000;
        }
    }

    if (save_mxcsr) {
        data->mxcsr = ctx.mxcsr;
    }
}

void felix86_fxrstor(UserContext& ctx, void* address, bool restore_x87, bool restore_xmm, bool restore_mxcsr) {
    fxsave_frame* data = (fxsave_frame*)address;

    bool mode32 = ThreadState::Get()->ctx.Mode32();
    if (restore_xmm) {
        for (int i = 0; i < (mode32 ? 8 : 16); i++) {
            ctx.xmm[i].data[0] = data->xmms[i].val[0];
            ctx.xmm[i].data[1] = data->xmms[i].val[1];
        }
    }

    if (restore_x87) {
        ctx.fpu_tw = 0;
        for (int i = 0; i < 8; i++) {
            if (!((data->ftw >> i) & 0b1)) {
                ctx.fpu_tw |= 0b11 << (i * 2);
            }
        }

        ctx.fpu_cw = data->fcw;
        ctx.fpu_sw = data->fsw;
        ctx.fpu_top = (data->fsw >> 11) & 7;

        for (int i = 0; i < 8; i++) {
            if (ctx.fpu_cw & 0x8000) {
                memcpy(&ctx.fp[i], &data->st[i].st[0], sizeof(double));
            } else {
                double f64 = f80_to_64((Float80*)&data->st[i].st[0]);
                memcpy(&ctx.fp[i], &f64, sizeof(double));
            }
        }

        ctx.rmode_x87 = rounding_mode(x86RoundingMode((ctx.fpu_cw >> 10) & 0b11));
    }

    if (restore_mxcsr) {
        ctx.mxcsr = data->mxcsr;
        ctx.rmode_sse = rounding_mode(x86RoundingMode((ctx.mxcsr >> 13) & 0b11));
    }
}

bool felix86_xsave_contains_ymms() {
    return is_feature_enabled(x86_feature::AVX) && is_feature_enabled(x86_feature::OSXSAVE);
}

void felix86_xsave(const UserContext& ctx, void* address, bool save_all) {
    u64 rfbm = (u64)(u32)ctx.gprs[X86_REF_RDX] << 32 | (u32)ctx.gprs[X86_REF_RAX];
    bool save_x87 = (rfbm & 0b001) || save_all;
    bool save_xmm = (rfbm & 0b010) || save_all;
    bool save_avx = (rfbm & 0b100) || save_all;
    bool save_mxcsr = save_xmm || save_avx;
    felix86_fxsave(ctx, address, save_x87, save_xmm, save_mxcsr);
    if (felix86_xsave_contains_ymms() && save_avx) {
        xsave_header* header = (xsave_header*)((u8*)address + sizeof(fxsave_frame));
        header->xstate_bv = get_xfeature_enabled_mask();
        header->xcomp_bv = 0; // use standard form
        ymm_hi* ymm_storage = (ymm_hi*)((u8*)address + sizeof(fxsave_frame) + sizeof(xsave_header));
        for (int i = 0; i < 16; i++) {
            memcpy((u8*)ymm_storage->data + 16 * i, &ctx.xmm[i].data[2], sizeof(u64) * 2);
        }
    }
}

void felix86_xrstor(UserContext& ctx, void* address, bool restore_all) {
    u64 rfbm = (u64)(u32)ctx.gprs[X86_REF_RDX] << 32 | (u32)ctx.gprs[X86_REF_RAX];
    bool restore_x87 = (rfbm & 0b001) || restore_all;
    bool restore_xmm = (rfbm & 0b010) || restore_all;
    bool restore_avx = (rfbm & 0b100) || restore_all;
    bool restore_mxcsr = restore_xmm || restore_avx;
    felix86_fxrstor(ctx, address, restore_x87, restore_xmm, restore_mxcsr);
    if (felix86_xsave_contains_ymms() && restore_avx) {
        ymm_hi* ymm_storage = (ymm_hi*)((u8*)address + sizeof(fxsave_frame) + sizeof(xsave_header));
        for (int i = 0; i < 16; i++) {
            memcpy(&ctx.xmm[i].data[2], (u8*)ymm_storage->data + 16 * i, sizeof(u64) * 2);
        }
    }
}