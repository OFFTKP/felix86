%ifdef CONFIG
{
  "RegData": {
    "R8": "0x1",
    "R9": "0x0"
  }
}
%endif
bits 64

stc
sbb r14, r14
and r14, 1

mov r8, r14

clc
sbb r14, r14
and r14, 1

mov r9, r14

hlt