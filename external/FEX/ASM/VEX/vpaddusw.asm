%ifdef CONFIG
{
  "HostFeatures": ["AVX"],
  "RegData": {
    "XMM0": ["0xF142434445464748", "0x5152535455565758", "0xF142434445464748", "0x5152535455565758"],
    "XMM1": ["0x6162636465666768", "0x7172737475767778", "0x6162636465666768", "0x7172737475767778"],
    "XMM2": ["0xFFFFA6A8AAACAEB0", "0xC2C4C6C8CACCCED0", "0x0000000000000000", "0x0000000000000000"],
    "XMM3": ["0xFFFFA6A8AAACAEB0", "0xC2C4C6C8CACCCED0", "0xFFFFA6A8AAACAEB0", "0xC2C4C6C8CACCCED0"],
    "XMM4": ["0xFFFFA6A8AAACAEB0", "0xC2C4C6C8CACCCED0", "0x0000000000000000", "0x0000000000000000"],
    "XMM5": ["0xFFFFA6A8AAACAEB0", "0xC2C4C6C8CACCCED0", "0xFFFFA6A8AAACAEB0", "0xC2C4C6C8CACCCED0"]
  },
  "MemoryRegions": {
    "0x100000000": "4096"
  }
}
%endif
bits 64

lea rdx, [rel .data]

vmovaps ymm0, [rdx]
vmovaps ymm1, [rdx + 32]

vpaddusw xmm2, xmm0, xmm1
vpaddusw ymm3, ymm0, ymm1

vpaddusw xmm4, xmm0, [rdx + 32]
vpaddusw ymm5, ymm0, [rdx + 32]

hlt

align 32
.data:
dq 0xF142434445464748
dq 0x5152535455565758
dq 0xF142434445464748
dq 0x5152535455565758

dq 0x6162636465666768
dq 0x7172737475767778
dq 0x6162636465666768
dq 0x7172737475767778
