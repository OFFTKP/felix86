%ifdef CONFIG
{
  "HostFeatures": ["AVX"],
  "RegData": {
    "XMM0": ["0x4142434485868788", "0x5152535455565758", "0x4142434485868788", "0x5152535455565758"],
    "XMM1": ["0xFFFFFFFFFFFF8788", "0xFFFFFFFFFFFF8586", "0x0000000000000000", "0x0000000000000000"],
    "XMM2": ["0xFFFFFFFFFFFF8788", "0xFFFFFFFFFFFF8586", "0x0000000000004344", "0x0000000000004142"],
    "XMM3": ["0xFFFFFFFFFFFF8788", "0xFFFFFFFFFFFF8586", "0x0000000000000000", "0x0000000000000000"],
    "XMM4": ["0xFFFFFFFFFFFF8788", "0xFFFFFFFFFFFF8586", "0x0000000000004344", "0x0000000000004142"]
  }
}
%endif
bits 64

lea rdx, [rel .data]

vmovapd ymm0, [rdx]

; Memory operands
vpmovsxwq xmm1, [rdx]
vpmovsxwq ymm2, [rdx]

; Register only
vpmovsxwq xmm3, xmm0
vpmovsxwq ymm4, xmm0

hlt

align 32
.data:
dq 0x4142434485868788
dq 0x5152535455565758
dq 0x4142434485868788
dq 0x5152535455565758
