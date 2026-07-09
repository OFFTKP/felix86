%ifdef CONFIG
{
  "RegData": {
    "XMM0":  ["0x538fb8fe", "0x0"],
    "XMM1":  ["0x538fb8fe", "0x0"],
    "XMM2":  ["0x538fb8fe", "0x0"],
    "XMM3":  ["0x538fb8fe", "0x0"],
    "XMM4":  ["0xd38fb8fe", "0x0"],
    "XMM5":  ["0xd38fb8fe", "0x0"],
    "XMM6":  ["0xd38fb8fe", "0x0"],
    "XMM7":  ["0xd38fb8fe", "0x0"]
  }
}
%endif
bits 64

lea rdx, [rel .data]
movaps xmm0, [rdx]
movaps xmm4, [rdx + 16]
roundss xmm3, xmm0, 0
roundss xmm2, xmm0, 1
roundss xmm1, xmm0, 2
roundss xmm0, xmm0, 3
roundss xmm7, xmm4, 0
roundss xmm6, xmm4, 1
roundss xmm5, xmm4, 2
roundss xmm4, xmm4, 3

hlt

.data:
dd 0x538fb8fe
dd 0
dd 0
dd 0

dd 0xd38fb8fe
dd 0
dd 0
dd 0
