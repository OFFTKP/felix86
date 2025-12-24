%ifdef CONFIG
{
  "RegData": {
    "RAX": "0x1234567890abefcd",
  }
}
%endif
bits 64

mov rax, 0x1234567890abcdef
bswap ax

hlt