%ifdef CONFIG
{
  "HostFeatures": ["AVX"],
  "RegData": {
    "XMM0":  ["0x8000000080000000", "0x0000000000000000", "0x0000000000000000", "0x0000000000000000"],
    "XMM1":  ["0x8000000080000000", "0x0000000000000000", "0x0000000000000000", "0x0000000000000000"],
    "XMM2":  ["0x8000000080000000", "0x0000000000000000", "0x0000000000000000", "0x0000000000000000"],
    "XMM3":  ["0x8000000080000000", "0x0000000000000000", "0x0000000000000000", "0x0000000000000000"],
    "XMM4":  ["0x8000000080000000", "0x8000000080000000", "0x0000000000000000", "0x0000000000000000"],
    "XMM5":  ["0x800000007FFFFFFF", "0x8000000080000000", "0x0000000000000000", "0x0000000000000000"],
    "XMM6":  ["0x0000002A80000000", "0x8000000080000000", "0x0000000000000000", "0x0000000000000000"],
    "XMM7":  ["0x8000000080000000", "0x00000000FFFFFFD6", "0x0000000000000000", "0x0000000000000000"]
  }
}
%endif
bits 64

vpcmpeqd ymm0,  ymm0,  ymm0
vpcmpeqd ymm1,  ymm1,  ymm1
vpcmpeqd ymm2,  ymm2,  ymm2
vpcmpeqd ymm3,  ymm3,  ymm3
vpcmpeqd ymm4,  ymm4,  ymm4
vpcmpeqd ymm5,  ymm5,  ymm5
vpcmpeqd ymm6,  ymm6,  ymm6
vpcmpeqd ymm7,  ymm7,  ymm7

lea rdx, [rel .pd_data]
vcvttpd2dq xmm0, oword [rdx + 16 * 0]  ; +qNaN, -qNaN
vcvttpd2dq xmm1, oword [rdx + 16 * 1]  ; +sNaN, -sNaN
vcvttpd2dq xmm2, oword [rdx + 16 * 2]  ; +infinity, -infinity
vcvttpd2dq xmm3, oword [rdx + 16 * 3]  ; +2^31, -2147483649.0

vcvttpd2dq xmm4, yword [rdx + 64 + 32 * 0] ; +/-DBL_MAX, +/-2^40
vcvttpd2dq xmm5, yword [rdx + 64 + 32 * 1] ; near-boundary valid values and overflow
vcvttpd2dq xmm6, yword [rdx + 64 + 32 * 2] ; qNaN, 42.75, -infinity, negative overflow
vcvttpd2dq xmm7, yword [rdx + 64 + 32 * 3] ; +overflow, -overflow, -42.75, zero

hlt

align 32
.pd_data:
 dq 0x7FF8000000000001, 0xFFF8000000000002
 dq 0x7FF0000000000001, 0xFFF0000000000002
 dq 0x7FF0000000000000, 0xFFF0000000000000
 dq 0x41E0000000000000, 0xC1E0000000200000

 dq 0x7FEFFFFFFFFFFFFF, 0xFFEFFFFFFFFFFFFF, 0x4270000000000000, 0xC270000000000000
 dq 0x41DFFFFFFFFFFFFF, 0xC1E00000001FFFFF, 0xC1E0000000000000, 0x41E0000000000000
 dq 0x7FF8000000000001, 0x4045600000000000, 0xFFF0000000000000, 0xC1E0000000200000
 dq 0x41E0000000000000, 0xC270000000000000, 0xC045600000000000, 0x0000000000000000
