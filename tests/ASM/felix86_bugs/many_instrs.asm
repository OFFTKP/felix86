%ifdef CONFIG
{
  "RegData": {
    "RAX": "500"
  }
}
%endif
bits 64

; Ensure our big block detection and handling works
xor eax, eax
%rep 500
    add eax, 1
%endrep
hlt