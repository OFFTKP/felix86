%ifdef CONFIG
{
  "RegData": {
    "RAX": "0x3ff0000000000000",
    "RBX": "0x3ff0000000000000"
  },
  "Env": { "FEX_X87REDUCEDPRECISION" : "1" }
}
%endif
bits 64

; Only tests pop behaviour
fld1
fldz
fucomp
fld1

mov rdx, 0xe0000000
fstp qword [rdx]
mov rax, [rdx]
fstp qword [rdx]
mov rbx, [rdx]


hlt
