%ifdef CONFIG
{
  "RegData": {
    "MM7": ["0x8000000000000000", "0x3FFF"]
  },
  "Mode": "32BIT"
}
%endif
org 10000h
bits 32

fld1

hlt
