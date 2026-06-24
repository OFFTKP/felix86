%ifdef CONFIG
{
  "RegData": {
    "RAX": "0x0000000000000001",
    "RDX": "0x0000000000000000",
    "RBX": "0x0000000000000002"
  }
}
%endif
bits 64

mov rdx, 0x8000000000000000
mov rbx, 2
mulx rax, rdx, rbx

hlt
