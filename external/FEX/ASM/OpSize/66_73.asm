%ifdef CONFIG
{
  "RegData": {
    "XMM0": ["0x0000717273747576", "0x0000414243444546"],
    "XMM1": ["0x7374757677780000", "0x4344454647480000"],
    "XMM2": ["0x0000717273747576", "0x0000414243444546"],
    "XMM3": ["0x7374757677780000", "0x4344454647480000"]
  },
  "MemoryRegions": {
    "0x100000000": "4096"
  }
}
%endif
bits 64

mov rdx, 0xe0000000

mov rax, 0x7172737475767778
mov [rdx + 8 * 0], rax
mov rax, 0x4142434445464748
mov [rdx + 8 * 1], rax

movapd xmm0, [rdx]
psrlq xmm0, 16

movapd xmm1, [rdx]
psllq xmm1, 16

movapd xmm2, [rdx]
psrldq xmm2, 2

movapd xmm3, [rdx]
pslldq xmm3, 2

hlt
