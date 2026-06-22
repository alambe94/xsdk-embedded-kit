    .section .text.ram_entry, "ax", @progbits
    .global xSDK_RAM_Entry
    .type xSDK_RAM_Entry, @function
    .align 2
xSDK_RAM_Entry:
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop

    la sp, _eusrstack

    /* Clear BSS section */
    la a0, _sbss
    la a1, _ebss
    bgeu a0, a1, 2f
1:
    sw zero, 0(a0)
    addi a0, a0, 4
    bltu a0, a1, 1b
2:

    li t0, 0x1237B3E0
    csrw 0xBC0, t0

    li t0, 0x07
    csrw 0xBC1, t0

    li t0, 0x0F
    csrw 0x804, t0

    la t0, _vector_base
    ori t0, t0, 3
    csrw mtvec, t0

    li t0, 0x6088
    csrw mstatus, t0

    la t0, main
    csrw mepc, t0

    fence.i
    mret
    .size xSDK_RAM_Entry, . - xSDK_RAM_Entry
