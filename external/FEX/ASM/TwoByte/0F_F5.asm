%ifdef CONFIG
{
  "RegData": {
    "MM0": "0x2A9FE7742F697C44"
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

mov rax, 0x0
mov [rdx + 8 * 4], rax
mov [rdx + 8 * 5], rax

mov eax, dword [rdx + 8 * 0]
mov rbx, qword [rdx + 8 * 1]

movq mm0, [rdx + 8 * 0]

pmaddwd mm0, [rdx + 8 * 1]

hlt
