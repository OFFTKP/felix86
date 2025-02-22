%ifdef CONFIG
{
  "RegData": {
    "MM0": "0xA4A6ACAE84868C8E",
    "MM1": "0x84868C8EA4A6ACAE"
  }
}
%endif
bits 64

mov rdx, 0xe0000000

mov rax, 0x4142434445464748
mov [rdx + 8 * 0], rax
mov rax, 0x5152535455565758
mov [rdx + 8 * 1], rax

movq mm0, [rdx + 8 * 0]
movq mm1, [rdx + 8 * 1]

phaddw mm0, [rdx + 8 * 1]
phaddw mm1, [rdx + 8 * 0]

hlt
