ENTRY(_start)
MEMORY
{
    FLASH (rx) : ORIGIN = 0x42000000 , LENGTH = 4M
    RAM (rwx) : ORIGIN = 0x40800000 , LENGTH = 512K
}
SECTIONS
{
    .flash_header : {
        KEEP(*(.flash_header))
    }> FLASH
    .text :
    {
        KEEP(*(.entry))
        *(.text*)
        *(.text.*)
        KEEP(*(.init))
        KEEP(*(.fini))
        *crtbegin.o(.ctors)
        *crtbegin?.o(.ctors)
        *(EXCLUDE_FILE(*crtend?.o *crtend.o) .ctors)
        *(SORT(.ctors.*))
        *(.ctors)
        *crtbegin.o(.dtors)
        *crtbegin?.o(.dtors)
        *(EXCLUDE_FILE(*crtend?.o *crtend.o) .dtors)
        *(SORT(.dtors.*))
        *(.dtors)
        *(.rodata)
        *(.rodata.*)
       *(.gnu.linkonce.r.*)
       *(.rodata1)
       *(.sdata2 .sdata2.* .srodata .srodata.*)
       *(.gcc_except_table)
       *(.gnu.linkonce.e.*)
       *(.gnu.version_r)
       *(.eh_frame_hdr)
       *(.eh_frame)
        . = ALIGN(4);
        __text_end = .;
    } > FLASH
    .data : AT( __text_end)
    {
    __data_start = .;
    *(.data .data*)
    *(.data.*)
    *(.gnu.linkonce.d.*)
    *(.data1 .data1* .data1.*)
    *(.sdata .sdata*)
    *(.sdata.*)
    *(.dram1 .dram1.*)
    *(.gnu.linkonce.s.*)
    *(.gnu.linkonce.s2.*)
    *(.jcr)
    *(.iram1 .iram1.* .iram1*)
    . = ALIGN(256);
    *(.vector_table)
    . = ALIGN(4);
    __data_end = .;
    } > RAM
    .bss :
    {
       . = ALIGN(4);
    __bss_start = .;
    *(.dynsbss)
    *(.sbss)
    *(.sbss.* .sbss*)
    *(.sbss1)
    *(.sbss1.* .sbss1*)
    *(.gnu.linkonce.sb.*)
    *(.scommon)
    *(.sbss2)
    *(.sbss2.* .sbss2*)
    *(.gnu.linkonce.sb2.*)
    *(.dynbss)
    *(.bss)
    *(.bss.* .bss*)
    *(.bss1 .bss1.* .bss1*)
     *(.bss2 .bss2.* .bss2*)
    *(.gnu.linkonce.b.*)
    *(COMMON)
    . = ALIGN(4);
    __bss_end = .;
    } > RAM
    .heap (NOLOAD) :
    {
       . = ALIGN(16);
        __heap_start = .;
        end = . ;
        __end__ = . ;
        . += 0x10000;
        __heap_end = .;
    } > RAM
  .stack :
  {
    __stack_bottom = .;
    __stack_top = ORIGIN(RAM) + LENGTH(RAM);
    __stack_size_min = 0x4000;
    ASSERT(__stack_bottom + __stack_size_min < __stack_top, "Error: no space for stack");
  } > RAM
}
