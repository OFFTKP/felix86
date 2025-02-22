
%ifdef CONFIG
{
  "HostFeatures": ["AVX"],
  "RegData": {
    "XMM0":  ["0xAAAAAAAABBBBBBBB", "0xCCCCCCCCDDDDDDDD", "0xEEEEEEEEFFFFFFFF", "0x9999999988888888"],
    "XMM1":  ["0x1111111122222222", "0x3333333344444444", "0x5555555566666666", "0x7777777788888888"],
    "XMM2":  ["0xAAAAAAAA22222222", "0xCCCCCCCCDDDDDDDD", "0x0000000000000000", "0x0000000000000000"],
    "XMM3":  ["0x11111111BBBBBBBB", "0x3333333344444444", "0x0000000000000000", "0x0000000000000000"]
  }
}
%endif
bits 64

lea rdx, [rel .data]

vmovapd ymm0,  [rdx]
vmovapd ymm1,  [rdx + 32]

vmovss xmm2, xmm0, xmm1
vmovss xmm3, xmm1, xmm0

hlt

align 32
.data:
dq 0xAAAAAAAABBBBBBBB
dq 0xCCCCCCCCDDDDDDDD
dq 0xEEEEEEEEFFFFFFFF
dq 0x9999999988888888

dq 0x1111111122222222
dq 0x3333333344444444
dq 0x5555555566666666
dq 0x7777777788888888
