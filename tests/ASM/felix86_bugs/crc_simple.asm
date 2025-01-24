%ifdef CONFIG
{
  "RegData": {
    "RBX": "0"
  }
}
%endif
bits 64

mov rax, 0x1234567890ABCDEF
crc32 rbx, rax
hlt