bits 32

; Overcooked main menu block
.loop:
mov ecx,dword [ebp-0x14]
mov esi,dword [ecx]
mov dword [ebp+0xc],esi
movzx ecx,byte [ebp+0xc]
xorps xmm0,xmm0
cvtsi2sd xmm0,ecx
mov ecx,esi
shr ecx,0x8
movzx ecx,cl
xorps xmm1,xmm1
cvtsi2sd xmm1,ecx
mov ecx,esi
shr ecx,0x10
movzx ecx,cl
xorps xmm2,xmm2
cvtsi2sd xmm2,ecx
mov ecx,dword [ebp+0x1c]
movss xmm6,dword [ecx]
cvtps2pd xmm6,xmm6
divsd xmm0,xmm3
cvtpd2ps xmm0,xmm0
divsd xmm1,xmm3
cvtss2sd xmm0,xmm0
mulsd xmm0,xmm6
movss xmm6,dword [ecx+0x4]
cvtps2pd xmm6,xmm6
cvtpd2ps xmm1,xmm1
xorps xmm5,xmm5
divsd xmm2,xmm3
cvtss2sd xmm1,xmm1
mulsd xmm1,xmm6
movss xmm6,dword [ecx+0x8]
cvtpd2ps xmm2,xmm2
shr esi,0x18
cvtsi2sd xmm5,esi
divsd xmm5,xmm3
cvtps2pd xmm6,xmm6
cvtss2sd xmm2,xmm2
mulsd xmm2,xmm6
movss xmm6,dword [ecx+0xc]
cvtpd2ps xmm5,xmm5
cvtpd2ps xmm0,xmm0
comiss xmm4,xmm0
cvtss2sd xmm5,xmm5
cvtps2pd xmm6,xmm6
mulsd xmm5,xmm6
cvtpd2ps xmm1,xmm1
cvtpd2ps xmm2,xmm2
cvtpd2ps xmm5,xmm5
movss dword [ebp-0xb4],xmm0
lea ecx,[ebp-0x98]
ja .loop
