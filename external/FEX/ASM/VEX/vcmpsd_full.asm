%ifdef CONFIG
{
  "HostFeatures": ["AVX"],
  "RegData": {
    "RCX": "224"
  }
}
%endif

%define true 1
%define false 0

%define EQ_OQ 0
%define LT_OS 1
%define LE_OS 2
%define UNORD_Q 3
%define NEQ_UQ 4
%define NLT_US 5
%define NLE_US 6
%define ORD_Q 7
%define EQ_UQ 8
%define NGE_US 9
%define NGT_US 10
%define FALSE_OQ 11
%define NEQ_OQ 12
%define GE_OS 13
%define GT_OS 14
%define TRUE_UQ 15
%define EQ_OS 16
%define LT_OQ 17
%define LE_OQ 18
%define UNORD_S 19
%define NEQ_US 20
%define NLT_UQ 21
%define NLE_UQ 22
%define ORD_S 23
%define EQ_US 24
%define NGE_UQ 25
%define NGT_UQ 26
%define FALSE_OS 27
%define NEQ_OS 28
%define GE_OQ 29
%define GT_OQ 30
%define TRUE_US 31

; Arguments: src1, src2, predicate, true/false
%macro Compare 4
    vcmpsd xmm7, %1, %2, %3
    ; Construct expected result
    ; Move truthy/falsey value to the first element based on %4
    ; and [127:64] from src1
    vmovdqa xmm8, [rel .false + 32 * %4]
    vmovsd xmm5, %1, xmm8
    ; Compare if result and expected are equal
    vxorpd ymm6, ymm7, ymm5
    vptest ymm6, ymm6
    jnz .fail
    ; Increment counter of tests passed
    add ecx, 1
%endmacro

lea rax, [rel .data]
xor ecx, ecx

vmovdqa ymm0, [rax + 32 * 0] ; -4.0
vmovdqa ymm1, [rax + 32 * 1] ; -0.0
vmovdqa ymm2, [rax + 32 * 2] ; 0.0
vmovdqa ymm3, [rax + 32 * 3] ; 4.0
vmovdqa ymm4, [rel .nan]     ; NaN

%define xmm_neg_four xmm0
%define xmm_neg_zero xmm1
%define xmm_zero xmm2
%define xmm_four xmm3
%define xmm_nan xmm4

Compare xmm_four, xmm_four, EQ_OQ, true
Compare xmm_neg_four, xmm_four, EQ_OQ, false
Compare xmm_zero, xmm_neg_zero, EQ_OQ, true
Compare xmm_zero, xmm_zero, EQ_OQ, true
Compare xmm_four, xmm_nan, EQ_OQ, false
Compare xmm_nan, xmm_four, EQ_OQ, false
Compare xmm_nan, xmm_nan, EQ_OQ, false

Compare xmm_four, xmm_neg_four, LT_OS, false
Compare xmm_neg_four, xmm_four, LT_OS, true
Compare xmm_zero, xmm_neg_zero, LT_OS, false
Compare xmm_neg_zero, xmm_zero, LT_OS, false
Compare xmm_nan, xmm_four, LT_OS, false
Compare xmm_four, xmm_nan, LT_OS, false
Compare xmm_nan, xmm_nan, LT_OS, false

Compare xmm_four, xmm_neg_four, LE_OS, false
Compare xmm_neg_four, xmm_four, LE_OS, true
Compare xmm_zero, xmm_neg_zero, LE_OS, true
Compare xmm_neg_zero, xmm_zero, LE_OS, true
Compare xmm_nan, xmm_four, LE_OS, false
Compare xmm_four, xmm_nan, LE_OS, false
Compare xmm_nan, xmm_nan, LE_OS, false

Compare xmm_four, xmm_neg_four, UNORD_Q, false
Compare xmm_neg_four, xmm_four, UNORD_Q, false
Compare xmm_zero, xmm_neg_zero, UNORD_Q, false
Compare xmm_neg_zero, xmm_zero, UNORD_Q, false
Compare xmm_nan, xmm_four, UNORD_Q, true
Compare xmm_four, xmm_nan, UNORD_Q, true
Compare xmm_nan, xmm_nan, UNORD_Q, true

Compare xmm_four, xmm_four, NEQ_UQ, false
Compare xmm_four, xmm_neg_four, NEQ_UQ, true
Compare xmm_zero, xmm_neg_zero, NEQ_UQ, false
Compare xmm_zero, xmm_zero, NEQ_UQ, false
Compare xmm_four, xmm_nan, NEQ_UQ, true
Compare xmm_nan, xmm_four, NEQ_UQ, true
Compare xmm_nan, xmm_nan, NEQ_UQ, true

