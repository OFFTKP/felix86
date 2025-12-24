%ifdef CONFIG
{
  "RegData": {
    "RAX": "0x1234567890abefcd"
  }
}
%endif
bits 64

mov rax, 0x1234567890abcdef
db 0x66, 0x0f, 0xc8

hlt