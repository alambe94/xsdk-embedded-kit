ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x00100000, LENGTH = 4M
}

SECTIONS
{
    .text :
    {
        *(.text.startup)
        *(.text*)
        *(.rodata*)
    } > RAM

    .data :
    {
        *(.data*)
    } > RAM

    .bss :
    {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        _bss_end = .;
        _end = .;
        end = .;
    } > RAM

    .stack (NOLOAD) :
    {
        . = ALIGN(8);
        _stack_bottom = .;
        . += 0x1000; /* 4KB SYS stack */
        _sys_stack_top = .;
        . += 0x1000; /* 4KB SVC stack */
        _svc_stack_top = .;
        _stack_top = .;
    } > RAM
}
