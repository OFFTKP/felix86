%ifdef CONFIG
{
  "RegData": {
    "MM6": ["0x8000000000000000", "0x4001"],
    "MM7": ["0x8000000000000000", "0x3FFF"]
  }
}
%endif
bits 64

; Tests undocumented fstp implementation at 0xdf, 0xd8+i

lea rdx, [rel data]
fld tword [rdx + 8 * 0]

lea rdx, [rel data2]
fld tword [rdx + 8 * 0]

db 0xdf, 0xd9

lea rdx, [rel data3]
fld tword [rdx + 8 * 0]

hlt

align 4096
data:
  dt 2.0
  dq 0
data2:
  dt 1.0
  dq 0
data3:
  dt 4.0
  dq 0

