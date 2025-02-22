%ifdef CONFIG
{
  "HostFeatures": ["AVX"],
  "RegData": {
    "XMM0":  ["0x0000000000000000", "0x0000000000000000", "0x0000000000000000", "0x0000000000000000"],
    "XMM1":  ["0xFFFFFFFFFFFFFFFF", "0xFFFFFFFFFFFFFFFF", "0xFFFFFFFFFFFFFFFF", "0xFFFFFFFFFFFFFFFF"],
    "XMM2":  ["0x0000000000000001", "0x0000000000000001", "0x0000000000000001", "0x0000000000000001"],
    "XMM3":  ["0xFFFFFFFFFFFFFFFF", "0x0000000000000001", "0xFFFFFFFFFFFFFFFF", "0x0000000000000001"],
    "XMM4":  ["0x0000000000000001", "0xFFFFFFFFFFFFFFFF", "0x0000000000000001", "0xFFFFFFFFFFFFFFFF"],
    "XMM5":  ["0x0000000000000000", "0xFFFFFFFFFFFFFFFF", "0x0000000000000000", "0xFFFFFFFFFFFFFFFF"],
    "XMM6":  ["0x0000000000000000", "0x0000000000000000", "0x0000000000000000", "0x0000000000000000"],
    "XMM7":  ["0x0000000000000000", "0x0000000000000000", "0x0000000000000000", "0x0000000000000000"],
    "XMM8":  ["0x0000000000000000", "0xFFFFFFFFFFFFFFFF", "0x0000000000000000", "0x0000000000000000"],
    "XMM9":  ["0xFFFFFFFFFFFFFFFF", "0x0000000000000000", "0x0000000000000000", "0x0000000000000000"],
    "XMM10": ["0xFFFFFFFFFFFFFFFF", "0xFFFFFFFFFFFFFFFF", "0xFFFFFFFFFFFFFFFF", "0xFFFFFFFFFFFFFFFF"],
    "XMM11": ["0x0000000000000000", "0x0000000000000000", "0x0000000000000000", "0x0000000000000000"],
    "XMM12": ["0xFFFFFFFFFFFFFFFF", "0x0000000000000000", "0xFFFFFFFFFFFFFFFF", "0x0000000000000000"],
    "XMM13": ["0x0000000000000000", "0xFFFFFFFFFFFFFFFF", "0x0000000000000000", "0x0000000000000000"],
    "XMM14": ["0xFFFFFFFFFFFFFFFF", "0x0000000000000000", "0x0000000000000000", "0x0000000000000000"]
  }
}
%endif
bits 64

vmovaps ymm0, [rel .data0]
vmovaps ymm1, [rel .data1]
vmovaps ymm2, [rel .data2]
vmovaps ymm3, [rel .data3]
vmovaps ymm4, [rel .data4]

; Register only
vpcmpgtq ymm5, ymm0, [rel .data4]
vpcmpgtq ymm6, ymm1, [rel .data3]
vpcmpgtq ymm7, ymm2, [rel .data2]
vpcmpgtq xmm8, xmm3, [rel .data1]
vpcmpgtq xmm9, xmm4, [rel .data0]

; Memory operand
vpcmpgtq ymm10, ymm0, [rel .data1]
vpcmpgtq ymm11, ymm1, [rel .data2]
vpcmpgtq ymm12, ymm2, [rel .data3]
vpcmpgtq xmm13, xmm3, [rel .data4]
vpcmpgtq xmm14, xmm4, [rel .data0]

hlt

align 32
.data0:
dq 0
dq 0
dq 0
dq 0

.data1:
dq -1
dq -1
dq -1
dq -1

.data2:
dq 1
dq 1
dq 1
dq 1

.data3:
dq -1
dq 1
dq -1
dq 1

.data4:
dq 1
dq -1
dq 1
dq -1
