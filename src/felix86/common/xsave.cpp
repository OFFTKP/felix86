#include "felix86/common/feature.hpp"
#include "felix86/common/state.hpp"
#include "felix86/common/xsave.hpp"

void felix86_fsave_16(ThreadState* state, void* address) {
    bool is_mmx = (x87State)state->x87_state == x87State::MMX;
    fsave_frame_16* data = (fsave_frame_16*)address;
    for (int i = 0; i < 8; i++) {
        if (is_mmx) {
            u16 ones = 0xFFFF;
            memcpy(&data->st[i], &state->fp[i], sizeof(double));
            memcpy(&data->st[i].exponent, &ones, sizeof(u16));
        } else {
            Float80 f80 = f64_to_80(state->fp[i]);
            memcpy(&data->st[i], &f80, sizeof(Float80));
        }
    }

    data->cw = state->fpu_cw;
    data->tw = state->fpu_tw;
    data->sw = (state->fpu_top << 11) | (state->fpu_sw & ~(0b111 << 11));

    // We use this reserved bit in FCW to signify we stored the registers as MMX and thus
    // will not need f80->f64 conversion if loaded with frstor
    if (is_mmx) {
        data->cw |= 0x8000;
    }
}

void felix86_fsave_32(ThreadState* state, void* address) {
    bool is_mmx = (x87State)state->x87_state == x87State::MMX;
    fsave_frame_32* data = (fsave_frame_32*)address;
    for (int i = 0; i < 8; i++) {
        if (is_mmx) {
            u16 ones = 0xFFFF;
            memcpy(&data->st[i], &state->fp[i], sizeof(double));
            memcpy(&data->st[i].exponent, &ones, sizeof(u16));
        } else {
            Float80 f80 = f64_to_80(state->fp[i]);
            memcpy(&data->st[i], &f80, sizeof(Float80));
        }
    }

    data->cw = state->fpu_cw;
    data->tw = state->fpu_tw;
    data->sw = (state->fpu_top << 11) | (state->fpu_sw & ~(0b111 << 11));

    // We use this reserved bit in FCW to signify we stored the registers as MMX and thus
    // will not need f80->f64 conversion if loaded with frstor
    if (is_mmx) {
        data->cw |= 0x8000;
    }
}

void felix86_frstor_16(ThreadState* state, void* address) {
    fsave_frame_16* data = (fsave_frame_16*)address;

    state->fpu_top = (data->sw >> 11) & 0b111;
    state->fpu_cw = data->cw;
    state->fpu_tw = data->tw;
    state->fpu_sw = data->sw;
    state->rmode_x87 = rounding_mode(x86RoundingMode((state->fpu_cw >> 10) & 0b11));

    for (int i = 0; i < 8; i++) {
        if (state->fpu_cw & 0x8000) {
            memcpy(&state->fp[i], &data->st[i], sizeof(double));
        } else {
            double f64 = f80_to_64(&data->st[i]);
            memcpy(&state->fp[i], &f64, sizeof(double));
        }
    }
}

void felix86_frstor_32(ThreadState* state, void* address) {
    fsave_frame_32* data = (fsave_frame_32*)address;

    state->fpu_top = (data->sw >> 11) & 0b111;
    state->fpu_cw = data->cw;
    state->fpu_tw = data->tw;
    state->fpu_sw = data->sw;
    state->rmode_x87 = rounding_mode(x86RoundingMode((state->fpu_cw >> 10) & 0b11));

    for (int i = 0; i < 8; i++) {
        if (state->fpu_cw & 0x8000) {
            memcpy(&state->fp[i], &data->st[i], sizeof(double));
        } else {
            double f64 = f80_to_64(&data->st[i]);
            memcpy(&state->fp[i], &f64, sizeof(double));
        }
    }
}

