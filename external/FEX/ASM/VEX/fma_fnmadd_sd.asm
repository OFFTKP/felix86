%ifdef CONFIG
{
  "HostFeatures": ["AVX"],
  "RegData": {
    "XMM0": ["0x4026000000000000", "0x4008000000000000", "0", "0"],
    "XMM1": ["0x4050e00000000000", "0xc01c000000000000", "0", "0"],
    "XMM2": ["0x4049800000000000", "0x400c000000000000", "0", "0"]
  }
}
%endif
bits 64

vmovups ymm0, [rel .data]
vmovups ymm1, [rel .data2]
vmovups ymm2, [rel .data3]

vfnmadd231sd xmm0, xmm1, xmm2
vfnmadd213sd xmm1, xmm0, xmm2
vfnmadd132sd xmm2, xmm1, xmm0

hlt

align 32
.data:
dq 2.0, 3.0
dq 6.0, 7.0

.data2:
dq -6.0, -7.0
dq 20.0, 30.0

.data3:
dq 1.5, 3.5
dq -15.5, -21.5
