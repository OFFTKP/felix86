%ifdef CONFIG
{
  "RegData": {
    "RCX": "0x10000"
  },
  "Mode": "32BIT"
}
%endif
org 10000h
bits 32

mov ecx, 0x10

.loop:
dec ecx
jecxz .end
jmp .loop
.end:

mov ecx, 0x1FFFF

.loop2:
dec cx
jcxz .end2
jmp .loop2
.end2:

hlt
