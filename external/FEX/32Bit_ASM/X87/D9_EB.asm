%ifdef CONFIG
{
  "RegData": {
    "MM7": ["0xC90FDAA22168C235", "0x4000"]
  },
  "Mode": "32BIT"
}
%endif
org 10000h
bits 32

fldpi

hlt
