%ifdef CONFIG
{
  "RegData": {
    "MM7": ["0xD49A784BCD1B8AFE", "0x4000"]
  },
  "Mode": "32BIT"
}
%endif
org 10000h
bits32

fldl2t

hlt
