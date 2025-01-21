%ifdef CONFIG
{
  "RegData": {
    "RBX": "0x0000000000000001"
  }
}
%endif
bits 64

mov rax, 0x8000
test ah, 0x80
setne rbx

hlt