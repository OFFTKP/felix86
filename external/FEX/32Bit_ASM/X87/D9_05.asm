%ifdef CONFIG
{
  "Mode": "32BIT"
}
%endif
org 10000h
bits 32

mov edx, 0xe0000000
; Just to ensure execution
fldcw [edx]

hlt
