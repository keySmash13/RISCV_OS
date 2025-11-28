/* Linker script: place kernel at 0x80000000 (typical for simple RV64 examples) */
OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS
{
  . = 0x80200000;

  .text : {
    *(.text.boot)
    *(.text)
  }

  .rodata : { *(.rodata) }

  .data : { *(.data) }

  .bss (NOLOAD) : {
    __bss_start = .;
    *(.bss)
    *(COMMON)
    __bss_end = .;
  }

  /* small stack space directly after BSS (boot.S declares symbols) */
  /DISCARD/ : { *(.eh_frame) }
}
