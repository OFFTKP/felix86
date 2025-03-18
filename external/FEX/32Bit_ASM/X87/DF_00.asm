%ifdef CONFIG
{
  "RegData": {
    "MM7": ["0x8000000000000000", "0x4009"]
  },
  "Mode": "32BIT"
}
%endif
bits 32

lea edx, [.data]

fild word [edx + 8 * 0]

hlt

.data:
dq 1024
dq -1
