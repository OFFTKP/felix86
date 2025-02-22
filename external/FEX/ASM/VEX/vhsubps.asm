%ifdef CONFIG
{
  "HostFeatures": ["AVX"],
  "RegData": {
    "XMM2": ["0x447B00003F800000", "0xC53FF000C1880000", "0x0000000000000000", "0x0000000000000000"],
    "XMM3": ["0x447B00003F800000", "0xC53FF000C1880000", "0x0000000000000000", "0x0000000000000000"],
    "XMM4": ["0x447B00003F800000", "0xC53FF000C1880000", "0xC2540000C2A80000", "0x43BF800000000000"],
    "XMM5": ["0x447B00003F800000", "0xC53FF000C1880000", "0xC2540000C2A80000", "0x43BF800000000000"]
  }
}
%endif
bits 64

lea rdx, [rel .data]

vmovaps ymm0, [rdx]
vmovaps ymm1, [rdx + 32]

vhsubps xmm2, xmm0, xmm1
vhsubps xmm3, xmm0, [rdx + 32]

vhsubps ymm4, ymm0, ymm1
vhsubps ymm5, ymm0, [rdx + 32]

hlt

align 32
.data:
dq 0x3F80000040000000 ; 1.0  , 2.0
dq 0x41A0000044800000 ; 20.0 , 1024.0
dq 0x42F0000042100000 ; 120.0, 36.0
dq 0x429C000041C80000 ; 78.0 , 25.0

dq 0x42A4000042820000 ; 82.0  , 65.0
dq 0x457FF00044800000 ; 4095.0, 1024.0
dq 0xC1A00000C1A00000 ; -20   , -20
dq 0xC2FE000043800000 ; -127.0, 256.0
