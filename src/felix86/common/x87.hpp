#pragma once

extern "C" {
#include "softfloat.h"
}

struct ThreadState;

void felix86_x87_FLD(ThreadState* state, extFloat80_t* mem, int size);
void felix86_x87_FST(ThreadState* state, extFloat80_t* mem, int size);
void felix86_x87_FSTP(ThreadState* state, extFloat80_t* mem, int size);
#define COMMON_ARITHMETIC(name)                                                                                                                      \
    void felix86_x87_F##name(ThreadState* state, extFloat80_t* lhs, extFloat80_t* rhs, int size);                                                    \
    void felix86_x87_F##name##P(ThreadState* state, extFloat80_t* lhs, extFloat80_t* rhs, int size);                                                 \
    void felix86_x87_FI##name(ThreadState* state, extFloat80_t* lhs, extFloat80_t* rhs, int size);
COMMON_ARITHMETIC(ADD);
COMMON_ARITHMETIC(SUB);
COMMON_ARITHMETIC(MUL);
COMMON_ARITHMETIC(DIV);
COMMON_ARITHMETIC(SUBR);
COMMON_ARITHMETIC(DIVR);
#undef COMMON_ARITHMETIC
void felix86_x87_FLDZ(ThreadState* state);
void felix86_x87_FLD1(ThreadState* state);
void felix86_x87_FLDL2T(ThreadState* state);
void felix86_x87_FLDL2E(ThreadState* state);
void felix86_x87_FLDPI(ThreadState* state);
void felix86_x87_FLDLG2(ThreadState* state);
void felix86_x87_FLDLN2(ThreadState* state);
void felix86_x87_FABS(ThreadState* state);
void felix86_x87_FCHS(ThreadState* state);
void felix86_x87_FTST(ThreadState* state);
void felix86_x87_FSIN(ThreadState* state);
void felix86_x87_FCOS(ThreadState* state);
void felix86_x87_FPATAN(ThreadState* state);
void felix86_x87_FYL2X(ThreadState* state);
void felix86_x87_FYL2XP1(ThreadState* state);
void felix86_x87_F2XM1(ThreadState* state);
void felix86_x87_FSQRT(ThreadState* state);
void felix86_x87_FRNDINT(ThreadState* state);
void felix86_x87_FILD(ThreadState* state, extFloat80_t* mem, int size);
void felix86_x87_FIST(ThreadState* state, extFloat80_t* mem, int size);
void felix86_x87_FISTP(ThreadState* state, extFloat80_t* mem, int size);
void felix86_x87_FISTTP(ThreadState* state, extFloat80_t* mem, int size);
void felix86_x87_FXCH(ThreadState* state, extFloat80_t* lhs, extFloat80_t* rhs, int);
void felix86_x87_FUCOM(ThreadState* state, extFloat80_t* lhs, extFloat80_t* rhs, int size);
void felix86_x87_FUCOMP(ThreadState* state, extFloat80_t* lhs, extFloat80_t* rhs, int size);
void felix86_x87_FUCOMPP(ThreadState* state, extFloat80_t* lhs, extFloat80_t* rhs, int size);
void felix86_x87_FCOM(ThreadState* state, extFloat80_t* lhs, extFloat80_t* rhs, int);
void felix86_x87_FCOMP(ThreadState* state, extFloat80_t* lhs, extFloat80_t* rhs, int);
void felix86_x87_FCOMPP(ThreadState* state, extFloat80_t* lhs, extFloat80_t* rhs, int);
void felix86_x87_FUCOMI(ThreadState* state, extFloat80_t* lhs, extFloat80_t* rhs, int);
void felix86_x87_FUCOMIP(ThreadState* state, extFloat80_t* lhs, extFloat80_t* rhs, int);
void felix86_x87_FCOMI(ThreadState* state, extFloat80_t* lhs, extFloat80_t* rhs, int);
void felix86_x87_FCOMIP(ThreadState* state, extFloat80_t* lhs, extFloat80_t* rhs, int);
void felix86_x87_FICOM(ThreadState* state, extFloat80_t* mem, int size);
void felix86_x87_FICOMP(ThreadState* state, extFloat80_t* mem, int size);
void felix86_x87_FPREM(ThreadState* state);
void felix86_x87_FPREM1(ThreadState* state);
void felix86_x87_FSCALE(ThreadState* state);
void felix86_x87_FSINCOS(ThreadState* state);
void felix86_x87_FPTAN(ThreadState* state);
void felix86_x87_FXTRACT(ThreadState* state);
void felix86_x87_FINCSTP(ThreadState* state);
void felix86_x87_FDECSTP(ThreadState* state);
void felix86_x87_FXAM(ThreadState* state);
void felix86_x87_FFREEP(ThreadState* state, extFloat80_t* reg, int);
void felix86_x87_FBLD(ThreadState* state, extFloat80_t* mem, int);
void felix86_x87_FBSTP(ThreadState* state, extFloat80_t* mem, int);
