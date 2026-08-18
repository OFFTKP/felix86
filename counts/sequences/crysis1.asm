bits 64

movsxd rdx, edx
movsxd r9, r9d
and rdx, r9
imul rdx, rax
movsxd rax, r8d
and rax, r9
add rdx, rax
mov rax, qword [rcx + 8]
movzx eax, word [rax + rdx*2]
and eax, 0xfffffff0
cvtsi2ss xmm0, rax
mulss xmm0, dword [rcx + 4]
addss xmm0, dword [rcx]
ret
