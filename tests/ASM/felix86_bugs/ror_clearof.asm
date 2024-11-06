%ifdef CONFIG
{
  "RegData": {
    "R14": "0x0000000000000000"
  }
}
%endif
bits 64

pushfq
or qword [rsp], 0x800
popfq
mov r14, -1
mov r12, 0
ror r12, 1
mov r14, 0
mov r13, 1
cmovo r14, r13
hlt