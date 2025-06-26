; These aren't thunked with the traditional mechanism because we want vdso support regardless
; of if the user enabled thunking or not
bits 64
section .text

global __vdso_gettimeofday:function
global __vdso_time:function
global __vdso_clock_gettime:function
global __vdso_clock_getres:function
global __vdso_getcpu:function
global __vdso_getrandom:function
global gettimeofday:function
global time:function
global clock_gettime:function
global clock_getres:function
global getcpu:function
global getrandom:function

align 16
__vdso_gettimeofday:
gettimeofday:
mov rax, 96
syscall
ret

align 16
__vdso_time:
time:
mov rax, 201
syscall
ret

align 16
__vdso_clock_gettime:
clock_gettime:
mov rax, 228
syscall
ret

align 16
__vdso_clock_getres:
clock_getres:
mov rax, 229
syscall
ret

align 16
__vdso_getcpu:
getcpu:
mov rax, 309
syscall
ret

align 16
__vdso_getrandom:
getrandom:
mov rax, 318
syscall
ret
