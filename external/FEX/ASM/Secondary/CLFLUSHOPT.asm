%ifdef CONFIG
{
  "RegData": {
    "RAX": "1"
  }
}
%endif
bits 64

mov rdx, 0xe0000000
; Just ensures the code is executed.
clflushopt [rdx]

mov rax, 1
hlt
