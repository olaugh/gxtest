/*
 * Genesis startup code for gxtest ROMs
 * Minimal crt0 - initializes BSS, copies .data from ROM, and calls main()
 */

    .section .header
    .ascii "SEGA MEGA DRIVE "      /* Console name (16 bytes) */
    .ascii "(C)GXTEST 2026  "      /* Copyright (16 bytes) */
    .ascii "SYMBOL EXAMPLE ROM                              " /* Domestic name (48 bytes) */
    .ascii "SYMBOL EXAMPLE ROM                              " /* Overseas name (48 bytes) */
    .ascii "GM 00000001-00"        /* Serial/version (14 bytes) */
    .word  0x0000                  /* Checksum placeholder */
    .ascii "J               "      /* I/O support (16 bytes) */
    .long  0x00000000              /* ROM start */
    .long  0x000007FF              /* ROM end (rounded up to 2KB) */
    .long  0x00FF0000              /* RAM start */
    .long  0x00FFFFFF              /* RAM end */
    .ascii "            "          /* SRAM info (12 bytes) */
    .ascii "            "          /* Notes (12 bytes) */
    .ascii "JUE             "      /* Region + padding to 0x1FF */

    .section .text
    .global _start
    .type _start, @function

_start:
    /* Initialize stack pointer */
    lea     0x00FFFE00, %sp

    /* Clear BSS section */
    lea     __bss_start, %a0
    lea     __bss_end, %a1
    cmp.l   %a0, %a1
    beq.s   .Lbss_done
.Lbss_clear:
    clr.b   (%a0)+
    cmp.l   %a0, %a1
    bne.s   .Lbss_clear
.Lbss_done:

    /* Copy .data section from ROM (LMA) to RAM (VMA) */
    lea     __data_load, %a0     /* Source: ROM load address */
    lea     __data_start, %a1    /* Destination: RAM start of .data */
    lea     __data_end, %a2      /* End of .data in RAM */
    cmp.l   %a1, %a2
    beq.s   .Ldata_done
.Ldata_copy:
    move.b  (%a0)+, (%a1)+
    cmp.l   %a1, %a2
    bne.s   .Ldata_copy
.Ldata_done:

    /* Call main */
    jsr     main

    /* Infinite loop after main returns */
.Lhang:
    bra.s   .Lhang
