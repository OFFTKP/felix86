%ifdef CONFIG
{
  "RegData": {
    "RBX": "0x9abcdef0",
    "R8": "0x9abcdef0",
    "XMM5": ["0x31244231", "0x0"],
    "XMM6": ["0x56123f00", "0x0"]
  },
  "MemoryRegions": {
    "0x100000000": "4096"
  }
}
%endif
bits 64
global my_func

my_func:
lea rdx, [rsp-1000]
and rdx, 0xFFFFFFFFFFFFFFF8

mov rax, 0x123456789ABCDEF0
mov [rdx + 8 * 0], rax
mov rax, 0x1255231231244231
mov [rdx + 8 * 1], rax

mov rax, 0
mov [rdx + 8 * 2], rax
mov [rdx + 8 * 3], rax

movapd xmm0, [rdx + 8 * 0]
movapd xmm1, [rdx + 8 * 0]
movapd xmm5, [rdx + 8 * 0]
movapd xmm6, [rdx + 8 * 0]

mov r9, 0x91FE2B1356123F00
mov r8, 0xdeadbeefdeadbeef

lea rsi, [rdx + 8]
lea rdi, [rdx + 8*2]

movd [rdi], xmm0
mov rbx, [rdi]
movd r8d, xmm1
movd xmm5, [rsi]
movd xmm6, r9d

hlt
