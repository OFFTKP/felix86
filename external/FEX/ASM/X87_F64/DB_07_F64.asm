%ifdef CONFIG
{
  "RegData": {
    "RAX": "0x4000000000000000"
  },
  "Env": { "FEX_X87REDUCEDPRECISION" : "1" }
}
%endif
bits 64

lea rdx, [rel data]
fld tword [rdx + 8 * 0]

lea rdx, [rel data2]
fstp tword [rdx + 8 * 0]
fld tword [rdx + 8 * 0]

mov rdx, 0xe0000000
fstp qword [rdx]
mov rax, [rdx]

hlt

align 8
data:
  dt 2.0
  dq 0
data2:
  dt 0.0
  dq 0
