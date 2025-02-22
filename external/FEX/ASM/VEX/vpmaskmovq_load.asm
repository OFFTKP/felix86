%ifdef CONFIG
{
  "HostFeatures": ["AVX"],
  "RegData": {
      "XMM0": ["0x8868C3F30AED56E0", "0x10FCE9E284E6E6DE", "0x1DDDDDDD8DDDDDDD", "0x8CCCCCCC0CCCCCCC"],
      "XMM1": ["0x0000000000000000", "0x0000000000000000", "0x0000000000000000", "0x0000000000000000"],
      "XMM2": ["0x8000000080000000", "0x8000000080000000", "0x8000000080000000", "0x8000000080000000"],
      "XMM3": ["0xA76C4F06A12BFCE0", "0x0000000000000000", "0x0000000000000000", "0xEEEEEEEEEEEEEEEE"],
      "XMM4": ["0xA76C4F06A12BFCE0", "0x0000000000000000", "0x0000000000000000", "0x0000000000000000"],
      "XMM5": ["0x0000000000000000", "0x0000000000000000", "0x0000000000000000", "0x0000000000000000"],
      "XMM6": ["0x0000000000000000", "0x0000000000000000", "0x0000000000000000", "0x0000000000000000"],
      "XMM7": ["0xA76C4F06A12BFCE0", "0x9B80767F1E6A060F", "0xFFFFFFFFFFFFFFFF", "0xEEEEEEEEEEEEEEEE"],
      "XMM8": ["0xA76C4F06A12BFCE0", "0x9B80767F1E6A060F", "0x0000000000000000", "0x0000000000000000"]
  }
}
%endif
bits 64

lea rdx, [rel .data]

vmovaps ymm0, [rdx + 32]
vmovaps ymm1, [rdx + 64]
vmovaps ymm2, [rdx + 96]

vpmaskmovq ymm3, ymm0, [rdx]
vpmaskmovq xmm4, xmm0, [rdx]

vpmaskmovq ymm5, ymm1, [rdx]
vpmaskmovq xmm6, xmm1, [rdx]

vpmaskmovq ymm7, ymm2, [rdx]
vpmaskmovq xmm8, xmm2, [rdx]

hlt

align 32
.data:
dq 0xA76C4F06A12BFCE0
dq 0x9B80767F1E6A060F
dq 0xFFFFFFFFFFFFFFFF
dq 0xEEEEEEEEEEEEEEEE

; Disastrously organized mask (sign mask [1, 0, 0, 1])
dq 0x8868C3F30AED56E0
dq 0x10FCE9E284E6E6DE
dq 0x1DDDDDDD8DDDDDDD
dq 0x8CCCCCCC0CCCCCCC

; No masking at all. Should not touch memory at all.
dq 0x0000000000000000
dq 0x0000000000000000
dq 0x0000000000000000
dq 0x0000000000000000

; Select all elements
dq 0x8000000080000000
dq 0x8000000080000000
dq 0x8000000080000000
dq 0x8000000080000000
