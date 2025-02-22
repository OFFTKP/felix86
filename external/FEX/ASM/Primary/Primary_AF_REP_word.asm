%ifdef CONFIG
{
  "RegData": {
    "RCX": "6",
    "RDI": "0xE0000004"
  },
  "MemoryRegions": {
    "0x100000000": "4096"
  }
}
%endif
bits 64

mov rdx, 0xe0000000

mov rax, 0x4142434445466162
mov [rdx + 8 * 0], rax
mov rax, 0x5152535455565758
mov [rdx + 8 * 1], rax
mov rax, 0x0
mov [rdx + 8 * 2], rax

lea rdi, [rdx + 8 * 0]

cld
mov rax, 0x6162
mov rcx, 8
cmp rax, 0x6162

rep scasw

hlt