Compare xmm_four, xmm_neg_four, NLT_US, true
Compare xmm_neg_four, xmm_four, NLT_US, false
Compare xmm_zero, xmm_neg_zero, NLT_US, true
Compare xmm_neg_zero, xmm_zero, NLT_US, true
Compare xmm_nan, xmm_four, NLT_US, true
Compare xmm_four, xmm_nan, NLT_US, true
Compare xmm_nan, xmm_nan, NLT_US, true

Compare xmm_four, xmm_neg_four, NLE_US, true
Compare xmm_neg_four, xmm_four, NLE_US, false
Compare xmm_zero, xmm_neg_zero, NLE_US, false
Compare xmm_neg_zero, xmm_zero, NLE_US, false
Compare xmm_nan, xmm_four, NLE_US, true
Compare xmm_four, xmm_nan, NLE_US, true
Compare xmm_nan, xmm_nan, NLE_US, true

Compare xmm_four, xmm_neg_four, ORD_Q, true
Compare xmm_neg_four, xmm_four, ORD_Q, true
Compare xmm_zero, xmm_neg_zero, ORD_Q, true
Compare xmm_neg_zero, xmm_zero, ORD_Q, true
Compare xmm_nan, xmm_four, ORD_Q, false
Compare xmm_four, xmm_nan, ORD_Q, false
Compare xmm_nan, xmm_nan, ORD_Q, false

Compare xmm_four, xmm_neg_four, EQ_UQ, false
Compare xmm_four, xmm_four, EQ_UQ, true
Compare xmm_zero, xmm_neg_zero, EQ_UQ, true
Compare xmm_zero, xmm_zero, EQ_UQ, true
Compare xmm_four, xmm_nan, EQ_UQ, true
Compare xmm_nan, xmm_four, EQ_UQ, true
Compare xmm_nan, xmm_nan, EQ_UQ, true

Compare xmm_four, xmm_neg_four, NGE_US, false
Compare xmm_neg_four, xmm_four, NGE_US, true
Compare xmm_zero, xmm_neg_zero, NGE_US, false
Compare xmm_neg_zero, xmm_zero, NGE_US, false
Compare xmm_nan, xmm_four, NGE_US, true
Compare xmm_four, xmm_nan, NGE_US, true
Compare xmm_nan, xmm_nan, NGE_US, true

Compare xmm_four, xmm_neg_four, NGT_US, false
Compare xmm_neg_four, xmm_four, NGT_US, true
Compare xmm_zero, xmm_neg_zero, NGT_US, true
Compare xmm_neg_zero, xmm_zero, NGT_US, true
Compare xmm_nan, xmm_four, NGT_US, true
Compare xmm_four, xmm_nan, NGT_US, true
Compare xmm_nan, xmm_nan, NGT_US, true

Compare xmm_four, xmm_neg_four, FALSE_OQ, false
Compare xmm_neg_four, xmm_four, FALSE_OQ, false
Compare xmm_zero, xmm_neg_zero, FALSE_OQ, false
Compare xmm_neg_zero, xmm_zero, FALSE_OQ, false
Compare xmm_nan, xmm_four, FALSE_OQ, false
Compare xmm_four, xmm_nan, FALSE_OQ, false
Compare xmm_nan, xmm_nan, FALSE_OQ, false

Compare xmm_four, xmm_neg_four, NEQ_OQ, true
Compare xmm_four, xmm_four, NEQ_OQ, false
Compare xmm_zero, xmm_neg_zero, NEQ_OQ, false
Compare xmm_zero, xmm_zero, NEQ_OQ, false
Compare xmm_four, xmm_nan, NEQ_OQ, false
Compare xmm_nan, xmm_four, NEQ_OQ, false
Compare xmm_nan, xmm_nan, NEQ_OQ, false

Compare xmm_four, xmm_neg_four, GE_OS, true
Compare xmm_neg_four, xmm_four, GE_OS, false
Compare xmm_zero, xmm_neg_zero, GE_OS, true
Compare xmm_neg_zero, xmm_zero, GE_OS, true
Compare xmm_nan, xmm_four, GE_OS, false
Compare xmm_four, xmm_nan, GE_OS, false
Compare xmm_nan, xmm_nan, GE_OS, false

Compare xmm_four, xmm_neg_four, GT_OS, true
Compare xmm_neg_four, xmm_four, GT_OS, false
Compare xmm_zero, xmm_neg_zero, GT_OS, false
Compare xmm_neg_zero, xmm_zero, GT_OS, false
Compare xmm_nan, xmm_four, GT_OS, false
Compare xmm_four, xmm_nan, GT_OS, false
Compare xmm_nan, xmm_nan, GT_OS, false

