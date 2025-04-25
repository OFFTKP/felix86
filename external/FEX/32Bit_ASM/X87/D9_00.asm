%ifdef CONFIG
{
  "RegData": {
    "MM7":  ["0x8000000000000000", "0x3fff"]
  },
  "Mode": "32BIT"
}
%endif
org 10000h
bits 32

lea edx, [.data]

fld dword [edx + 8 * 0]
hlt

.data:
dq 0x3f800000
