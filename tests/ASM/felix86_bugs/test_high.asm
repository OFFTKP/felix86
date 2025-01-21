%ifdef CONFIG
{
  "RegData": {
    "RBX": "0x0000000000000001",
    "RCX": "0x1234"
  }
}
%endif
bits 64

xor ebx, ebx
mov rax, 0x8000
test ah, 0x80
setne bl

mov rax, 0x12
mov rcx, 0x34
or ch, al

hlt