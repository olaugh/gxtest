/* Genesis/Mega Drive Linker Script for gxtest ROMs */

OUTPUT_FORMAT("elf32-m68k")
OUTPUT_ARCH(m68k)
ENTRY(_start)

MEMORY
{
    rom (rx)  : ORIGIN = 0x000000, LENGTH = 4M
    ram (rwx) : ORIGIN = 0xFF0000, LENGTH = 64K
}

SECTIONS
{
    /* Vector table at start of ROM */
    .vectors 0x000000 :
    {
        LONG(0x00FFFE00)    /* Initial SSP (stack at end of RAM) */
        LONG(_start)         /* Initial PC (entry point) */

        /* Exception vectors - point to infinite loop */
        . = 0x100;
    } > rom

    /* ROM header at 0x100 */
    .header 0x000100 :
    {
        *(.header)
        . = 0x100;          /* Header is exactly 256 bytes */
    } > rom

    /* Code starts at 0x200 */
    .text 0x000200 :
    {
        *(.text*)
        *(.rodata*)
        . = ALIGN(2);
    } > rom

    /* Work RAM */
    .bss 0xFF0000 (NOLOAD) :
    {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } > ram

    .data __bss_end :
    {
        __data_start = .;
        *(.data*)
        __data_end = .;
    } > ram AT > rom

    /* Symbol for .data load address in ROM */
    __data_load = LOADADDR(.data);
}
