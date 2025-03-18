%ifdef CONFIG
{
  "RegData": {
    "RAX": "0x2"
  },
  "Mode": "32BIT"
}
%endif
bits 32

mov eax, 0x1234
aam
aam 0xc
aam 0x1f
aam 0xff
hlt
