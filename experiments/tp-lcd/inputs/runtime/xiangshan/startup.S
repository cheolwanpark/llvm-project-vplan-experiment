    .section .text.init
    .globl _start
    .type _start, @function
_start:
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop
    csrw mie, zero
    csrw mip, zero
    la t0, 1f
    csrw mtvec, t0
    li t0, 0x00006600
    csrs mstatus, t0
    csrwi fcsr, 0
    csrwi vcsr, 0
    la sp, __stack_top
    la tp, __tls_base
    call __builder_start
1:  j 1b
