ENTRY(_start)

STACK_SIZE = 2048;

MEMORY
{
    FLASH    (rx)  : ORIGIN = 0x00010000, LENGTH = 128K
    RAM_CODE (xrw) : ORIGIN = 0x200A0000, LENGTH = 128K
    RAM_LOAD (xrw) : ORIGIN = 0x200C0200, LENGTH = 256
    RAM      (xrw) : ORIGIN = 0x200C0300, LENGTH = 256K - 768
}

SECTIONS
{
    .init :
    {
        _sinit = .;
        KEEP(*(SORT_NONE(.init)))
        . = ALIGN(4);
        _einit = .;
    } > FLASH

    .loadcode :
    {
        . = ALIGN(4);
        _loadcode_vma_start = .;
        *(.load)
        . = ALIGN(4);
        _loadcode_vma_end = .;
    } > RAM_LOAD AT > FLASH
    _loadcode_lma = LOADADDR(.loadcode);

    .highcode :
    {
        . = ALIGN(4);
        _highcode_vma_start = .;
        *(.vector)
        KEEP(*(SORT_NONE(.vector_handler)))
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
        *(.sdata2.*)
        *(.glue_7)
        *(.glue_7t)
        *(.gnu.linkonce.t.*)
        . = ALIGN(4);
        _highcode_vma_end = .;
    } > RAM_CODE AT > FLASH
    _highcode_lma = LOADADDR(.highcode);

    .handle_reset :
    {
        . = ALIGN(4);
        KEEP(*(SORT_NONE(.handle_reset)))
        . = ALIGN(4);
    } > FLASH

    .highcode_tail :
    {
        . = ALIGN(4);
        _highcode_vma_start1 = .;
        KEEP(*(SORT_NONE(.fini)))
        KEEP(*(.preinit_array))
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array))
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
        KEEP(*(.fini_array))
        KEEP(*(.ctors))
        KEEP(*(.dtors))
        . = ALIGN(4);
        _highcode_vma_end1 = .;
    } > RAM_CODE AT > FLASH
    _highcode_lma1 = LOADADDR(.highcode_tail);

    _etext = .;
    _eitcm = .;

    .data :
    {
        . = ALIGN(4);
        _data_vma = .;
        *(.gnu.linkonce.r.*)
        *(.data)
        *(.data.*)
        *(.gnu.linkonce.d.*)
        . = ALIGN(8);
        __global_pointer$ = . + 0x800;
        *(.sdata)
        *(.sdata.*)
        *(.gnu.linkonce.s.*)
        *(.srodata.cst16)
        *(.srodata.cst8)
        *(.srodata.cst4)
        *(.srodata.cst2)
        *(.srodata)
        *(.srodata.*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH
    _data_lma = LOADADDR(.data);

    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.sbss*)
        *(.gnu.linkonce.sb.*)
        *(.bss*)
        *(.gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    _end = _ebss;
    end = .;

    .stack ORIGIN(RAM) + LENGTH(RAM) - STACK_SIZE (NOLOAD) :
    {
        _heap_end = .;
        . = ALIGN(16);
        _susrstack = .;
        . += STACK_SIZE;
        _eusrstack = .;
    } > RAM

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
    }
}

ASSERT((_vector_base & 0xFF) == 0, "CH32H417 vector table must be 256-byte aligned")
ASSERT(_ebss <= _susrstack, "CH32H417 data and BSS overlap the stack")
