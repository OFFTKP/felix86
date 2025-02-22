%ifdef CONFIG
{
  "RegData": {
    "XMM0": ["0x0000000000000000", "0x0000000000000000"],
    "XMM1": ["0x0000000000000000", "0x0000000000000000"],
    "XMM2": ["0x4200440046004800", "0x5200540056005800"],
    "XMM3": ["0xC2C4C6C8CACCCED0", "0xE2E4E6E8EAECEEF0"]
  },
  "MemoryRegions": {
    "0x100000000": "4096"
  }
}
%endif
bits 64

mov rdx, 0xe0000000

mov rax, 0x4142434445464748
mov [rdx + 8 * 0], rax
mov rax, 0x5152535455565758
mov [rdx + 8 * 1], rax

mov rax, 0x6162636465666768
mov [rdx + 8 * 2], rax
mov rax, 0x7172737475767778
mov [rdx + 8 * 3], rax

movapd xmm0, [rdx]
movapd xmm1, [rdx + 16]
movapd xmm2, [rdx]
movapd xmm3, [rdx + 16]

psllw xmm0, 32
psllw xmm1, 16
psllw xmm2, 8
psllw xmm3, 1

hlt
