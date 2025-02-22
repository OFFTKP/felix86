%ifdef CONFIG
{
  "HostFeatures": ["AVX"],
  "RegData": {
    "XMM0": ["0xFFFFFFFFEEEEEEEE", "0x1011121314151617", "0x0809AABBCCDDEEFF", "0x4041424344454647"],
    "XMM1": ["0x090A0B0C0D0E0F10", "0xCCEEDDAABBFF0990", "0x2021222324252627", "0x0062636465666768"],
    "XMM2": ["0x090A0B0BFBFCFDFE", "0xDCFFEFBDD0141FA7", "0x282ACCDEF1031526", "0x40A3A5A7A9ABADAF"],
    "XMM3": ["0x090A0B0BFBFCFDFE", "0xDCFFEFBDD0141FA7", "0x0000000000000000", "0x0000000000000000"],
    "XMM4": ["0x090A0B0BFBFCFDFE", "0xDCFFEFBDD0141FA7", "0x282ACCDEF1031526", "0x40A3A5A7A9ABADAF"],
    "XMM5": ["0x090A0B0BFBFCFDFE", "0xDCFFEFBDD0141FA7", "0x0000000000000000", "0x0000000000000000"]
  },
  "MemoryRegions": {
    "0x100000000": "4096"
  }
}
%endif
bits 64

lea rdx, [rel .data]

; Registers
vmovapd ymm0, [rdx]
vmovapd ymm1, [rdx + 32]
vpaddd ymm2, ymm0, ymm1
vpaddd xmm3, xmm0, xmm1

; Memory operand
vpaddd ymm4, ymm0, [rdx + 32]
vpaddd xmm5, xmm0, [rdx + 32]

hlt

align 32
.data:
dq 0xFFFFFFFFEEEEEEEE
dq 0x1011121314151617
dq 0x0809AABBCCDDEEFF
dq 0x4041424344454647

dq 0x090A0B0C0D0E0F10
dq 0xCCEEDDAABBFF0990
dq 0x2021222324252627
dq 0x0062636465666768
