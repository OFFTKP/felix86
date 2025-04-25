%ifdef CONFIG
{
  "RegData": {
    "RAX": "0",
    "RBX": "0x00000000FFFE0000"
  },
  "Mode": "32BIT"
}
%endif
org 10000h
bits 32

sgdt [rel data]

movzx eax, word [rel data]
mov ebx, dword [rel data + 2]
hlt

data:
; Limit
dw 0
; Base
dd 0
