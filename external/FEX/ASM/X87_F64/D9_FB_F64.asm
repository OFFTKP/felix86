%ifdef CONFIG
{
  "RegData": {
    "RAX":  "1"
  },
  "Env": { "FEX_X87REDUCEDPRECISION" : "1" }
}
%endif
bits 64

%include "checkprecision.mac"

mov rcx, 0xe0000000

lea rdx, [rel data]
fld tword [rdx + 8 * 0]

fsincos

; st0 = cos, st1 = sin
fstp qword [rcx]
check_relerr_d rel expected_cos, rcx, rel tolerance
mov r8, rax

fstp qword [rcx]
check_relerr_d rel expected_sin, rcx, rel tolerance
and rax, r8

hlt

align 8
data:
  dt 1.0
  dq 0
expected_cos:
  dq 0x3fe14a280fb5068c ; cos(1.0)
expected_sin:
  dq 0x3feaed548f090cee ; sin(1.0)
tolerance:
  dq 0x3cb0000000000000 ; 2^-52, ~1 ULP relative error

define_check_data_constants
