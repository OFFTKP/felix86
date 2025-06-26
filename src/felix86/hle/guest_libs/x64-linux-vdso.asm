; These aren't thunked with the traditional mechanism because we want vdso support regardless
; of if the user enabled thunking or not
bits 64
section .text

global __vdso_gettimeofday:function
align 16
__vdso_gettimeofday:
mov rax, 96
syscall
ret

global __vdso_time:function
align 16
__vdso_time:
mov rax, 201
syscall
ret

global __vdso_clock_gettime:function
align 16
__vdso_clock_gettime:
mov rax, 228
syscall
ret

global __vdso_clock_getres:function
align 16
__vdso_clock_getres:
mov rax, 229
syscall
ret

global __vdso_getcpu:function
align 16
__vdso_getcpu:
mov rax, 309
syscall
ret

global __vdso_getrandom:function
align 16
__vdso_getrandom:
mov rax, 318
syscall
ret
