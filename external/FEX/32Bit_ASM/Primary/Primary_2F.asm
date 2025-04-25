%ifdef CONFIG
{
  "RegData": {
    "RAX": "0x12345607"
  },
  "Mode": "32BIT"
}
%endif
org 10000h
bits32

mov eax, 0x1234561f
das
das
das
das
hlt
