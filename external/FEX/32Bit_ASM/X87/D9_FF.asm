%ifdef CONFIG
{
  "RegData": {
    "MM7":  ["0xD51132BA9B902522", "0xBFFD"]
  },
  "Mode": "32BIT"
}
%endif
bits 32

lea edx, [data]
fld tword [edx + 8 * 0]

fcos

hlt

align 8
data:
  dt 2.0
  dq 0
