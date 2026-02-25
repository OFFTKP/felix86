#pragma once

#include "felix86/common/types.hpp"

enum class x86_feature {
    X87,
    MMX,
    SSE,
    SSE2,
    SSE3,
    SSSE3,
    SSE4_1,
    SSE4_2,
    AVX,
    AVX2,
    BMI1,
    BMI2,
    AES,
    VAES,
    PCLMULQDQ,
    VPCLMULQDQ,
    F16C,
    LZCNT_POPCNT,
};

u64 get_xfeature_enabled_mask();
bool is_feature_enabled(x86_feature feature);