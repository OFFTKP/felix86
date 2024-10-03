%ifdef CONFIG
{
  "HostFeatures": ["AVX"],
  "RegData": {
    "XMM2": ["0x00807F4100807F41", "0x00FF7F4100FF7F41", "0x0000000000000000", "0x0000000000000000"],
    "XMM3": ["0x00807F4100807F41", "0x00FF7F4100FF7F41", "0x0000000000000000", "0x0000000000000000"],
    "XMM4": ["0x00807F4100807F41", "0x00FF7F4100FF7F41", "0x00807F4100807F41", "0x00FF7F4100FF7F41"],
    "XMM5": ["0x00807F4100807F41", "0x00FF7F4100FF7F41", "0x00807F4100807F41", "0x00FF7F4100FF7F41"]
  },
  "MemoryRegions": {
    "0x100000000": "4096"
  }
}
%endif

lea rdx, [rel .data]

; 16bit signed -> 8bit signed (saturated)
; input > 0x7F(SCHAR_MAX, 127) = 0x7F(SCHAR_MAX, 127)
; input < 0x80(-127) = 0x80

vmovapd ymm0, [rdx]
vmovapd ymm1, [rdx + 32]

vpacksswb xmm2, xmm0, [rdx + 32]
vpacksswb xmm3, xmm0, xmm1

vpacksswb ymm4, ymm0, [rdx + 32]
vpacksswb ymm5, ymm0, ymm1

hlt

align 32
.data:
dq 0x00008000007F0041
dq 0x00008000007F0041
dq 0x00008000007F0041
dq 0x00008000007F0041

dq 0x0000FFFF007F0041
dq 0x0000FFFF007F0041
dq 0x0000FFFF007F0041
dq 0x0000FFFF007F0041
