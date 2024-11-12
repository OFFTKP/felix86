%ifdef CONFIG
{
  "RegData": {
    "XMM0": ["0xFFFFFFFFFFFFFFFF", "0"],
    "RAX": "0x00FF"
  },
  "MemoryRegions": {
    "0x100000000": "4096"
  }
}
%endif
bits 64

mov rdx, 0xe0000000

mov rax, 0xFFFFFFFFFFFFFFFF
mov [rdx + 8 * 0], rax
mov rax, 0
mov [rdx + 8 * 1], rax

movapd xmm0, [rdx + 8 * 0]

pmovmskb eax, xmm0

hlt
