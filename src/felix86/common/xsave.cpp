#include "felix86/common/feature.hpp"
#include "felix86/common/state.hpp"
#include "felix86/common/xsave.hpp"

struct fenv_data_16 {
    u16 cw;
    u16 sw;
    u16 tw;
    u16 fip;
    u16 fcs;
    u16 fdp;
    u16 fds;
};

static_assert(sizeof(fenv_data_16) == 14);

struct fenv_data_32 {
    u16 cw = 0;
    alignas(u32) u16 sw = 0;
    alignas(u32) u16 tw = 0;
    u32 fip = 0;
    u32 unused = 0;
    u32 fdp = 0;
    u32 fds = 0;
};

static_assert(sizeof(fenv_data_32) == 28);

static u8 tw_16_to_8(u16 tw) {
    u8 ret = 0;
    for (int i = 0; i < 8; i++) {
        if (((tw >> (i * 2)) & 0b11) != 0b11) {
            ret |= 1 << i;
        }
    }
    return ret;
}

static u16 tw_8_to_16(const Float80* st, u8 tw) {
    u16 ret = 0;
    for (int i = 0; i < 8; i++) {
        if (((tw >> i) & 0b1) == 0b1) {
            auto& reg = st[i];
            if ((reg.exponent & 0x7FFF) == 0 && reg.significand == 0) {
                ret |= 0b01 << (i * 2);
            } else if ((reg.exponent & 0x7FFF) == 0x7FFF || ((reg.exponent & 0x7FFF) == 0 && reg.significand != 0) ||
                       ((reg.significand >> 63) & 1) == 0) {
                ret |= 0b10 << (i * 2);
            }
        } else {
            ret |= 0b11 << (i * 2);
        }
    }
    return ret;
}

void felix86_fsave_16(const UserContext& ctx, void* address) {
    bool is_mmx = (x87State)ctx.x87_state == x87State::MMX;
    fsave_frame_16* data = (fsave_frame_16*)address;
    for (int i = 0; i < 8; i++) {
        const Float80& st = ctx.st[(ctx.fpu_top + i) & 0b111];
        if (g_config.reduced_precision) {
            if (is_mmx) {
                u16 ones = 0xFFFF;
                memcpy(&data->st[i], &st.significand, sizeof(double));
                memcpy(&data->st[i].exponent, &ones, sizeof(u16));
            } else {
                Float80 f80 = f64_to_80(st.significand);
                memcpy(&data->st[i], &f80, sizeof(Float80));
            }
        } else {
            memcpy(&data->st[i], &st, sizeof(Float80));
        }
    }

    data->cw = ctx.fpu_cw;
    data->tw = tw_8_to_16(ctx.st, ctx.fpu_tw);
    data->sw = (ctx.fpu_top << 11) | (ctx.fpu_sw & ~(0b111 << 11));

    if (g_config.reduced_precision) {
        // We use this reserved bit in FCW to signify we stored the registers as MMX and thus
        // will not need f80->f64 conversion if loaded with frstor
        // TODO: can we do better than this hack
        if (is_mmx) {
            data->cw |= 0x8000;
        }
    }
}

void felix86_fsave_32(const UserContext& ctx, void* address) {
    bool is_mmx = (x87State)ctx.x87_state == x87State::MMX;
    fsave_frame_32* data = (fsave_frame_32*)address;
    for (int i = 0; i < 8; i++) {
        const Float80& st = ctx.st[(ctx.fpu_top + i) & 0b111];
        if (g_config.reduced_precision) {
            if (is_mmx) {
                u16 ones = 0xFFFF;
                memcpy(&data->st[i], &st, sizeof(double));
                memcpy(&data->st[i].exponent, &ones, sizeof(u16));
            } else {
                Float80 f80 = f64_to_80(st.significand);
                memcpy(&data->st[i], &f80, sizeof(Float80));
            }
        } else {
            memcpy(&data->st[i], &st, sizeof(Float80));
        }
    }

    data->cw = ctx.fpu_cw;
    data->tw = tw_8_to_16(ctx.st, ctx.fpu_tw);
    data->sw = (ctx.fpu_top << 11) | (ctx.fpu_sw & ~(0b111 << 11));

    if (g_config.reduced_precision) {
        // We use this reserved bit in FCW to signify we stored the registers as MMX and thus
        // will not need f80->f64 conversion if loaded with frstor
        // TODO: can we do better than this hack
        if (is_mmx) {
            data->cw |= 0x8000;
        }
    }
}

