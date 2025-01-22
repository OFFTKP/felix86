%ifdef CONFIG
{
  "RegData": {
    "RAX": "0x17",
    "RBX": "0x7",
    "RCX": "0xF"
  }
}
%endif
bits 64

mov rax, 0xabcdef
mov ebx, 0xea
mov ecx, 0x9090

bsr rax, rax
bsr ebx, ebx
bsr cx, cx

hlt