%ifdef CONFIG
{
  "RegData": {
    "RAX": "0x0000000000000013",
    "RBX": "0x0000000000000035",
    "RCX": "0x0000000000000057",
    "RDX": "0x0000000000000079",
    "RSI": "0x000000000000001A",
    "RDI": "0x000000000000003C",
    "R8": "0x000000000000005E",
    "R9": "0x0000000000000070"
  }
}
%endif
bits 64

lea rsp, [rsp - 128]

mov byte[rsp + 0x00], 0x12
mov byte[rsp + 0x01], 0x34
mov byte[rsp + 0x02], 0x56
mov byte[rsp + 0x03], 0x78
mov byte[rsp + 0x04], 0x9a
mov byte[rsp + 0x05], 0xbc
mov byte[rsp + 0x06], 0xde
mov byte[rsp + 0x07], 0xf0

lock add byte[rsp + 0x00], 0x01
lock add byte[rsp + 0x01], 0x01
lock add byte[rsp + 0x02], 0x01
lock add byte[rsp + 0x03], 0x01
lock add byte[rsp + 0x04], 0x80
lock add byte[rsp + 0x05], 0x80
lock add byte[rsp + 0x06], 0x80
lock add byte[rsp + 0x07], 0x80

movzx rax, byte[rsp + 0x00]
movzx rbx, byte[rsp + 0x01]
movzx rcx, byte[rsp + 0x02]
movzx rdx, byte[rsp + 0x03]
movzx rsi, byte[rsp + 0x04]
movzx rdi, byte[rsp + 0x05]
movzx r8, byte[rsp + 0x06]
movzx r9, byte[rsp + 0x07]

hlt