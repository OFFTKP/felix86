#pragma once

#include "softfloat.h"

#ifdef __cplusplus
extern "C" {
#endif
float128_t cephes_f128_atanl(float128_t x);
float128_t cephes_f128_atan2l(float128_t y, float128_t x);
float128_t cephes_f128_ceill(float128_t x);
float128_t cephes_f128_cosl(float128_t x);
float128_t cephes_f128_fabsl(float128_t x);
float128_t cephes_f128_floorl(float128_t x);
float128_t cephes_f128_frexpl(float128_t x, int* pw2);
int cephes_f128_isfinitel(float128_t x);
int cephes_f128_isnanl(float128_t x);
float128_t cephes_f128_ldexpl(float128_t x, int pw2);
float128_t cephes_f128_polevll(float128_t x, void* PP, int n);
float128_t cephes_f128_p1evll(float128_t x, void* PP, int n);
int cephes_f128_signbitl(float128_t x);
float128_t cephes_f128_sinl(float128_t x);
float128_t cephes_f128_tanl(float128_t x);
float128_t cephes_f128_log2l(float128_t x);
float128_t cephes_f128_exp2l(float128_t x);
#ifdef __cplusplus
}
#endif
