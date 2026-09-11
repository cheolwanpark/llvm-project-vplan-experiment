OUTPUT_ARCH("riscv")
ENTRY(_start)

SECTIONS {
    . = 0x80000000;
    .text.init : { KEEP(*(.text.init)) }
    .tohost ALIGN(0x1000) : { *(.tohost) }
    .text ALIGN(0x1000) : { *(.text .text.*) }
    .rodata ALIGN(0x1000) : { *(.rodata .rodata.*) *(.srodata .srodata.*) }
    .preinit_array ALIGN(16) : {
        __preinit_array_start = .; KEEP(*(.preinit_array .preinit_array.*)); __preinit_array_end = .;
    }
    .init_array ALIGN(16) : {
        __init_array_start = .; KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) .init_array)); __init_array_end = .;
    }
    .fini_array ALIGN(16) : {
        __fini_array_start = .; KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*) .fini_array)); __fini_array_end = .;
    }
    .data.rel.ro ALIGN(16) : { *(.data.rel.ro .data.rel.ro.*) }
    .got ALIGN(16) : { *(.got .got.*) }
    PROVIDE(__global_pointer$ = . + 0x800);
    .data ALIGN(0x1000) : { *(.data .data.*) *(.sdata .sdata.*) }
    .bss ALIGN(0x1000) (NOLOAD) : {
        __bss_start = .; *(.bss .bss.*) *(.sbss .sbss.*) *(COMMON); __bss_end = .;
    }
    .tdata ALIGN(16) : { __tls_base = .; *(.tdata .tdata.*) }
    .tbss ALIGN(16) (NOLOAD) : { __tbss_start = .; *(.tbss .tbss.*) *(.tcommon); __tbss_end = .; }
    . = ALIGN(16);
    __heap_start = .;
    __heap_end = 0x87f00000;
    __stack_top = 0x88000000;
    _end = .;
    PROVIDE(end = .);
}
