%ifdef CONFIG
{
  "RegData": {
    "RAX": "0x123456789a0cd0f1"
  }
}
%endif
bits 64

mov rax, 0x123456789abcdef1
mov [rsp], rax
mov ebx, 0x0FF0
lock and [rsp + 1], ebx
mov rax, [rsp]

hlt