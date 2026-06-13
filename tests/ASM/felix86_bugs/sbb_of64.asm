%ifdef CONFIG
{
  "RegData": {
    "RAX": "0x7FFFFFFFFFFFFFFF",
    "RDI": "0x0000000000000800"
  }
}
%endif
bits 64

mov rax, 0x8000000000000000
mov rbx, 0
stc
sbb rax, rbx
pushfq
pop rdi
and rdi, 0x800

hlt
