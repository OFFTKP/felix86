%ifdef CONFIG
{
  "RegData": {
    "RBX": "0x0000000000000001",
    "RCX": "0x1234"
  }
}
%endif
bits 64

xor ecx, ecx
lea rsi, [rax + rbx]
lea r14, [rax + 0x10]
cmp r12, r15
setnz cl
sub rdx, rbx
mov [r12 + 0x60], rsi
shl rcx, 0x2
or rdx, 0x1
or rcx, rbx
or rcx, 0x1
mov [rax + 0x8], rcx
mov [rsi + 0x8], rdx
hlt