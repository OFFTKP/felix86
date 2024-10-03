%ifdef CONFIG
{
  "HostFeatures": ["AVX"],
  "RegData": {
    "XMM2": ["0x00000040FFFF8000", "0xFFFF800000000040", "0x0000000000000000", "0x0000000000000000"],
    "XMM3": ["0x00000040FFFF8000", "0xFFFF800000000040", "0x0000000000000000", "0x0000000000000000"],
    "XMM4": ["0x00000040FFFF8000", "0xFFFF800000000040", "0x00000040FFFF8000", "0xFFFF800000000040"],
    "XMM5": ["0x00000040FFFF8000", "0xFFFF800000000040", "0x00000040FFFF8000", "0xFFFF800000000040"]
  },
  "MemoryRegions": {
    "0x100000000": "4096"
  }
}
%endif

lea rdx, [rel .data]

; 32bit signed -> 16bit signed (saturated)
; input > 0x7FFF(SHRT_MAX, 32767) = 0x7FFF(SHRT_MAX, 32767)
; input < 0x8000(-32767) = 0x8000

vmovapd ymm0, [rdx]
vmovapd ymm1, [rdx + 32]

vpackssdw xmm2, xmm0, [rdx + 32]
vpackssdw xmm3, xmm0, xmm1

vpackssdw ymm4, ymm0, [rdx + 32]
vpackssdw ymm5, ymm0, ymm1

hlt

align 32
.data:
dq 0xFFFFFFFF80000000
dq 0x0000000000000040
dq 0xFFFFFFFF80000000
dq 0x0000000000000040

dq 0x0000000000000040
dq 0xFFFFFFFF80000000
dq 0x0000000000000040
dq 0xFFFFFFFF80000000
