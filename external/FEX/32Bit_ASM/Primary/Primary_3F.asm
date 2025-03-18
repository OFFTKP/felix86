%ifdef CONFIG
{
  "RegData": {
    "RAX": "0x12345107"
  },
  "Mode": "32BIT"
}
%endif
bits 32

mov eax, 0x1234561f
aas
aas
aas
aas
hlt
