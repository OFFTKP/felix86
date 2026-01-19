%ifdef CONFIG
{
  "RegData": {
    "RAX": "0x123456789a0cd0f1"
  }
}
%endif
bits 64

lea rsp, [rsp - 16]
mov rax, 0x123456789abcdef1
mov [rsp], rax
mov ebx, 0x0FF0
lock and [rsp + 1], bx
mov rax, [rsp]

hlt