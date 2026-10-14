bits 64

; Hot block that happens during final boss fight to calculate cloth physics
.loop:
movaps xmm0, [rel $+0xb3239]
movzx eax, word [r14-0x4]
movaps xmm7, xmm0
movzx edx, word [r14]
movaps xmm8, xmm0
movzx r9d, word [r14+0x4]
movaps xmm9, xmm0
movzx r11d, word [r14+0x8]
add rax, rax
movzx ecx, word [r14-0x2]
add rdx, rdx
movzx r8d, word [r14+0x2]
add r9, r9
movzx r10d, word [r14+0x6]
add r11, r11
movaps xmm11, [r15+rax*8]
add rcx, rcx
movaps xmm13, [r15+rdx*8]
add r8, r8
movaps xmm15, [r15+r9*8]
add r10, r10
movaps xmm10, [r15+r11*8]
movzx ebx, word [r14+0xa]
movaps xmm12, [r15+rcx*8]
add rbx, rbx
movaps xmm1, [r15+r10*8]
movaps xmm14, [r15+r8*8]
movaps [rsp+0x30], xmm1
movaps xmm2, [r15+rbx*8]
movaps [rsp+0x40], xmm10
mulps xmm10, xmm0
mulps xmm7, xmm11
mulps xmm8, xmm13
addps xmm10, xmm2
mulps xmm9, xmm15
addps xmm7, xmm12
movaps [rsp+0x50], xmm2
movaps xmm2, [rsi]
addps xmm8, xmm14
addps xmm9, xmm1
movaps xmm3, xmm7
movaps xmm6, xmm7
movaps xmm0, xmm8
movaps xmm5, xmm8
unpcklps xmm0, xmm10
unpcklps xmm3, xmm9
movaps xmm1, xmm3
unpckhps xmm5, xmm10
unpcklps xmm1, xmm0
unpckhps xmm3, xmm0
mulps xmm1, xmm1
mulps xmm3, xmm3
unpckhps xmm6, xmm9
movaps xmm0, xmm6
unpckhps xmm6, xmm5
unpcklps xmm0, xmm5
movaps xmm5, [rel $+0xbdb67]
mulps xmm0, xmm0
addps xmm0, xmm3
movaps xmm3, [rel $+0xb313a]
addps xmm6, xmm3
addps xmm0, xmm1
movaps xmm1, [rsp+0x10]
addps xmm0, xmm3
rsqrtps xmm0, xmm0
mulps xmm0, xmm2
cmpnleps xmm2, xmm3
subps xmm4, xmm0
andps xmm4, xmm2
movaps xmm0, xmm4
minps xmm0, [rsp]
maxps xmm1, xmm0
rcpps xmm0, xmm6
mulps xmm1, [rsp+0x20]
mulps xmm0, [rdi]
subps xmm4, xmm1
mulps xmm4, xmm0
movaps xmm3, xmm4
andps xmm3, xmm5
andnps xmm5, xmm4
movaps xmm1, xmm3
movaps xmm4, [rel $+0x8cbde]
movaps xmm2, xmm5
shufps xmm1, xmm3, 0xc0
movaps xmm0, xmm11
shufps xmm0, xmm11, 0xff
add r14, 0x10
mulps xmm1, xmm7
add rsi, 0x10
shufps xmm2, xmm5, 0x2a
mulps xmm2, xmm9
shufps xmm3, xmm3, 0xd5
mulps xmm0, xmm1
mulps xmm3, xmm8
shufps xmm5, xmm5, 0x3f
mulps xmm5, xmm10
addps xmm0, xmm11
movaps [r15+rax*8], xmm0
movaps xmm0, xmm12
shufps xmm0, xmm12, 0xff
mulps xmm0, xmm1
movaps xmm1, [rsp+0x30]
subps xmm12, xmm0
movaps xmm0, xmm13
shufps xmm0, xmm13, 0xff
mulps xmm0, xmm3
movaps [r15+rcx*8], xmm12
addps xmm0, xmm13
movaps [r15+rdx*8], xmm0
movaps xmm0, xmm14
shufps xmm0, xmm14, 0xff
mulps xmm0, xmm3
subps xmm14, xmm0
movaps xmm0, xmm15
shufps xmm0, xmm15, 0xff
mulps xmm0, xmm2
movaps [r15+r8*8], xmm14
addps xmm0, xmm15
movaps [r15+r9*8], xmm0
movaps xmm0, xmm1
shufps xmm0, xmm1, 0xff
mulps xmm0, xmm2
subps xmm1, xmm0
movaps [r15+r10*8], xmm1
movaps xmm1, [rsp+0x40]
movaps xmm0, xmm1
shufps xmm0, xmm1, 0xff
mulps xmm0, xmm5
addps xmm0, xmm1
movaps xmm1, [rsp+0x50]
movaps [r15+r11*8], xmm0
movaps xmm0, xmm1
shufps xmm0, xmm1, 0xff
mulps xmm0, xmm5
subps xmm1, xmm0
movaps [r15+rbx*8], xmm1
cmp rsi, rbp
jne near .loop