void felix86_fxsave(ThreadState* state, void* address) {
    bool is_mmx = (x87State)state->x87_state == x87State::MMX;
    bool is_x87 = (x87State)state->x87_state == x87State::x87;
    fxsave_frame* data = (fxsave_frame*)address;

    for (int i = 0; i < (g_mode32 ? 8 : 16); i++) {
        data->xmms[i] = state->GetXmm(x86_ref_e(X86_REF_XMM0 + i));
    }

    for (int i = 0; i < 8; i++) {
        if (is_x87) {
            Float80 f80 = f64_to_80(state->fp[i]);
            memcpy(&data->st[i].st[0], &f80, sizeof(Float80));
        } else {
            if (!is_mmx) {
                WARN("Unknown x87 state during fxsave");
            }
            u16 ones = 0xFFFF;
            memcpy(&data->st[i].st[0], &state->fp[i], sizeof(double));
            memcpy(&data->st[i].st[8], &ones, sizeof(u16));
        }
    }

    // Construct abridged FTW
    data->ftw = 0;
    for (int i = 0; i < 8; i++) {
        u16 mask = 0b11 << (i * 2);
        bool empty = (mask & state->fpu_tw) == mask;
        if (!empty) {
            data->ftw |= 1 << i;
        }
    }

    data->fcw = state->fpu_cw;
    data->fsw = (state->fpu_top << 11) | (state->fpu_sw & ~(0b111 << 11));
    data->mxcsr = state->mxcsr;

    // We use this reserved bit in FCW to signify we stored the registers as MMX and thus
    // will not need f80->f64 conversion if loaded with fxrstor
    if (is_mmx) {
        data->fcw |= 0x8000;
    }
}

void felix86_fxrstor(ThreadState* state, void* address) {
    fxsave_frame* data = (fxsave_frame*)address;

    for (int i = 0; i < (g_mode32 ? 8 : 16); i++) {
        state->xmm[i].data[0] = data->xmms[i].val[0];
        state->xmm[i].data[1] = data->xmms[i].val[1];
    }

    state->fpu_tw = 0;
    for (int i = 0; i < 8; i++) {
        if (!((data->ftw >> i) & 0b1)) {
            state->fpu_tw |= 0b11 << (i * 2);
        }
    }

    state->fpu_cw = data->fcw;
    state->fpu_sw = data->fsw;
    state->fpu_top = (data->fsw >> 11) & 7;
    state->mxcsr = data->mxcsr;

    for (int i = 0; i < 8; i++) {
        if (state->fpu_cw & 0x8000) {
            memcpy(&state->fp[i], &data->st[i].st[0], sizeof(double));
        } else {
            double f64 = f80_to_64((Float80*)&data->st[i].st[0]);
            memcpy(&state->fp[i], &f64, sizeof(double));
        }
    }

    state->rmode_x87 = rounding_mode(x86RoundingMode((state->fpu_cw >> 10) & 0b11));
    state->rmode_sse = rounding_mode(x86RoundingMode((state->mxcsr >> 13) & 0b11));
}

bool felix86_xsave_contains_ymms() {
    return is_feature_enabled(x86_feature::AVX) && is_feature_enabled(x86_feature::OSXSAVE);
}

void felix86_xsave(ThreadState* state, void* address) {
    felix86_fxsave(state, address);
    if (felix86_xsave_contains_ymms()) {
        xsave_header* header = (xsave_header*)((u8*)address + sizeof(fxsave_frame));
        header->xstate_bv = get_xfeature_enabled_mask();
        header->xcomp_bv = 0; // use standard form
        ymm_hi* ymm_storage = (ymm_hi*)((u8*)address + sizeof(fxsave_frame) + sizeof(xsave_header));
        for (int i = 0; i < 16; i++) {
            memcpy((u8*)ymm_storage->data + 16 * i, &state->xmm[i].data[2], sizeof(u64) * 2);
        }
    }
}

void felix86_xrstor(ThreadState* state, void* address) {
    felix86_fxrstor(state, address);
    if (felix86_xsave_contains_ymms()) {
        ymm_hi* ymm_storage = (ymm_hi*)((u8*)address + sizeof(fxsave_frame) + sizeof(xsave_header));
        for (int i = 0; i < 16; i++) {
            memcpy(&state->xmm[i].data[2], (u8*)ymm_storage->data + 16 * i, sizeof(u64) * 2);
        }
    }
}