Compare xmm_four, xmm_neg_four, TRUE_UQ, true
Compare xmm_neg_four, xmm_four, TRUE_UQ, true
Compare xmm_zero, xmm_neg_zero, TRUE_UQ, true
Compare xmm_neg_zero, xmm_zero, TRUE_UQ, true
Compare xmm_nan, xmm_four, TRUE_UQ, true
Compare xmm_four, xmm_nan, TRUE_UQ, true
Compare xmm_nan, xmm_nan, TRUE_UQ, true

Compare xmm_four, xmm_neg_four, EQ_OS, false
Compare xmm_neg_four, xmm_four, EQ_OS, false
Compare xmm_zero, xmm_neg_zero, EQ_OS, true
Compare xmm_neg_zero, xmm_zero, EQ_OS, true
Compare xmm_nan, xmm_four, EQ_OS, false
Compare xmm_four, xmm_nan, EQ_OS, false
Compare xmm_nan, xmm_nan, EQ_OS, false

Compare xmm_four, xmm_neg_four, LT_OQ, false
Compare xmm_neg_four, xmm_four, LT_OQ, true
Compare xmm_zero, xmm_neg_zero, LT_OQ, false
Compare xmm_neg_zero, xmm_zero, LT_OQ, false
Compare xmm_nan, xmm_four, LT_OQ, false
Compare xmm_four, xmm_nan, LT_OQ, false
Compare xmm_nan, xmm_nan, LT_OQ, false

Compare xmm_four, xmm_neg_four, LE_OQ, false
Compare xmm_neg_four, xmm_four, LE_OQ, true
Compare xmm_zero, xmm_neg_zero, LE_OQ, true
Compare xmm_neg_zero, xmm_zero, LE_OQ, true
Compare xmm_nan, xmm_four, LE_OQ, false
Compare xmm_four, xmm_nan, LE_OQ, false
Compare xmm_nan, xmm_nan, LE_OQ, false

Compare xmm_four, xmm_neg_four, UNORD_S, false
Compare xmm_neg_four, xmm_four, UNORD_S, false
Compare xmm_zero, xmm_neg_zero, UNORD_S, false
Compare xmm_neg_zero, xmm_zero, UNORD_S, false
Compare xmm_nan, xmm_four, UNORD_S, true
Compare xmm_four, xmm_nan, UNORD_S, true
Compare xmm_nan, xmm_nan, UNORD_S, true

Compare xmm_four, xmm_neg_four, NEQ_US, true
Compare xmm_neg_four, xmm_four, NEQ_US, true
Compare xmm_zero, xmm_neg_zero, NEQ_US, false
Compare xmm_neg_zero, xmm_zero, NEQ_US, false
Compare xmm_nan, xmm_four, NEQ_US, true
Compare xmm_four, xmm_nan, NEQ_US, true
Compare xmm_nan, xmm_nan, NEQ_US, true

Compare xmm_four, xmm_neg_four, NLT_UQ, true
Compare xmm_neg_four, xmm_four, NLT_UQ, false
Compare xmm_zero, xmm_neg_zero, NLT_UQ, true
Compare xmm_neg_zero, xmm_zero, NLT_UQ, true
Compare xmm_nan, xmm_four, NLT_UQ, true
Compare xmm_four, xmm_nan, NLT_UQ, true
Compare xmm_nan, xmm_nan, NLT_UQ, true

Compare xmm_four, xmm_neg_four, NLE_UQ, true
Compare xmm_neg_four, xmm_four, NLE_UQ, false
Compare xmm_zero, xmm_neg_zero, NLE_UQ, false
Compare xmm_neg_zero, xmm_zero, NLE_UQ, false
Compare xmm_nan, xmm_four, NLE_UQ, true
Compare xmm_four, xmm_nan, NLE_UQ, true
Compare xmm_nan, xmm_nan, NLE_UQ, true

Compare xmm_four, xmm_neg_four, ORD_S, true
Compare xmm_neg_four, xmm_four, ORD_S, true
Compare xmm_zero, xmm_neg_zero, ORD_S, true
Compare xmm_neg_zero, xmm_zero, ORD_S, true
Compare xmm_nan, xmm_four, ORD_S, false
Compare xmm_four, xmm_nan, ORD_S, false
Compare xmm_nan, xmm_nan, ORD_S, false

Compare xmm_four, xmm_neg_four, EQ_US, false
Compare xmm_four, xmm_four, EQ_US, true
Compare xmm_zero, xmm_neg_zero, EQ_US, true
Compare xmm_zero, xmm_zero, EQ_US, true
Compare xmm_four, xmm_nan, EQ_US, true
Compare xmm_nan, xmm_four, EQ_US, true
Compare xmm_nan, xmm_nan, EQ_US, true

