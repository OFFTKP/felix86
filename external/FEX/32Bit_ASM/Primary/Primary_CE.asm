%ifdef CONFIG
{
  "RegData": {
  },
  "Mode": "32BIT"
}
%endif
org 10000h
bits 32

; Clear OF just incase
test eax, eax

; Just ensure it executes safely
into

hlt
