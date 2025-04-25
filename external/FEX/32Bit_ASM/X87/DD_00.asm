%ifdef CONFIG
{
  "RegData": {
    "MM7":  ["0x8000000000000000", "0x4000"]
  },
  "Mode": "32BIT"
}
%endif
org 10000h
bits 32

lea edx, [.data]

fld qword [edx + 8 * 0]
hlt

.data:
dq 0x4000000000000000 ; 2.0
