%ifdef CONFIG
{
  "RegData": {
    "R14":"0x2000",
    "R13":"0x10",
    "R12":"0x4000",
    "R11":"0xC000",
    "R15":"0x6"
  }
}
%endif
bits 64

// todo: finish this test
global _start
_start:
finit
lea rsp, [rsp - 108]
fnstenv [rsp]
lea rdi, [rsp + 28]
lea rsi, [rel floats]
mov ecx, 80

rep movsb

// confirms that fsave and frstor save st(0) as the top float
frstor [rsp]
fincstp
fincstp
fsave [rsp - 108]

hlt

floats:
dt 1.0
dt 2.0
dt 3.0
dt 4.0
dt 5.0
dt 6.0
dt 7.0
dt 8.0
