bits 64

mov    edi, eax
shr    eax, 0xb
imul   eax, edx
sub    edi, eax
mov    esi, ebp
sub    ebp, eax
cmovae eax, edi
movzx  edi, word [r11 + rbx*2 + 0x2]
cmovae ecx, edi
mov    edi, 0x1f
cmovb  ebp, esi
mov    esi, ebx
cmovb  edi, r10d
sbb    ebx, 0xffffffff
sub    edi, edx
sar    edi, 0x5
add    edi, edx
mov    word [r11 + rsi*1], di
movzx  edx, word [r11 + rbx*4]
add    ebx, ebx
cmp    eax, 0x1000000
jae    0xbeef