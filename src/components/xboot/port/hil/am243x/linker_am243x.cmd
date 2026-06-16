/*
 * AM243x xBOOT linker script — TI ARM Clang syntax.
 * Entry: _vectors_xboot
 * xBOOT runs entirely from MSRAM — vectors at 0x70000000, code/data follows.
 */

--stack_size=16384
--heap_size=32768
-e _vectors_xboot

__IRQ_STACK_SIZE       = 4096;
__FIQ_STACK_SIZE       = 256;
__SVC_STACK_SIZE       = 256;
__ABORT_STACK_SIZE     = 256;
__UNDEFINED_STACK_SIZE = 256;

MEMORY
{
    R5F_VECS   : ORIGIN = 0x00000000, LENGTH = 0x00000040
    R5F_TCMA   : ORIGIN = 0x00000040, LENGTH = 0x00007FC0
    R5F_TCMB0  : ORIGIN = 0x41010000, LENGTH = 0x00008000
    MSRAM_VECS : ORIGIN = 0x70000000, LENGTH = 0x00000100
    MSRAM_CODE : ORIGIN = 0x70000100, LENGTH = 0x0002BF00    /* 176 KB for code + data */
    MSRAM_DATA : ORIGIN = 0x7002C000, LENGTH = 0x00014000    /* 80 KB for BSS + stacks */
}

SECTIONS
{
    .vectors : {
    } > MSRAM_VECS, palign(8)

    GROUP : {
        .text.hwi  : { } palign(8)
        .text.cache: { } palign(8)
        .text.mpu  : { } palign(8)
        .text.boot : { } palign(8)
        .text      : { } palign(8)
        .rodata    : { } palign(8)
        .rodata.cfg: { } palign(8)
        .data      : { } palign(8)
        .ARM.exidx : { } palign(8)
        .init_array: { } palign(8)
        .fini_array: { } palign(8)
    } > MSRAM_CODE

    GROUP : {
        .bss : {
        } palign(8)
        RUN_START(__BSS_START)
        RUN_END(__BSS_END)
        .sysmem : { } palign(8)

        .irqstack : {
            . += __IRQ_STACK_SIZE;
        } align(8)
        RUN_START(__IRQ_STACK_START)
        RUN_END(__IRQ_STACK_END)

        .fiqstack : {
            . += __FIQ_STACK_SIZE;
        } align(8)
        RUN_START(__FIQ_STACK_START)
        RUN_END(__FIQ_STACK_END)

        .svcstack : {
            . += __SVC_STACK_SIZE;
        } align(8)
        RUN_START(__SVC_STACK_START)
        RUN_END(__SVC_STACK_END)

        .abortstack : {
            . += __ABORT_STACK_SIZE;
        } align(8)
        RUN_START(__ABORT_STACK_START)
        RUN_END(__ABORT_STACK_END)

        .undefinedstack : {
            . += __UNDEFINED_STACK_SIZE;
        } align(8)
        RUN_START(__UNDEFINED_STACK_START)
        RUN_END(__UNDEFINED_STACK_END)

        .stack : {
        } palign(8)
        RUN_START(__STACK_START)
        RUN_END(__STACK_END)
    } > MSRAM_DATA
}
