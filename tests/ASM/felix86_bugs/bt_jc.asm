%ifdef CONFIG
{
  "RegData": {
    "RBX": "0x0000000000000001"
  }
}
%endif
bits 64

xor rcx, rcx
bts rcx, 0
jc .label1
mov rbx, 1

.label1:
hlt