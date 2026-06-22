ENTRY(xSDK_RAM_Entry)

STACK_SIZE = 2048;

MEMORY
{
    RAM_CODE (xrw) : ORIGIN = 0x200A0000, LENGTH = 128K
    RAM      (xrw) : ORIGIN = 0x200C0300, LENGTH = 256K - 768
}

SECTIONS
{
    .text :
    {
        . = ALIGN(256);
        _vector_base = .;
        KEEP(*(.vector))
        KEEP(*(SORT_NONE(.vector_handler)))
        KEEP(*(.init))
        KEEP(*(.load))
        KEEP(*(.handle_reset))
        KEEP(*(.text.ram_entry))
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
        *(.sdata2.*)
        *(.glue_7)
        *(.glue_7t)
        *(.gnu.linkonce.t.*)
        KEEP(*(SORT_NONE(.fini)))
        KEEP(*(.preinit_array))
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array))
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
        KEEP(*(.fini_array))
        KEEP(*(.ctors))
        KEEP(*(.dtors))
        . = ALIGN(4);
        _etext = .;
        _eitcm = .;
    } > RAM_CODE
    _loadcode_lma = ADDR(.text);
    _loadcode_vma_start = ADDR(.text);
    _loadcode_vma_end = ADDR(.text);
    _highcode_lma = ADDR(.text);
    _highcode_vma_start = ADDR(.text);
    _highcode_vma_end = ADDR(.text);
    _highcode_lma1 = ADDR(.text);
    _highcode_vma_start1 = ADDR(.text);
    _highcode_vma_end1 = ADDR(.text);

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
    } > RAM
    _data_lma = ADDR(.data);

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

ASSERT((_vector_base & 0xFF) == 0, "CH32H417 RAM vector table must be 256-byte aligned")
ASSERT(_ebss <= _susrstack, "CH32H417 RAM image data and BSS overlap the stack")