void felix86_frstor_16(UserContext& ctx, void* address) {
    fsave_frame_16* data = (fsave_frame_16*)address;

    ctx.fpu_top = (data->sw >> 11) & 0b111;
    ctx.fpu_cw = data->cw;
    ctx.fpu_tw = tw_16_to_8(data->tw);
    ctx.fpu_sw = data->sw;
    ctx.rmode_x87 = rounding_mode(x86RoundingMode((ctx.fpu_cw >> 10) & 0b11));

    for (int i = 0; i < 8; i++) {
        Float80& st = ctx.st[(ctx.fpu_top + i) & 0b111];
        if (g_config.reduced_precision) {
            if (ctx.fpu_cw & 0x8000) {
                memcpy(&st, &data->st[i], sizeof(double));
            } else {
                double f64 = f80_to_64(&data->st[i]);
                memcpy(&st, &f64, sizeof(double));
            }
        } else {
            memcpy(&st, &data->st[i], sizeof(Float80));
        }
    }
}

void felix86_frstor_32(UserContext& ctx, void* address) {
    fsave_frame_32* data = (fsave_frame_32*)address;

    ctx.fpu_top = (data->sw >> 11) & 0b111;
    ctx.fpu_cw = data->cw;
    ctx.fpu_tw = tw_16_to_8(data->tw);
    ctx.fpu_sw = data->sw;
    ctx.rmode_x87 = rounding_mode(x86RoundingMode((ctx.fpu_cw >> 10) & 0b11));

    for (int i = 0; i < 8; i++) {
        Float80& st = ctx.st[(ctx.fpu_top + i) & 0b111];
        if (g_config.reduced_precision) {
            if (ctx.fpu_cw & 0x8000) {
                memcpy(&st, &data->st[i], sizeof(double));
            } else {
                double f64 = f80_to_64(&data->st[i]);
                memcpy(&st, &f64, sizeof(double));
            }
        } else {
            memcpy(&st, &data->st[i], sizeof(Float80));
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
            const Float80& st = ctx.st[(ctx.fpu_top + i) & 0b111];
            if (g_config.reduced_precision) {
                if (is_x87) {
                    Float80 f80 = f64_to_80(st.significand);
                    memcpy(&data->st[i].st[0], &f80, sizeof(Float80));
                } else {
                    if (!is_mmx) {
                        WARN("Unknown x87 state during fxsave");
                    }
                    u16 ones = 0xFFFF;
                    memcpy(&data->st[i].st[0], &st.significand, sizeof(u64));
                    memcpy(&data->st[i].st[8], &ones, sizeof(u16));
                }
            } else {
                memcpy(&data->st[i].st[0], &st, sizeof(Float80));
            }
        }

        data->ftw = ctx.fpu_tw;
        data->fcw = ctx.fpu_cw;
        data->fsw = (ctx.fpu_top << 11) | (ctx.fpu_sw & ~(0b111 << 11));

        if (g_config.reduced_precision) {
            // We use this reserved bit in FCW to signify we stored the registers as MMX and thus
            // will not need f80->f64 conversion if loaded with fxrstor
            // TODO: can we do better than this hack
            if (is_mmx) {
                data->fcw |= 0x8000;
            }
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
        ctx.fpu_tw = data->ftw;
        ctx.fpu_cw = data->fcw;
        ctx.fpu_sw = data->fsw;
        ctx.fpu_top = (data->fsw >> 11) & 7;

        for (int i = 0; i < 8; i++) {
            Float80& st = ctx.st[(ctx.fpu_top + i) & 0b111];
            if (g_config.reduced_precision) {
                if (ctx.fpu_cw & 0x8000) {
                    memcpy(&st, &data->st[i].st[0], sizeof(double));
                } else {
                    double f64 = f80_to_64((Float80*)&data->st[i].st[0]);
                    memcpy(&st, &f64, sizeof(double));
                }
            } else {
                memcpy(&st, &data->st[i].st[0], sizeof(Float80));
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
    if ((u64)address & 63) {
        // This would cause #GP, so warn
        WARN("Improperly aligned xsave area: %lx", (u64)address);
    }
    u64 rfbm = (u64)(u32)ctx.gprs[X86_REF_RDX] << 32 | (u32)ctx.gprs[X86_REF_RAX];
    bool save_x87 = (rfbm & 0b001) || save_all;
    bool save_xmm = (rfbm & 0b010) || save_all;
    bool save_avx = (rfbm & 0b100) || save_all;
    bool save_mxcsr = save_xmm || save_avx;
    felix86_fxsave(ctx, address, save_x87, save_xmm, save_mxcsr);
    if (save_all || rfbm != 0) {
        xsave_header* header = (xsave_header*)((u8*)address + sizeof(fxsave_frame));
        header->xstate_bv = save_all ? get_xfeature_enabled_mask() : (rfbm & get_xfeature_enabled_mask());
    }
    if (felix86_xsave_contains_ymms() && save_avx) {
        ymm_hi* ymm_storage = (ymm_hi*)((u8*)address + sizeof(fxsave_frame) + sizeof(xsave_header));
        for (int i = 0; i < 16; i++) {
            memcpy((u8*)ymm_storage->data + 16 * i, &ctx.xmm[i].data[2], sizeof(u64) * 2);
        }
    }
}

void felix86_x87_init(UserContext& ctx) {
    ctx.fpu_cw = 0x37F;
    ctx.fpu_sw = 0;
    ctx.fpu_tw = 0xFF;
    ctx.fpu_top = 0;
    for (int i = 0; i < 8; i++) {
        ctx.st[i] = Float80{};
    }
    ctx.x87_state = x87State::x87;
    ctx.rmode_x87 = rounding_mode(x86RoundingMode((ctx.fpu_cw >> 10) & 0b11));
}

void felix86_xrstor(UserContext& ctx, void* address, bool restore_all) {
    u64 rfbm = (u64)(u32)ctx.gprs[X86_REF_RDX] << 32 | (u32)ctx.gprs[X86_REF_RAX];
    u64 xcr0 = get_xfeature_enabled_mask();
    u64 hdr_bv = 0;
    if (!restore_all) {
        xsave_header* header = (xsave_header*)((u8*)address + sizeof(fxsave_frame));
        hdr_bv = header->xstate_bv;
    }
    u64 req_x87 = restore_all || (rfbm & 0b001);
    u64 req_xmm = restore_all || (rfbm & 0b010);
    u64 req_avx = restore_all || (rfbm & 0b100);
    bool restore_x87 = req_x87 && (restore_all || (hdr_bv & 0b001)) && (xcr0 & 0b001);
    bool restore_xmm = req_xmm && (restore_all || (hdr_bv & 0b010)) && (xcr0 & 0b010);
    bool restore_avx = req_avx && (restore_all || (hdr_bv & 0b100)) && (xcr0 & 0b100);
    bool restore_mxcsr = req_xmm;
    felix86_fxrstor(ctx, address, restore_x87, restore_xmm, restore_mxcsr);
    if (felix86_xsave_contains_ymms() && restore_avx) {
        ymm_hi* ymm_storage = (ymm_hi*)((u8*)address + sizeof(fxsave_frame) + sizeof(xsave_header));
        for (int i = 0; i < 16; i++) {
            memcpy(&ctx.xmm[i].data[2], (u8*)ymm_storage->data + 16 * i, sizeof(u64) * 2);
        }
    }

    if (!restore_x87 && req_x87) {
        felix86_x87_init(ctx);
    }
    if (!restore_xmm) {
        for (int i = 0; i < 16; i++) {
            ctx.xmm[i] = XmmReg{};
        }
        ctx.rmode_sse = rounding_mode(x86RoundingMode((ctx.mxcsr >> 13) & 0b11));
    }
    if (!restore_avx) {
        for (int i = 0; i < 16; i++) {
            ctx.xmm[i].data[2] = 0;
            ctx.xmm[i].data[3] = 0;
        }
    }
}

void felix86_fstenv_16(ThreadState* state, u64 address) {
    fenv_data_16* env = (fenv_data_16*)address;
    env->cw = state->ctx.fpu_cw;
    env->tw = tw_8_to_16(state->ctx.st, state->ctx.fpu_tw);
    env->sw = (state->ctx.fpu_top << 11) | (state->ctx.fpu_sw & ~(0b111 << 11));
}

void felix86_fstenv_32(ThreadState* state, u64 address) {
    fenv_data_32* env = (fenv_data_32*)address;
    env->cw = state->ctx.fpu_cw;
    env->tw = tw_8_to_16(state->ctx.st, state->ctx.fpu_tw);
    env->sw = (state->ctx.fpu_top << 11) | (state->ctx.fpu_sw & ~(0b111 << 11));
}

void felix86_fldenv_16(struct ThreadState* state, u64 address) {
    fenv_data_16* env = (fenv_data_16*)address;
    state->ctx.fpu_cw = env->cw;
    state->ctx.fpu_tw = tw_16_to_8(env->tw);
    state->ctx.fpu_sw = env->sw;
    state->ctx.fpu_top = (env->sw >> 11) & 0b111;
    state->ctx.rmode_x87 = rounding_mode(x86RoundingMode((state->ctx.fpu_cw >> 10) & 0b11));
}

void felix86_fldenv_32(struct ThreadState* state, u64 address) {
    fenv_data_32* env = (fenv_data_32*)address;
    state->ctx.fpu_cw = env->cw;
    state->ctx.fpu_tw = tw_16_to_8(env->tw);
    state->ctx.fpu_sw = env->sw;
    state->ctx.fpu_top = (env->sw >> 11) & 0b111;
    state->ctx.rmode_x87 = rounding_mode(x86RoundingMode((state->ctx.fpu_cw >> 10) & 0b11));
}
