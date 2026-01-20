#!/usr/bin/env python3
"""
Generate a Sega Genesis ROM that computes the first 100 primes using
the Sieve of Eratosthenes and writes them to work RAM.

Memory layout in work RAM ($FF0000):
  $FF0000 - $FF0257: Sieve array (600 bytes)
  $FF0300 - $FF03C7: Prime results (100 x 2 bytes = 200 bytes)
  $FF0500: Prime count (word)
  $FF0502: Done flag ($DEAD when complete)
"""

import struct
import sys

def assemble_68k(instructions):
    """Assemble 68000 instructions into machine code."""
    code = bytearray()
    labels = {}

    # First pass: collect labels
    pc = 0
    for inst in instructions:
        if isinstance(inst, str) and inst.endswith(':'):
            labels[inst[:-1]] = pc
        elif isinstance(inst, (bytes, bytearray)):
            pc += len(inst)
        elif isinstance(inst, tuple):
            pc += inst[1]  # (opcode, size)

    # Second pass: generate code
    pc = 0
    for inst in instructions:
        if isinstance(inst, str) and inst.endswith(':'):
            continue
        elif isinstance(inst, (bytes, bytearray)):
            code.extend(inst)
            pc += len(inst)
        elif isinstance(inst, tuple):
            opcode, size, *args = inst
            if callable(opcode):
                # Deferred assembly (for forward references)
                result = opcode(pc, labels, *args)
                code.extend(result)
            else:
                code.extend(opcode)
            pc += size

    return bytes(code)

def word(val):
    """Pack a 16-bit word (big-endian)."""
    return struct.pack('>H', val & 0xFFFF)

def long(val):
    """Pack a 32-bit long (big-endian)."""
    return struct.pack('>I', val & 0xFFFFFFFF)

def branch_displacement(pc, labels, target):
    """Calculate branch displacement."""
    disp = labels[target] - (pc + 2)
    return word(disp)

def generate_rom():
    """Generate the prime sieve ROM."""

    # 68000 opcodes (big-endian)
    LEA_FF0000_A7 = bytes([0x4F, 0xF9, 0x00, 0xFF, 0x00, 0x00])  # lea $FF0000,a7
    LEA_FF0000_A0 = bytes([0x41, 0xF9, 0x00, 0xFF, 0x00, 0x00])  # lea $FF0000,a0
    LEA_FF0300_A1 = bytes([0x43, 0xF9, 0x00, 0xFF, 0x03, 0x00])  # lea $FF0300,a1

    MOVE_W_IMM_D0 = lambda val: bytes([0x30, 0x3C]) + word(val)  # move.w #imm,d0
    MOVE_W_IMM_D1 = lambda val: bytes([0x32, 0x3C]) + word(val)  # move.w #imm,d1

    CLR_B_A0_INC = bytes([0x42, 0x18])   # clr.b (a0)+
    CLR_W_D1 = bytes([0x42, 0x41])       # clr.w d1

    MOVE_B_IMM_A0 = lambda val: bytes([0x10, 0xBC, 0x00, val])  # move.b #imm,(a0)
    MOVE_B_IMM_A0_OFF = lambda val, off: bytes([0x11, 0x7C, 0x00, val]) + word(off)  # move.b #imm,off(a0)

    TST_B_A0_D0 = bytes([0x4A, 0x30, 0x00, 0x00])  # tst.b (a0,d0.w)
    MOVE_B_1_A0_D1 = bytes([0x11, 0xBC, 0x00, 0x01, 0x10, 0x00])  # move.b #1,(a0,d1.w)

    CMP_W_IMM_D0 = lambda val: bytes([0xB0, 0x7C]) + word(val)  # cmp.w #imm,d0
    CMP_W_IMM_D1 = lambda val: bytes([0xB2, 0x7C]) + word(val)  # cmp.w #imm,d1

    BNE_8 = lambda disp: bytes([0x66, disp & 0xFF])  # bne.s disp
    BGT_8 = lambda disp: bytes([0x6E, disp & 0xFF])  # bgt.s disp
    BGE_8 = lambda disp: bytes([0x6C, disp & 0xFF])  # bge.s disp
    BEQ_8 = lambda disp: bytes([0x67, disp & 0xFF])  # beq.s disp
    BRA_8 = lambda disp: bytes([0x60, disp & 0xFF])  # bra.s disp
    BRA_16 = lambda disp: bytes([0x60, 0x00]) + word(disp)  # bra.w disp

    ADDQ_W_1_D0 = bytes([0x52, 0x40])    # addq.w #1,d0
    ADDQ_W_1_D1 = bytes([0x52, 0x41])    # addq.w #1,d1
    ADD_W_D0_D1 = bytes([0xD2, 0x40])    # add.w d0,d1

    DBRA_D0 = lambda disp: bytes([0x51, 0xC8]) + word(disp)  # dbra d0,disp

    MOVE_W_D0_A1_INC = bytes([0x32, 0xC0])  # move.w d0,(a1)+
    # move.w d1,(xxx).l: 0011 001 111 000 001 = 0x33C1
    MOVE_W_D1_ABS = lambda addr: bytes([0x33, 0xC1]) + long(addr)
    # move.w #imm,(xxx).l: 0011 001 111 111 100 = 0x33FC
    MOVE_W_IMM_ABS = lambda val, addr: bytes([0x33, 0xFC]) + word(val) + long(addr)

    NOP = bytes([0x4E, 0x71])

    # Build the program
    # Start address will be $000200 (after header)

    program = bytearray()

    # Initialize stack pointer
    program.extend(LEA_FF0000_A7)  # lea $FF0000,a7 (stack at top of RAM)

    # Clear sieve array: 600 bytes at $FF0000
    program.extend(LEA_FF0000_A0)  # lea $FF0000,a0
    program.extend(MOVE_W_IMM_D0(599))  # move.w #599,d0
    # clear_loop:
    clear_loop = len(program)
    program.extend(CLR_B_A0_INC)   # clr.b (a0)+
    program.extend(DBRA_D0(-4))    # dbra d0,clear_loop

    # Mark 0 and 1 as composite
    program.extend(LEA_FF0000_A0)  # lea $FF0000,a0
    program.extend(MOVE_B_IMM_A0(1))  # move.b #1,(a0) - 0 is not prime
    program.extend(MOVE_B_IMM_A0_OFF(1, 1))  # move.b #1,1(a0) - 1 is not prime

    # Sieve main loop
    program.extend(MOVE_W_IMM_D0(2))  # move.w #2,d0 - start with 2

    # sieve_loop:
    sieve_loop = len(program)
    program.extend(CMP_W_IMM_D0(25))  # cmp.w #25,d0 (sqrt(600) ≈ 24)
    program.extend(BGT_8(0))  # bgt.s collect (placeholder)
    bgt_collect = len(program) - 1

    # Check if d0 is composite
    program.extend(LEA_FF0000_A0)  # lea $FF0000,a0
    program.extend(TST_B_A0_D0)    # tst.b (a0,d0.w)
    program.extend(BNE_8(0))       # bne.s next (placeholder)
    bne_next1 = len(program) - 1

    # Mark multiples: d1 = 2*d0
    program.extend(bytes([0x32, 0x00]))  # move.w d0,d1
    program.extend(ADD_W_D0_D1)    # add.w d0,d1

    # mark_multiples:
    mark_multiples = len(program)
    program.extend(CMP_W_IMM_D1(600))  # cmp.w #600,d1
    program.extend(BGE_8(0))       # bge.s next (placeholder)
    bge_next = len(program) - 1
    program.extend(MOVE_B_1_A0_D1) # move.b #1,(a0,d1.w)
    program.extend(ADD_W_D0_D1)    # add.w d0,d1
    disp = mark_multiples - len(program) - 2
    program.extend(BRA_8(disp & 0xFF))  # bra.s mark_multiples

    # next:
    next_addr = len(program)
    program[bne_next1] = (next_addr - bne_next1 - 1) & 0xFF
    program[bge_next] = (next_addr - bge_next - 1) & 0xFF
    program.extend(ADDQ_W_1_D0)    # addq.w #1,d0
    disp = sieve_loop - len(program) - 2
    program.extend(BRA_8(disp & 0xFF))  # bra.s sieve_loop

    # collect:
    collect = len(program)
    program[bgt_collect] = (collect - bgt_collect - 1) & 0xFF

    # Collect primes
    program.extend(LEA_FF0000_A0)  # lea $FF0000,a0 (sieve)
    program.extend(LEA_FF0300_A1)  # lea $FF0300,a1 (results)
    program.extend(CLR_W_D1)       # clr.w d1 (count)
    program.extend(MOVE_W_IMM_D0(2))  # move.w #2,d0 (start at 2)

    # collect_loop:
    collect_loop = len(program)
    program.extend(CMP_W_IMM_D1(100))  # cmp.w #100,d1
    program.extend(BEQ_8(0))       # beq.s done (placeholder)
    beq_done = len(program) - 1
    program.extend(CMP_W_IMM_D0(600))  # cmp.w #600,d0
    program.extend(BGE_8(0))       # bge.s done (placeholder)
    bge_done = len(program) - 1

    program.extend(TST_B_A0_D0)    # tst.b (a0,d0.w)
    program.extend(BNE_8(0))       # bne.s not_prime (placeholder)
    bne_not_prime = len(program) - 1

    # Found a prime
    program.extend(MOVE_W_D0_A1_INC)  # move.w d0,(a1)+
    program.extend(ADDQ_W_1_D1)    # addq.w #1,d1

    # not_prime:
    not_prime = len(program)
    program[bne_not_prime] = (not_prime - bne_not_prime - 1) & 0xFF
    program.extend(ADDQ_W_1_D0)    # addq.w #1,d0
    disp = collect_loop - len(program) - 2
    program.extend(BRA_8(disp & 0xFF))  # bra.s collect_loop

    # done:
    done_addr = len(program)
    program[beq_done] = (done_addr - beq_done - 1) & 0xFF
    program[bge_done] = (done_addr - bge_done - 1) & 0xFF

    # Store count and set done flag
    program.extend(MOVE_W_D1_ABS(0x00FF0500))  # move.w d1,$FF0500
    program.extend(MOVE_W_IMM_ABS(0xDEAD, 0x00FF0502))  # move.w #$DEAD,$FF0502

    # hang:
    hang = len(program)
    program.extend(BRA_8(-2))      # bra.s hang (infinite loop)

    # Build the full ROM
    rom = bytearray(0x200)  # Start with 512 bytes for header

    # Exception vectors at $000000
    rom[0x00:0x04] = long(0x00FFFFFE)  # Initial SSP
    rom[0x04:0x08] = long(0x00000200)  # Initial PC (start of our code)

    # Fill remaining vectors with address of hang loop
    hang_addr = 0x200 + hang
    for i in range(0x08, 0x100, 4):
        rom[i:i+4] = long(hang_addr)

    # ROM header at $000100
    header = bytearray(256)

    # Console name
    console = b"SEGA MEGA DRIVE "
    header[0x00:0x10] = console

    # Copyright
    copyright_str = b"(C)GXTEST 2026  "
    header[0x10:0x20] = copyright_str

    # Domestic name (48 bytes)
    domestic = b"PRIME SIEVE TEST ROM                            "
    header[0x20:0x50] = domestic[:48]

    # Overseas name (48 bytes)
    overseas = b"PRIME SIEVE TEST ROM                            "
    header[0x50:0x80] = overseas[:48]

    # Serial/version
    serial = b"GM 00000000-00"
    header[0x80:0x8E] = serial

    # Checksum (we'll calculate later)
    header[0x8E:0x90] = word(0)

    # I/O support
    header[0x90:0xA0] = b"J               "

    # ROM start/end
    rom_end = 0x200 + len(program)
    header[0xA0:0xA4] = long(0x00000000)  # ROM start
    header[0xA4:0xA8] = long(rom_end - 1)  # ROM end

    # RAM start/end
    header[0xA8:0xAC] = long(0x00FF0000)
    header[0xAC:0xB0] = long(0x00FFFFFF)

    # No SRAM
    header[0xB0:0xBC] = b"            "

    # Notes
    header[0xBC:0xC8] = b"            "

    # Region
    header[0xF0:0xF3] = b"JUE"

    rom[0x100:0x200] = header

    # Append program code
    rom.extend(program)

    # Pad to at least 512 bytes total, aligned to 512
    while len(rom) < 512 or len(rom) % 512 != 0:
        rom.append(0)

    # Calculate checksum (sum of words from $200 to end)
    checksum = 0
    for i in range(0x200, len(rom), 2):
        checksum += (rom[i] << 8) | rom[i+1]
    checksum &= 0xFFFF
    rom[0x18E:0x190] = word(checksum)

    return bytes(rom)

def generate_cpp_header(rom_data, output_path):
    """Generate C++ header with ROM data as byte array."""

    # First 100 primes for verification
    primes = [2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71,
              73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151,
              157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223, 227, 229, 233,
              239, 241, 251, 257, 263, 269, 271, 277, 281, 283, 293, 307, 311, 313, 317,
              331, 337, 347, 349, 353, 359, 367, 373, 379, 383, 389, 397, 401, 409, 419,
              421, 431, 433, 439, 443, 449, 457, 461, 463, 467, 479, 487, 491, 499, 503,
              509, 521, 523, 541]

    with open(output_path, 'w') as f:
        f.write('// Auto-generated by gen_prime_rom.py\n')
        f.write('// DO NOT EDIT\n\n')
        f.write('#ifndef PRIME_SIEVE_ROM_H\n')
        f.write('#define PRIME_SIEVE_ROM_H\n\n')
        f.write('#include <cstdint>\n')
        f.write('#include <cstddef>\n\n')
        f.write('namespace GX {\n')
        f.write('namespace TestRoms {\n\n')

        # ROM data
        f.write(f'// Prime Sieve ROM ({len(rom_data)} bytes)\n')
        f.write('// Computes first 100 primes using Sieve of Eratosthenes\n')
        f.write('// Results written to:\n')
        f.write('//   $FF0300-$FF03C7: Prime values (100 x 16-bit words)\n')
        f.write('//   $FF0500: Prime count\n')
        f.write('//   $FF0502: Done flag ($DEAD when complete)\n\n')

        f.write(f'constexpr size_t PRIME_SIEVE_ROM_SIZE = {len(rom_data)};\n\n')

        f.write('constexpr uint8_t PRIME_SIEVE_ROM[] = {\n')
        for i in range(0, len(rom_data), 16):
            chunk = rom_data[i:i+16]
            hex_str = ', '.join(f'0x{b:02X}' for b in chunk)
            f.write(f'    {hex_str},\n')
        f.write('};\n\n')

        # Expected primes
        f.write('// First 100 prime numbers for verification\n')
        f.write('constexpr uint16_t EXPECTED_PRIMES[] = {\n')
        for i in range(0, 100, 10):
            chunk = primes[i:i+10]
            f.write(f'    {", ".join(str(p) for p in chunk)},\n')
        f.write('};\n\n')

        f.write('constexpr size_t NUM_PRIMES = 100;\n\n')

        # Memory addresses
        f.write('// Memory addresses for test verification\n')
        f.write('constexpr uint32_t PRIME_RESULTS_ADDR = 0xFF0300;\n')
        f.write('constexpr uint32_t PRIME_COUNT_ADDR = 0xFF0500;\n')
        f.write('constexpr uint32_t DONE_FLAG_ADDR = 0xFF0502;\n')
        f.write('constexpr uint16_t DONE_FLAG_VALUE = 0xDEAD;\n\n')

        f.write('} // namespace TestRoms\n')
        f.write('} // namespace GX\n\n')
        f.write('#endif // PRIME_SIEVE_ROM_H\n')

def main():
    rom = generate_rom()

    # Write binary ROM file
    with open('prime_sieve.bin', 'wb') as f:
        f.write(rom)
    print(f"Generated prime_sieve.bin ({len(rom)} bytes)")

    # Generate C++ header
    generate_cpp_header(rom, 'prime_sieve_rom.h')
    print("Generated prime_sieve_rom.h")

    return 0

if __name__ == '__main__':
    sys.exit(main())
