%ifdef CONFIG
{
  "RegData": {
    "RAX": "0x1337",
    "R12": "0x7775",
    "XMM0": ["0x7f8080807f808080", "0x7f8080807f808080"],
    "XMM1": ["0xe4fa74c0e4fa74c0", "0x1168900da923c4e4"],
    "XMM2": ["0xf1a2d900f1a2d900", "0x1e10f44db5cc2924"],
    "XMM3": ["0x7fff800080008000", "0x7fff800080008000"]
  }
}
%endif
bits 64
global test
test:
xorps xmm0, xmm0
xorps xmm1, xmm1
xorps xmm2, xmm2
xorps xmm3, xmm3
xorps xmm4, xmm4
xorps xmm5, xmm5
xorps xmm6, xmm6
xor r12d, r12d

mov r9,  0x104e8954c4295024
mov r10, 0x9adb4e4d2c6e1b4d

mov rax, 0x123456789abcdef0
movq xmm1, rax

movd       xmm2, r9d
movd       xmm3, r10d
punpcklqdq xmm3, xmm2
shufps     xmm0, xmm3, 0x24
pshufd     xmm1, xmm1, 0
psrad      xmm1, 8
pslld      xmm1, 4
movaps     xmm2, xmm0
paddd      xmm2, xmm1
movdqa     xmm3, xmm2
paddd      xmm3, xmm1
paddd      xmm1, xmm3
movaps     xmm4, xmm0
packssdw   xmm4, xmm2
movdqa     xmm5, xmm3
packssdw   xmm5, xmm1
packsswb   xmm4, xmm5
pmovmskb   r9d, xmm4
or         r12d, r9d
mov        r9d, 0xfffffffe
rol        r9d, cl
movd       xmm4, r8d
pshufd     xmm4, xmm4, 0
paddd      xmm0, xmm4
paddd      xmm2, xmm4
packssdw   xmm0, xmm2
paddd      xmm3, xmm4
paddd      xmm1, xmm4
packssdw   xmm3, xmm1
packsswb   xmm0, xmm3
pmovmskb   ecx, xmm0
or         eax, ecx
inc        edi
and        edx, r9d
cmp        r12d, 0xffff
jne        .success
jmp        .failure

.failure:
mov rax, 0xdead
hlt

.success:
mov rax, 0x1337
hlt