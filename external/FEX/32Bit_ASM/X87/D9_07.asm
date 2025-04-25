%ifdef CONFIG
{
  "RegData": {
    "RAX": "0x37F"
  },
  "Mode": "32BIT"
}
%endif
org 10000h
bits 32

mov edx, 0xe0000000
fnstcw [edx]
mov eax, 0
mov ax, [edx]

hlt
