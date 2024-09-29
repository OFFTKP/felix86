#pragma once

#include "felix86/common/utility.hpp"

struct XmmReg {
    u64 data[2];
};

struct FpReg {
    u64 mm;
    u16 signexp;
};

struct ThreadState {
    u64 gprs[16] = {};
    u64 rip = 0;
    FpReg fp[8];
    XmmReg xmm[16];
    bool cf;
    bool pf;
    bool af;
    bool zf;
    bool sf;
    bool of;
    u64 gsbase;
    u64 fsbase;

    u64 robust_futex_list;
    u64 set_child_tid;
    u64 clear_child_tid;
    u64 brk_current_address;
};

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

typedef struct {
    u32 fpu : 1;
    u32 vme : 1;
    u32 de : 1;
    u32 pse : 1;
    u32 tsc : 1;
    u32 msr : 1;
    u32 pae : 1;
    u32 mce : 1;
    u32 cx8 : 1;
    u32 apic : 1;
    u32 : 1;
    u32 sep : 1;
    u32 mtrr : 1;
    u32 pge : 1;
    u32 mca : 1;
    u32 cmov : 1;
    u32 pat : 1;
    u32 pse36 : 1;
    u32 psn : 1;
    u32 clfsh : 1;
    u32 : 1;
    u32 ds : 1;
    u32 acpi : 1;
    u32 mmx : 1;
    u32 fxsr : 1;
    u32 sse : 1;
    u32 sse2 : 1;
    u32 ss : 1;
    u32 htt : 1;
    u32 tm : 1;
    u32 : 1;
    u32 pbe : 1;
} feature_info_edx_t;

typedef struct {
    u32 sse3 : 1;
    u32 pclmulqdq : 1;
    u32 dtes64 : 1;
    u32 monitor : 1;
    u32 ds_cpl : 1;
    u32 vmx : 1;
    u32 smx : 1;
    u32 est : 1;
    u32 tm2 : 1;
    u32 ssse3 : 1;
    u32 cnxt_id : 1;
    u32 sdbg : 1;
    u32 fma : 1;
    u32 cmpxchg16b : 1;
    u32 xtpr : 1;
    u32 pdcm : 1;
    u32 : 1;
    u32 pcid : 1;
    u32 dca : 1;
    u32 sse4_1 : 1;
    u32 sse4_2 : 1;
    u32 x2apic : 1;
    u32 movbe : 1;
    u32 popcnt : 1;
    u32 tsc_deadline : 1;
    u32 aes : 1;
    u32 xsave : 1;
    u32 osxsave : 1;
    u32 avx : 1;
    u32 f16c : 1;
    u32 rdrand : 1;
    u32 hypervisor : 1;
} feature_info_ecx_t;

typedef struct {
    u32 fpu : 1;
    u32 vme : 1;
    u32 de : 1;
    u32 pse : 1;
    u32 tsc : 1;
    u32 msr : 1;
    u32 pae : 1;
    u32 mce : 1;
    u32 cx8 : 1;
    u32 apic : 1;
    u32 : 1;
    u32 syscall : 1;
    u32 mtrr : 1;
    u32 pge : 1;
    u32 mca : 1;
    u32 cmov : 1;
    u32 pat : 1;
    u32 pse36 : 1;
    u32 : 1;
    u32 ecc : 1;
    u32 nx : 1;
    u32 : 1;
    u32 mmxext : 1;
    u32 mmx : 1;
    u32 fxsr : 1;
    u32 fxsr_opt : 1;
    u32 pdpe1gb : 1;
    u32 rdtscp : 1;
    u32 : 1;
    u32 lm : 1;
    u32 _3dnowext : 1;
    u32 _3dnow : 1;
} feature_info_80000001_edx_t;

typedef struct {
    u32 lahf_lm : 1;
    u32 cmp_legacy : 1;
    u32 svm : 1;
    u32 extapic : 1;
    u32 cr8_legacy : 1;
    u32 abm : 1;
    u32 sse4a : 1;
    u32 misalignsse : 1;
    u32 _3dnowprefetch : 1;
    u32 osvw : 1;
    u32 ibs : 1;
    u32 xop : 1;
    u32 skinit : 1;
    u32 wdt : 1;
    u32 : 1;
    u32 lwp : 1;
    u32 fma4 : 1;
    u32 tce : 1;
    u32 : 1;
    u32 nodeid_msr : 1;
    u32 : 1;
    u32 tbm : 1;
    u32 topoext : 1;
    u32 perfctr_core : 1;
    u32 perfctr_nb : 1;
    u32 : 1;
    u32 dbx : 1;
    u32 perftsc : 1;
    u32 pcx_l2i : 1;
    u32 monitorx : 1;
    u32 addr_mask_ext : 1;
    u32 : 1;
} feature_info_80000001_ecx_t;
