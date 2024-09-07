#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "felix86/common/utility.h"

typedef struct {
	u64 data[2];
} xmm_reg_t;

typedef struct {
	u64 gprs[16];
	u64 rip;
	u64 mm[8];
	xmm_reg_t xmm[16];
	u16 gs;
	u16 fs;
	bool cf;
	bool pf;
	bool af;
	bool zf;
	bool sf;
	bool of;
} x86_state_t;

typedef union {
	struct {
		u64 x87 : 1;
		u64 sse : 1;
		u64 avx : 1;
		u64 mpx : 2;
		u64 avx512 : 3;
		u64 pt : 1;
		u64 pkru : 1;
		u64 pasid : 1;
		u64 cet_u : 1;
		u64 cet_s : 1;
		u64 hdc : 1;
		u64 uintr : 1;
		u64 lbr : 1;
		u64 hwp : 1;
		u64 xtilecfg : 1;
		u64 xtiledata : 1;
		u64 apx : 1;
		u64 : 44;
	};

	u64 raw;
} xcr0_reg_t;

typedef union {
    struct {
        u8 rm : 3;
        u8 reg : 3;
        u8 mod : 2;
    };

    u8 raw;
} modrm_t;

typedef union {
    struct {
        u8 base : 3;
        u8 index : 3;
        u8 scale : 2;
    };

    u8 raw;
} sib_t;

#ifdef __cplusplus
}
#endif