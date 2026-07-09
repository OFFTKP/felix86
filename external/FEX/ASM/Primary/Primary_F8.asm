%ifdef CONFIG
{
  "RegData": {
    "RAX": "0x0"
  }
}
%endif
bits 64

clc

; Get CF
sbb rax, rax
and rax, 1

hlt