Compare xmm_four, xmm_neg_four, NGE_UQ, false
Compare xmm_neg_four, xmm_four, NGE_UQ, true
Compare xmm_zero, xmm_neg_zero, NGE_UQ, false
Compare xmm_neg_zero, xmm_zero, NGE_UQ, false
Compare xmm_nan, xmm_four, NGE_UQ, true
Compare xmm_four, xmm_nan, NGE_UQ, true
Compare xmm_nan, xmm_nan, NGE_UQ, true

Compare xmm_four, xmm_neg_four, NGT_UQ, false
Compare xmm_neg_four, xmm_four, NGT_UQ, true
Compare xmm_zero, xmm_neg_zero, NGT_UQ, true
Compare xmm_neg_zero, xmm_zero, NGT_UQ, true
Compare xmm_nan, xmm_four, NGT_UQ, true
Compare xmm_four, xmm_nan, NGT_UQ, true
Compare xmm_nan, xmm_nan, NGT_UQ, true

Compare xmm_four, xmm_neg_four, FALSE_OS, false
Compare xmm_neg_four, xmm_four, FALSE_OS, false
Compare xmm_zero, xmm_neg_zero, FALSE_OS, false
Compare xmm_neg_zero, xmm_zero, FALSE_OS, false
Compare xmm_nan, xmm_four, FALSE_OS, false
Compare xmm_four, xmm_nan, FALSE_OS, false
Compare xmm_nan, xmm_nan, FALSE_OS, false

Compare xmm_four, xmm_neg_four, NEQ_OS, true
Compare xmm_four, xmm_four, NEQ_OS, false
Compare xmm_zero, xmm_neg_zero, NEQ_OS, false
Compare xmm_zero, xmm_zero, NEQ_OS, false
Compare xmm_four, xmm_nan, NEQ_OS, false
Compare xmm_nan, xmm_four, NEQ_OS, false
Compare xmm_nan, xmm_nan, NEQ_OS, false

Compare xmm_four, xmm_neg_four, GE_OQ, true
Compare xmm_neg_four, xmm_four, GE_OQ, false
Compare xmm_zero, xmm_neg_zero, GE_OQ, true
Compare xmm_neg_zero, xmm_zero, GE_OQ, true
Compare xmm_nan, xmm_four, GE_OQ, false
Compare xmm_four, xmm_nan, GE_OQ, false
Compare xmm_nan, xmm_nan, GE_OQ, false

Compare xmm_four, xmm_neg_four, GT_OQ, true
Compare xmm_neg_four, xmm_four, GT_OQ, false
Compare xmm_zero, xmm_neg_zero, GT_OQ, false
Compare xmm_neg_zero, xmm_zero, GT_OQ, false
Compare xmm_nan, xmm_four, GT_OQ, false
Compare xmm_four, xmm_nan, GT_OQ, false
Compare xmm_nan, xmm_nan, GT_OQ, false

Compare xmm_four, xmm_neg_four, TRUE_US, true
Compare xmm_neg_four, xmm_four, TRUE_US, true
Compare xmm_zero, xmm_neg_zero, TRUE_US, true
Compare xmm_neg_zero, xmm_zero, TRUE_US, true
Compare xmm_nan, xmm_four, TRUE_US, true
Compare xmm_four, xmm_nan, TRUE_US, true
Compare xmm_nan, xmm_nan, TRUE_US, true

; If ecx is not correct we can use the counter to determine which test failed
.fail:
hlt

align 32
.data:
dq -4.0
dq 0xCCCCCCCCCCCCCCCC
dq 0xCCCCCCCCCCCCCCCC
dq 0xCCCCCCCCCCCCCCCC

dq -0.0
dq 0xCCCCCCCCCCCCCCCC
dq 0xCCCCCCCCCCCCCCCC
dq 0xCCCCCCCCCCCCCCCC

dq 0.0
dq 0xCCCCCCCCCCCCCCCC
dq 0xCCCCCCCCCCCCCCCC
dq 0xCCCCCCCCCCCCCCCC

dq 4.0
dq 0xCCCCCCCCCCCCCCCC
dq 0xCCCCCCCCCCCCCCCC
dq 0xCCCCCCCCCCCCCCCC

.nan:
dq 0x7FF8000000000000
dq 0xCCCCCCCCCCCCCCCC
dq 0xCCCCCCCCCCCCCCCC
dq 0xCCCCCCCCCCCCCCCC

.false:
dq 0x0000000000000000
dq 0x0000000000000000
dq 0x0000000000000000
dq 0x0000000000000000

.true:
dq 0xFFFFFFFFFFFFFFFF
dq 0x0000000000000000
dq 0x0000000000000000
dq 0x0000000000000000