%ifdef CONFIG
{
  "RegData": {
  },
  "Mode": "32BIT"
}
%endif
bits 32

; Clear OF just incase
test eax, eax

; Just ensure it executes safely
into

hlt
