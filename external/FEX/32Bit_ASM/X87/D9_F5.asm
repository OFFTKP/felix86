%ifdef CONFIG
{
  "RegData": {
    "MM6":  ["0xE666666666666668", "0xBFFE"],
    "MM7":  ["0xC000000000000000", "0x4000"]
  },
  "Mode": "32BIT"
}
%endif
org 10000h
bits 32

lea edx, [data]
fld tword [edx + 8 * 0]

lea edx, [data2]
fld tword [edx + 8 * 0]

fprem1

hlt

align 8
data:
  dt 3.0
  dq 0
data2:
  dt 5.1
  dq 0
