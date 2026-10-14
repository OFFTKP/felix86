bits 64

.loop:
movups xmm3, [rbp + r11 - 0x10]
movups xmm5, [rbp + r11 - 0x50]
movaps xmm1, xmm3
mulps xmm1, [rbp + 0x60]
movaps xmm0, xmm5
mulps xmm0, [rbp + 0x80]
movaps xmm2, xmm3
shufps xmm3, [rel $+0x1ad8d68], 0x50
addps xmm1, [rbp + 0x70]
cmpleps xmm2, xmm13
psrad xmm2, 0x1f
shufps xmm5, xmm10, 0x50
shufps xmm5, xmm3, 0x88
addps xmm1, xmm11
pand xmm5, xmm2
addps xmm1, xmm0
movaps xmm0, xmm1
shufps xmm0, xmm1, 0xff
maxps xmm0, xmm6
divps xmm1, xmm0
movaps xmm4, xmm1
movaps xmm0, xmm1
shufps xmm0, xmm1, 0x55
shufps xmm4, xmm1, 0xaa
mulps xmm0, xmm15
shufps xmm1, xmm1, 0x0
mulps xmm1, xmm12
mulps xmm4, xmm7
addps xmm4, [rsp + 0x70]
addps xmm4, xmm0
addps xmm4, xmm1
movaps xmm0, xmm4
movaps xmm1, xmm4
shufps xmm0, xmm4, 0xff
maxps xmm0, xmm6
shufps xmm4, xmm4, 0xaa
movdqa xmm6, xmm14
divps xmm1, xmm0
movdqa xmm0, xmm2
pand xmm6, xmm2
pandn xmm0, xmm1
pandn xmm2, xmm4
por xmm5, xmm0
por xmm6, xmm2
movaps xmm0, [rel $+0x1aee899]
xorps xmm2, xmm2
cmpleps xmm0, xmm5
shufps xmm0, xmm0, 0x44
movaps xmm1, xmm5
cmpleps xmm1, xmm2
por xmm1, xmm0
movdqa xmm4, xmm9
movaps xmm3, xmm1
movaps xmm0, xmm1
shufps xmm0, xmm1, 0xaa
shufps xmm3, xmm1, 0xff
por xmm3, xmm0
movaps xmm0, xmm1
shufps xmm0, xmm1, 0x55
por xmm3, xmm0
shufps xmm1, xmm1, 0x0
por xmm3, xmm1
psrad xmm3, 0x1f
pand xmm4, xmm3
movdqa xmm1, xmm3
pandn xmm1, xmm5
movdqa xmm0, xmm4
por xmm0, xmm1
movdqa xmm1, [rel $+0x1aee408]
cvttps2dq xmm2, xmm0
movdqa xmm0, xmm2
movd eax, xmm2
psrldq xmm0, 0x4
pand xmm1, xmm2
movd ecx, xmm0
movd edx, xmm1
shl ecx, 0x8
sub ecx, edx
add ecx, eax
movsxd r8, ecx
prefetcht0 byte [rbx + r8*4]
movsxd rax, edx
movdqa xmm1, xmm8
add rax, rax
add r11, 0x10
pandn xmm3, [rsp + rax*8 + 0x30]
por xmm4, xmm3
movups xmm3, [rbx + r8*4]
movdqa xmm0, xmm4
psrad xmm4, 0x1f
psrad xmm0, 0x1f
movdqa xmm2, xmm3
pand xmm2, xmm0
pandn xmm0, xmm9
por xmm2, xmm0
movaps xmm0, xmm6
cmpleps xmm2, xmm9
psrad xmm2, 0x1f
pand xmm1, xmm2
pandn xmm2, xmm3
por xmm1, xmm2
cmpleps xmm0, xmm1
psrad xmm0, 0x1f
pand xmm6, xmm0
pandn xmm0, xmm1
por xmm6, xmm0
pand xmm6, xmm4
pandn xmm4, xmm3
por xmm6, xmm4
movdqu [rbx + r8*4], xmm6
movaps xmm6, [rel $+0x1ad8bb8]
sub rdi, 0x1
jne near .loop
