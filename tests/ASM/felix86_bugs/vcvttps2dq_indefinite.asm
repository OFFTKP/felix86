%ifdef CONFIG
{
  "HostFeatures": ["AVX"],
  "RegData": {
    "XMM0": ["0x8000000080000000", "0x8000000080000000", "0x0000000000000000", "0x0000000000000000"],
    "XMM1": ["0x8000000080000000", "0x8000000080000000", "0x0000000000000000", "0x0000000000000000"],
    "XMM2": ["0x8000000080000000", "0x8000000080000000", "0x0000000000000000", "0x0000000000000000"],
    "XMM3": ["0x800000007FFFFF80", "0x800001007FFFFF00", "0x0000000000000000", "0x0000000000000000"],
    "XMM4": ["0x8000000080000000", "0x8000000080000000", "0x8000000080000000", "0x8000000080000000"],
    "XMM5": ["0x8000000080000000", "0x8000000080000000", "0x8000000080000000", "0x8000000080000000"],
    "XMM6": ["0x8000000080000000", "0x8000000080000000", "0x8000000080000000", "0x8000000080000000"],
    "XMM7": ["0x800000007FFFFF80", "0x8000000080000000", "0xFFFFFFFF00000001", "0x0000000000000000"]

  }
}
%endif
bits 64

vpcmpeqd ymm0,  ymm0,  ymm0
vpcmpeqd ymm1,  ymm1,  ymm1
vpcmpeqd ymm2,  ymm2,  ymm2
vpcmpeqd ymm3,  ymm3,  ymm3
vpcmpeqd ymm4,  ymm4,  ymm4
vpcmpeqd ymm5,  ymm5,  ymm5
vpcmpeqd ymm6,  ymm6,  ymm6
vpcmpeqd ymm7,  ymm7,  ymm7

; VCVTTPS2DQ, VEX.128: four floats -> four signed dwords.
lea rsi, [rel .ps_data]
vcvttps2dq xmm0, oword [rsi + 16 * 0] ; qNaNs and sNaNs, both signs
vcvttps2dq xmm1, oword [rsi + 16 * 1] ; +/-infinity and first finite overflow values
vcvttps2dq xmm2, oword [rsi + 16 * 2] ; +/-largest finite floats and larger overflows
vcvttps2dq xmm3, oword [rsi + 16 * 3] ; finite values just inside both limits

; VCVTTPS2DQ, VEX.256: eight floats -> eight signed dwords in YMM.
vcvttps2dq ymm4, yword [rsi + 64 + 32 * 0] ; qNaNs, both signs and payloads
vcvttps2dq ymm5, yword [rsi + 64 + 32 * 1] ; sNaNs and infinities
vcvttps2dq ymm6, yword [rsi + 64 + 32 * 2] ; infinities, extreme finite values, overflow
vcvttps2dq ymm7, yword [rsi + 64 + 32 * 3] ; valid boundary values mixed with invalids

hlt

align 32
.ps_data:
; VEX.128 source blocks: four binary32 values per instruction.
; qNaNs and sNaNs, both signs
 dd 0x7FC00001, 0xFFC00002, 0x7F800001, 0xFF800002
; +infinity, -infinity, +2^31, first float below -2^31
 dd 0x7F800000, 0xFF800000, 0x4F000000, 0xCF000001
; +/-largest finite float and larger positive/negative overflow values
 dd 0x7F7FFFFF, 0xFF7FFFFF, 0x4F000001, 0xCF000002
; largest floats below +2^31 and at/above -2^31, all valid
 dd 0x4EFFFFFF, 0xCF000000, 0x4EFFFFFE, 0xCEFFFFFE

; VEX.256 source blocks: eight binary32 values per instruction.
; qNaNs with varied signs and payloads
 dd 0x7FC00001, 0xFFC00002, 0x7FC12345, 0xFFC54321
 dd 0x7FFFFFFF, 0xFFFFFFFF, 0x7FC00000, 0xFFC00000
; sNaNs with varied signs and payloads, then +/-infinity
 dd 0x7F800001, 0xFF800002, 0x7F812345, 0xFF854321
 dd 0x7FBFFFFF, 0xFFBFFFFF, 0x7F800000, 0xFF800000
; +/-infinity, +/-largest finite, +2^31 and first negative overflow values
 dd 0x7F800000, 0xFF800000, 0x7F7FFFFF, 0xFF7FFFFF
 dd 0x4F000000, 0xCF000001, 0x4F000001, 0xCF000002
; Values just inside both boundaries, the overflow boundary, and ordinary values
 dd 0x4EFFFFFF, 0xCF000000, 0x4F000000, 0xCF000001
 dd 0x3FE00000, 0xBFE00000, 0x00000000, 0x80000000
