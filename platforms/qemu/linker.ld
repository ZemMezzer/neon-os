ENTRY(_start)

SECTIONS {
    . = 0x40080000;

    .text : {
        KEEP(*(.text.boot))
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    . = ALIGN(16);

    .bss (NOLOAD) : {
        *(.bss*)
        *(COMMON)
    }

    . = ALIGN(16);

    .stack (NOLOAD) : {
        KEEP(*(.stack))
    }

    . = ALIGN(16);

    kernel_end = .;

    .heap (NOLOAD) : {
        __heap_start = .;
        . += 0x30000000;
        __heap_end = .;
    }
}