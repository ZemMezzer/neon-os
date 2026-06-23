OUTPUT_ARCH(aarch64)
ENTRY(_start)

SECTIONS
{
    . = 0x00080000;
    __kernel_start = .;

    . = ALIGN(4096);
    .text :
    {
        KEEP(*(.text.boot))
        KEEP(*(.text.boot.*))
        *(.text .text.*)
    }

    . = ALIGN(4096);
    .rodata :
    {
        *(.rodata .rodata.*)
    }

    . = ALIGN(4096);
    .data :
    {
        *(.data .data.*)
    }

    . = ALIGN(16);
    __bss_start = .;
    .bss (NOLOAD) :
    {
        *(.bss .bss.*)
        *(COMMON)
    }
    . = ALIGN(16);
    __bss_end = .;

    __kernel_end = .;

    __heap_start = .;
    __heap_end = 0x3C000000;

    ASSERT(__heap_start < __heap_end, "Kernel image is too large for heap")

    /DISCARD/ :
    {
        *(.comment)
        *(.note .note.*)
        *(.eh_frame .eh_frame.*)
    }
}
