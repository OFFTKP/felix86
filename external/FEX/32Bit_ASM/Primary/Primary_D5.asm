%ifdef CONFIG
{
  "RegData": {
    "RAX": "0xe8"
  },
  "Mode": "32BIT"
}
%endif
bits 32

mov eax, 0x1234
aad
aad 0x3
aad 0x1f
aad 0xae
hlt
