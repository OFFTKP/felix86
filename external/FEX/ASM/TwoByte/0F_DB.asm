%ifdef CONFIG
{
  "RegData": {
    "MM0": "0x1010101010101010",
    "MM1": "0x1010101010101010",
    "MM2": "0x1010101010101010"
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
mov rax, 0x0
mov [rdx + 8 * 1], rax

mov rax, 0x1010101010101010
mov [rdx + 8 * 2], rax
mov rax, 0xFFFFFFFFFFFFFFFF
mov [rdx + 8 * 3], rax

movq mm0, [rdx]
pand mm0, [rdx + 8 * 2]

movq mm1, [rdx]
movq mm2, [rdx + 8 * 2]
pand mm1, mm2

hlt
