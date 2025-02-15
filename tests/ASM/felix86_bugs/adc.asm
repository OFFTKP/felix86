%ifdef CONFIG
{
  "RegData": {
    "RAX": "0x0000000000000080",
    "RDI": "0x0000000000000000"
  }
}
%endif
bits 64

mov rax, -75
mov rbx, -54
movzx eax, al
movzx ebx, bl
stc
adc al, bl

pushfq
pop rdi
and rdi, 0x800

hlt