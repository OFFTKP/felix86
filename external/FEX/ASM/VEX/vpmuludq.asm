%ifdef CONFIG
{
  "HostFeatures": ["AVX"],
  "RegData": {
    "XMM2": ["0x000000000003FFFC", "0x000000000000FFFE", "0x0000000000000000", "0x0000000000000000"],
    "XMM3": ["0x000000000003FFFC", "0x000000000000FFFE", "0x000000000003FFFC", "0x000000000000FFFE"],
    "XMM4": ["0x000000000003FFFC", "0x000000000000FFFE", "0x0000000000000000", "0x0000000000000000"],
    "XMM5": ["0x000000000003FFFC", "0x000000000000FFFE", "0x000000000003FFFC", "0x000000000000FFFE"]
  },
  "MemoryRegions": {
    "0x100000000": "4096"
  }
}
%endif
bits 64

lea rdx, [rel .data]

vmovapd ymm0, [rdx]
vmovapd ymm1, [rdx + 32]

vpmuludq xmm2, xmm0, [rdx + 32]
vpmuludq ymm3, ymm0, [rdx + 32]

vpmuludq xmm4, xmm0, xmm1
vpmuludq ymm5, ymm0, ymm1

hlt

align 32
.data:
dq 0x414243440000FFFF
dq 0x5152535400007FFF
dq 0x414243440000FFFF
dq 0x5152535400007FFF

dq 0x6162636400000004
dq 0x7172737400000002
dq 0x6162636400000004
dq 0x7172737400000002
