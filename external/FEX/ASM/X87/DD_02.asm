%ifdef CONFIG
{
  "RegData": {
    "RAX": "0x4000000000000000",
    "MM6": ["0x8000000000000000", "0x3FFF"],
    "MM7": ["0x8000000000000000", "0x4000"]
  },
  "MemoryRegions": {
    "0x100000000": "4096"
  }
}
%endif
bits 64

lea rdx, [rel data]
fld tword [rdx + 8 * 0]

lea rdx, [rel data3]
fst qword [rdx + 8 * 0]

mov rax, [rdx + 8 * 0]

lea rdx, [rel data2]
fld tword [rdx + 8 * 0]

hlt

align 8
data:
  dt 2.0
  dq 0
data2:
  dt 1.0
  dq 0
data3:
  dq 0
  dq 0
