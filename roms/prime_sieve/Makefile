# Prime Sieve ROM Makefile
# Cross-compiles C code to a Genesis/Mega Drive ROM

# Cross-compiler toolchain
PREFIX ?= m68k-elf-
CC = $(PREFIX)gcc
AS = $(PREFIX)as
LD = $(PREFIX)ld
OBJCOPY = $(PREFIX)objcopy

# Flags
CFLAGS = -m68000 -Os -fomit-frame-pointer -fno-builtin -nostdlib -Wall
ASFLAGS = -m68000
LDFLAGS = -T genesis.ld -nostdlib

# Output
TARGET = prime_sieve
ROM = $(TARGET).bin
HEADER = prime_sieve_rom.h

# Source files
CSRC = main.c
ASRC = crt0.s

# Object files
OBJS = crt0.o main.o

.PHONY: all clean header

all: $(ROM) $(HEADER)

# Link object files into ELF
$(TARGET).elf: $(OBJS) genesis.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

# Convert ELF to binary ROM
$(ROM): $(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	@echo "ROM size: $$(wc -c < $@) bytes"

# Compile C sources
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Assemble startup code
%.o: %.s
	$(AS) $(ASFLAGS) -o $@ $<

# Generate C++ header with embedded ROM data
$(HEADER): $(ROM)
	@echo "// Auto-generated from $(ROM)" > $@
	@echo "// Built from C source code" >> $@
	@echo "" >> $@
	@echo "#ifndef PRIME_SIEVE_ROM_H" >> $@
	@echo "#define PRIME_SIEVE_ROM_H" >> $@
	@echo "" >> $@
	@echo "#include <cstdint>" >> $@
	@echo "#include <cstddef>" >> $@
	@echo "" >> $@
	@echo "namespace GX {" >> $@
	@echo "namespace TestRoms {" >> $@
	@echo "" >> $@
	@echo "// Prime Sieve ROM ($$(wc -c < $(ROM) | tr -d ' ') bytes)" >> $@
	@echo "// Computes first 100 primes using Sieve of Eratosthenes" >> $@
	@echo "// Results written to:" >> $@
	@echo "//   \$$FF0300-\$$FF03C7: Prime values (100 x 16-bit words)" >> $@
	@echo "//   \$$FF0500: Prime count" >> $@
	@echo "//   \$$FF0502: Done flag (\$$DEAD when complete)" >> $@
	@echo "" >> $@
	@echo "constexpr size_t PRIME_SIEVE_ROM_SIZE = $$(wc -c < $(ROM) | tr -d ' ');" >> $@
	@echo "" >> $@
	@echo "constexpr uint8_t PRIME_SIEVE_ROM[] = {" >> $@
	@xxd -i < $(ROM) | sed 's/^/    /' >> $@
	@echo "};" >> $@
	@echo "" >> $@
	@echo "// First 100 prime numbers for verification" >> $@
	@echo "constexpr uint16_t EXPECTED_PRIMES[] = {" >> $@
	@echo "    2, 3, 5, 7, 11, 13, 17, 19, 23, 29," >> $@
	@echo "    31, 37, 41, 43, 47, 53, 59, 61, 67, 71," >> $@
	@echo "    73, 79, 83, 89, 97, 101, 103, 107, 109, 113," >> $@
	@echo "    127, 131, 137, 139, 149, 151, 157, 163, 167, 173," >> $@
	@echo "    179, 181, 191, 193, 197, 199, 211, 223, 227, 229," >> $@
	@echo "    233, 239, 241, 251, 257, 263, 269, 271, 277, 281," >> $@
	@echo "    283, 293, 307, 311, 313, 317, 331, 337, 347, 349," >> $@
	@echo "    353, 359, 367, 373, 379, 383, 389, 397, 401, 409," >> $@
	@echo "    419, 421, 431, 433, 439, 443, 449, 457, 461, 463," >> $@
	@echo "    467, 479, 487, 491, 499, 503, 509, 521, 523, 541," >> $@
	@echo "};" >> $@
	@echo "" >> $@
	@echo "constexpr size_t NUM_PRIMES = 100;" >> $@
	@echo "" >> $@
	@echo "// Memory addresses for test verification" >> $@
	@echo "constexpr uint32_t PRIME_RESULTS_ADDR = 0xFF0300;" >> $@
	@echo "constexpr uint32_t PRIME_COUNT_ADDR = 0xFF0500;" >> $@
	@echo "constexpr uint32_t DONE_FLAG_ADDR = 0xFF0502;" >> $@
	@echo "constexpr uint16_t DONE_FLAG_VALUE = 0xDEAD;" >> $@
	@echo "" >> $@
	@echo "} // namespace TestRoms" >> $@
	@echo "} // namespace GX" >> $@
	@echo "" >> $@
	@echo "#endif // PRIME_SIEVE_ROM_H" >> $@
	@echo "Generated $(HEADER)"

clean:
	rm -f $(OBJS) $(TARGET).elf $(ROM) $(HEADER)

# Install header to tests directory
install: $(HEADER)
	cp $(HEADER) ../../tests/
	@echo "Installed $(HEADER) to tests/"
