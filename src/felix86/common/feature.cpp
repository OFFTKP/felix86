#include "felix86/common/config.hpp"
#include "felix86/common/feature.hpp"
#include "felix86/common/global.hpp"

u64 get_xfeature_enabled_mask() {
    u64 result = 0;
    result |= 0b1; // x87/MMX
    if (!g_config.no_sse2) {
        result |= 0b10; // SSE

        if (!g_config.no_avx) {
            result |= 0b100; // AVX
        }
    }
    return result;
}

bool is_feature_enabled(x86_feature feature) {
    switch (feature) {
    case x86_feature::X87: {
        return true;
    }
    case x86_feature::MMX: {
        return true && is_feature_enabled(x86_feature::X87);
    }
    case x86_feature::SSE: {
        return true && is_feature_enabled(x86_feature::MMX);
    }
    case x86_feature::SSE2: {
        return !g_config.no_sse2 && is_feature_enabled(x86_feature::SSE);
    }
    case x86_feature::SSE3: {
        return !g_config.no_sse3 && is_feature_enabled(x86_feature::SSE2);
    }
    case x86_feature::SSSE3: {
        return !g_config.no_ssse3 && is_feature_enabled(x86_feature::SSE3);
    }
    case x86_feature::SSE4_1: {
        return !g_config.no_sse4_1 && is_feature_enabled(x86_feature::SSSE3);
    }
    case x86_feature::SSE4_2: {
        // Zbc is needed for CRC32
        // TODO: Zbc is not part of RVA23 so it shouldn't be necessary for CRC32
        return !g_config.no_sse4_2 && is_feature_enabled(x86_feature::SSE4_1) && Extensions::B;
    }
    case x86_feature::AVX: {
        return !g_config.no_avx && is_feature_enabled(x86_feature::SSE4_2) && Extensions::VLEN >= 256;
    }
    case x86_feature::AVX2: {
        // Zicclsm is needed for gathers
        return !g_config.no_avx2 && is_feature_enabled(x86_feature::AVX) && Extensions::Zicclsm;
    }
    case x86_feature::AES: {
        return Extensions::Zvkned;
    }
    case x86_feature::VAES: {
        return is_feature_enabled(x86_feature::AES) && is_feature_enabled(x86_feature::AVX);
    }
    case x86_feature::PCLMULQDQ: {
        return !g_config.no_pclmulqdq && Extensions::B;
    }
    case x86_feature::VPCLMULQDQ: {
        return false && is_feature_enabled(x86_feature::AVX);
    }
    case x86_feature::BMI1: {
        return is_feature_enabled(x86_feature::AVX) && Extensions::B;
    }
    case x86_feature::BMI2: {
        return false && is_feature_enabled(x86_feature::BMI1);
    }
    case x86_feature::F16C: {
        return is_feature_enabled(x86_feature::AVX) && Extensions::Zvfhmin;
    }
    case x86_feature::LZCNT_POPCNT: {
        return Extensions::B;
    }
    }
}