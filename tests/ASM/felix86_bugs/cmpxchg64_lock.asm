%ifdef CONFIG
{
  "RegData": {
    "RBX": "0xDEADBEEFDEADBEEF",
    "RDX": "0x123456789ABCDEF0"
  }
}
%endif
bits 64

lea rsp, [rsp-16]
mov rax, 0x123456789abcdef0

; True
mov [rsp], rax
mov rcx, 0xdeadbeefdeadbeef
cmpxchg [rsp], rcx

; False
mov [rsp+8], rax
xor eax, eax
mov rcx, 0xcafebabecafebabe
cmpxchg [rsp+8], rcx

mov rbx, [rsp]
mov rdx, [rsp+8]

hlt