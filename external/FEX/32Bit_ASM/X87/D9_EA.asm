%ifdef CONFIG
{
  "RegData": {
    "MM7": ["0xB8AA3B295C17F0BC", "0x3FFF"]
  },
  "Mode": "32BIT"
}
%endif
org 10000h
bits 32

fldl2e

hlt
