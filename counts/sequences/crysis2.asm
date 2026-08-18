bits 64

movss xmm0, dword [rel $+0xd842f]
movss xmm1, dword [rsp + 0x58]
movss xmm2, dword [rsp + 0x68]
mov rsi, qword [rsp + 0xe8]
lea rcx, [rsp + 0x30]
movss xmm3, dword [rsp + 0x6c]
addss xmm1, dword [rsp + 0x64]
addss xmm2, dword [rsp + 0x5c]
mulss xmm1, xmm0
mulss xmm2, xmm0
addss xmm3, dword [rsp + 0x60]
movss dword [rsp + 0x30], xmm1
movss dword [rsp + 0x34], xmm2
mulss xmm3, xmm0
movss dword [rsp + 0x38], xmm3
mov eax, dword [rcx]
mov dword [rdi], eax
mov eax, dword [rcx + 4]
mov dword [rdi + 4], eax
mov eax, dword [rcx + 8]
mov dword [rdi + 8], eax
mov rax, rdi
add rsp, 0xd0
pop rdi
ret
