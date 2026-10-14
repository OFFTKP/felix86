bits 32

; Overcooked main menu block
.loop:
movss xmm0,dword [edi-0x8]
movss dword [ebp-0x108],xmm0
movss xmm0,dword [edi-0x4]
movss dword [ebp-0x104],xmm0
movss xmm0,dword [edi]
movss dword [ebp-0x100],xmm0
movss xmm0,dword [edi+0x4]
movss dword [ebp-0xfc],xmm0
lea ecx,[ebp-0x108]
movss xmm0,dword [ecx]
movss xmm1,dword [ecx+0x4]
movss xmm5,dword [ebp-0x78]
movss xmm2,dword [ecx+0x8]
cvtps2pd xmm6,xmm0
cvtps2pd xmm5,xmm5
mulsd xmm5,xmm6
movss xmm6,dword [ebp-0x68]
cvtps2pd xmm6,xmm6
cvtps2pd xmm7,xmm1
mulsd xmm6,xmm7
addsd xmm5,xmm6
movss xmm6,dword [ebp-0x58]
movss xmm4,dword [ecx+0xc]
cvtps2pd xmm6,xmm6
mov ecx,dword [ebp-0x1c]
add dword [ebp-0x8],ecx
mov ecx,dword [ebp-0x24]
add dword [ebp-0x14],ecx
cvtps2pd xmm7,xmm2
mulsd xmm6,xmm7
addsd xmm5,xmm6
mov ecx,dword [ebp-0x2c]
cvtsd2ss xmm5,xmm5
movss dword [ebp-0x114],xmm5
movss xmm5,dword [ebp-0x74]
cvtps2pd xmm6,xmm0
add dword [ebp-0x18],ecx
mov ecx,dword [ebp-0xc]
cvtps2pd xmm7,xmm1
add edx,dword [ebp-0xbc]
add dword [ebp-0x4],ecx
add edi,dword [ebp-0x34]
cvtps2pd xmm5,xmm5
mulsd xmm5,xmm6
movss xmm6,dword [ebp-0x64]
cvtps2pd xmm6,xmm6
mulsd xmm6,xmm7
addsd xmm5,xmm6
movss xmm6,dword [ebp-0x54]
cvtps2pd xmm0,xmm0
cvtps2pd xmm6,xmm6
cvtps2pd xmm7,xmm2
mulsd xmm6,xmm7
addsd xmm5,xmm6
movss xmm6,dword [ebp-0x70]
cvtps2pd xmm6,xmm6
mulsd xmm6,xmm0
movss xmm0,dword [ebp-0x60]
cvtps2pd xmm0,xmm0
cvtps2pd xmm1,xmm1
mulsd xmm0,xmm1
addsd xmm6,xmm0
movss xmm0,dword [ebp-0x50]
cvtps2pd xmm0,xmm0
cvtsd2ss xmm5,xmm5
cvtps2pd xmm1,xmm2
mulsd xmm0,xmm1
movss xmm1,dword [ebp-0x114]
addsd xmm6,xmm0
xorps xmm0,xmm0
cvtsd2ss xmm0,xmm6
movss dword [ebp-0xc8],xmm0
movss dword [ebp-0xd0],xmm1
movss dword [ebp-0xcc],xmm5
movq xmm0,qword [ebp-0xd0]
movq qword [eax+0x18],xmm0
movss dword [ebp-0xc4],xmm4
movq xmm0,qword [ebp-0xc8]
movq qword [eax+0x20],xmm0
add eax,0x3c
dec ebx
jne .loop
