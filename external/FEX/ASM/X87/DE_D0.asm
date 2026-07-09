%ifdef CONFIG
{
  "RegData": {
    "MM6":  ["0x8000000000000000", "0x3FFF"],
    "MM7":  ["0x8000000000000000", "0x3FFF"]
  }
}
%endif
bits 64

; Only tests pop behaviour
; Tests undocumented fcomp implementation at 0xde, 0xd0+i
finit
fld1
fldz
; fcomp
db 0xde, 0xd1
fld1

hlt
