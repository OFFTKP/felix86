bits 64

fld dword [rcx]
fmul dword [rax]
fstp dword [rbp+0x14]
fld dword [rbp+0x14]
fld dword [rcx+0x4]
fmul dword [rax+0x4]
fsubp st1
fstp dword [rbp+0x14]
fld dword [rbp+0x14]
fld dword [rcx+0x8]
fmul dword [rax+0x8]
faddp st1
fstp dword [rbp+0x14]
fld dword [rbp+0x14]
fld dword [rcx+0xc]
fmul dword [rax+0xc]
fsubp st1
fstp dword [rbp+0x14]
fld dword [rbp+0x14]
fld dword [rcx+0x10]
fmul dword [rax+0x10]
faddp st1
fstp dword [rbp+0x14]
fld dword [rbp+0x14]
fld dword [rcx+0x14]
fmul dword [rax+0x14]
fsubp st1
fstp dword [rbp+0x14]
fld dword [rbp+0x14]
fld dword [rcx+0x18]
fmul dword [rax+0x18]
faddp st1
fstp dword [rbp+0x14]
fld dword [rbp+0x14]
fld dword [rcx+0x1c]
fmul dword [rax+0x1c]
fsubp st1
fstp dword [rbp+0x14]
fld dword [rbp+0x14]
fld dword [rcx+0x20]
fmul dword [rax+0x20]
faddp st1
fstp dword [rbp+0x14]
fld dword [rbp+0x14]
fld dword [rcx+0x24]
fmul dword [rax+0x24]
fsubp st1
fstp dword [rbp+0x14]
fld dword [rbp+0x14]
fld dword [rcx+0x28]
fmul dword [rax+0x28]
faddp st1
fstp dword [rbp+0x14]
fld dword [rbp+0x14]
fld dword [rcx+0x2c]
fmul dword [rax+0x2c]
fsubp st1
fstp dword [rbp+0x14]
fld dword [rbp+0x14]
fld dword [rcx+0x30]
fmul dword [rax+0x30]
faddp st1
fstp dword [rbp+0x14]
fld dword [rbp+0x14]
fld dword [rcx+0x34]
fmul dword [rax+0x34]
fsubp st1
fstp dword [rbp+0x14]
fld dword [rbp+0x14]
fld dword [rcx+0x38]
fmul dword [rax+0x38]
faddp st1
fstp dword [rbp+0x14]
fld dword [rbp+0x14]
fld dword [rcx+0x3c]
fmul dword [rax+0x3c]
fsubp st1
fstp dword [rbp+0x14]
fld dword [rbp+0x14]
fmul qword [rel $+0xdc8488]
fstp dword [rbp+0x14]
fld dword [rbp+0x14]
fistp dword [rbp+0x10]
cmp dword [rbp+0x10],ebx
jle 0xb71402
