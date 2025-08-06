%ifdef CONFIG
{
  "RegData": {
    "RAX": "0xFFFF",
    "RBX": "0x3FFF",
    "RCX": "0xFFFF"
  }
}
%endif
bits 64

_start:
finit
lea rdi, [rsp - 512]
lea rsi, [rsp - 1024]
lea r8, [rsp - 1024 - 512]
fnstenv [rdi]
mov rax, 0x3ff0000000000000
mov [rsp - 8], rax
fld qword [rsp - 8]
fnstenv [rsi]
fstp qword [rsp - 8]
fnstenv [r8]
xor eax, eax
xor ebx, ebx
xor ecx, ecx
mov ax, word [rdi + 8]
mov bx, word [rsi + 8]
mov cx, word [r8 + 8]

hlt