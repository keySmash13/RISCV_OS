    .section .text
    .global _start
    .align 2
_start:
    /* set up stack pointer */
    la sp, _stack_top

    /* call kernel entry in C */
    call kmain

    /* if kmain returns, spin */
1:  wfi
    j 1b

    /* space for stack */
    .section .bss
    .align 12
    .global _stack
_stack:
    .skip 8192
    .global _stack_top
_stack_top:

