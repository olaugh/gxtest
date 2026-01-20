/*
 * Genesis startup code for gxtest ROMs
 * Minimal crt0 - just calls main()
 */

    .section .header
    .ascii "SEGA MEGA DRIVE "      /* Console name (16 bytes) */
    .ascii "(C)GXTEST 2026  "      /* Copyright (16 bytes) */
    .ascii "PRIME SIEVE TEST ROM                            " /* Domestic name (48 bytes) */
    .ascii "PRIME SIEVE TEST ROM                            " /* Overseas name (48 bytes) */
    .ascii "GM 00000000-00"        /* Serial/version (14 bytes) */
    .word  0x0000                  /* Checksum placeholder */
    .ascii "J               "      /* I/O support (16 bytes) */
    .long  0x00000000              /* ROM start */
    .long  0x000003FF              /* ROM end (1KB, will be updated) */
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

    /* Call main */
    jsr     main

    /* Infinite loop after main returns */
.Lhang:
    bra.s   .Lhang
