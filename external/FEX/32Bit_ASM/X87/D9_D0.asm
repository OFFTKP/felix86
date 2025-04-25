%ifdef CONFIG
{
  "Mode": "32BIT"
}
%endif
org 10000h
bits 32

; Just to ensure execution
fnop
hlt
