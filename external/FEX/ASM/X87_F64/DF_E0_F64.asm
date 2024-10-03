%ifdef CONFIG
{
  "RegData": {
    "RAX": "0xFFFFFFFFFFFF3800",
    "RBX": "0xFFFFFFFFFFFF0000"
  },
  "Env": { "FEX_X87REDUCEDPRECISION" : "1" }
}
%endif

mov rdx, 0xe0000000

mov eax, 0x3f800000 ; 1.0
mov [rdx + 8 * 0], eax

mov rax, -1
mov rbx, -1
fnstsw ax
mov bx, ax

fld dword [rdx + 8 * 0]
fnstsw ax

hlt
