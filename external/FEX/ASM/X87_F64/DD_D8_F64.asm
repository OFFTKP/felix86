%ifdef CONFIG
{
  "RegData": {
    "RAX": "0x4010000000000000",
    "RBX": "0x3ff0000000000000"
  },
  "Env": { "FEX_X87REDUCEDPRECISION" : "1" }
}
%endif
bits 64

lea rdx, [rel data]
fld tword [rdx + 8 * 0]

lea rdx, [rel data2]
fld tword [rdx + 8 * 0]
fstp st1

lea rdx, [rel data3]
fld tword [rdx + 8 * 0]

fstp qword [rdx]
mov rax, [rdx]
fstp qword [rdx]
mov rbx, [rdx]

hlt

align 8
data:
  dt 2.0
  dq 0
data2:
  dt 1.0
  dq 0
data3:
  dt 4.0
  dq 0

