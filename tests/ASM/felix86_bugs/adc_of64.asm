%ifdef CONFIG
{
  "RegData": {
    "RAX": "0x7FFFFFFFFFFFFFFF",
    "RDI": "0x0000000000000000"
  }
}
%endif
bits 64

mov rax, 0x7FFFFFFFFFFFFFFF
mov rbx, -1
stc
adc rax, rbx
pushfq
pop rdi
and rdi, 0x800

hlt
