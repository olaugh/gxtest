# Symbol Example ROM Makefile
# Cross-compiles C code to a Genesis/Mega Drive ROM with symbol extraction

# Cross-compiler toolchain
PREFIX ?= m68k-elf-
CC = $(PREFIX)gcc
AS = $(PREFIX)as
LD = $(PREFIX)ld
NM = $(PREFIX)nm
OBJCOPY = $(PREFIX)objcopy

# Flags
CFLAGS = -m68000 -Os -fomit-frame-pointer -fno-builtin -nostdlib -Wall -Wno-main
ASFLAGS = -m68000
LDFLAGS = -T genesis.ld -nostdlib

# Output
TARGET = symbol_example
ROM = $(TARGET).bin
ELF = $(TARGET).elf
ROM_HEADER = symbol_example_rom.h
SYM_HEADER = symbol_example_symbols.h
SYM_TXT = $(TARGET)_symbols.txt

# Tools
ELF2SYM = ../../tools/elf2sym.py

# Source files
CSRC = main.c
ASRC = crt0.s

# Object files
OBJS = crt0.o main.o

.PHONY: all clean header symbols install

all: $(ROM) $(ROM_HEADER) $(SYM_HEADER)

# Link object files into ELF
$(ELF): $(OBJS) genesis.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

# Convert ELF to binary ROM
$(ROM): $(ELF)
	$(OBJCOPY) -O binary $< $@
	@echo "ROM size: $$(wc -c < $@) bytes"

# Compile C sources
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Assemble startup code
%.o: %.s
	$(AS) $(ASFLAGS) -o $@ $<

# Extract symbols from ELF
$(SYM_TXT): $(ELF)
	$(NM) -n $< > $@

# Generate C++ symbol header using elf2sym.py
$(SYM_HEADER): $(SYM_TXT) $(ELF2SYM)
	python3 $(ELF2SYM) $< > $@
	@echo "Generated $(SYM_HEADER)"

# Generate C++ header with embedded ROM data
$(ROM_HEADER): $(ROM)
	@echo "// Auto-generated from $(ROM)" > $@
	@echo "// Built from C source code" >> $@
	@echo "" >> $@
	@echo "#ifndef SYMBOL_EXAMPLE_ROM_H" >> $@
	@echo "#define SYMBOL_EXAMPLE_ROM_H" >> $@
	@echo "" >> $@
	@echo "#include <cstdint>" >> $@
	@echo "#include <cstddef>" >> $@
	@echo "" >> $@
	@echo "namespace GX {" >> $@
	@echo "namespace TestRoms {" >> $@
	@echo "" >> $@
	@echo "// Symbol Example ROM ($$(wc -c < $(ROM) | tr -d ' ') bytes)" >> $@
	@echo "// A simple game simulation demonstrating symbol-based testing" >> $@
	@echo "//" >> $@
	@echo "// Global variables available via generated symbols header:" >> $@
	@echo "//   player_score, player_lives, player_x, player_y" >> $@
	@echo "//   game_state, game_over, frame_count, level" >> $@
	@echo "//   enemy_x, enemy_y, enemy_active" >> $@
	@echo "//   init_complete, done_flag" >> $@
	@echo "" >> $@
	@echo "constexpr size_t SYMBOL_EXAMPLE_ROM_SIZE = $$(wc -c < $(ROM) | tr -d ' ');" >> $@
	@echo "" >> $@
	@echo "constexpr uint8_t SYMBOL_EXAMPLE_ROM[] = {" >> $@
	@xxd -i < $(ROM) | sed 's/^/    /' >> $@
	@echo "};" >> $@
	@echo "" >> $@
	@echo "// Sentinel values for synchronization" >> $@
	@echo "constexpr uint16_t INIT_SENTINEL = 0xBEEF;" >> $@
	@echo "constexpr uint16_t DONE_SENTINEL = 0xDEAD;" >> $@
	@echo "" >> $@
	@echo "// Game state values" >> $@
	@echo "constexpr uint8_t STATE_INIT = 0;" >> $@
	@echo "constexpr uint8_t STATE_PLAYING = 1;" >> $@
	@echo "constexpr uint8_t STATE_PAUSED = 2;" >> $@
	@echo "constexpr uint8_t STATE_GAME_OVER = 3;" >> $@
	@echo "" >> $@
	@echo "} // namespace TestRoms" >> $@
	@echo "} // namespace GX" >> $@
	@echo "" >> $@
	@echo "#endif // SYMBOL_EXAMPLE_ROM_H" >> $@
	@echo "Generated $(ROM_HEADER)"

symbols: $(SYM_HEADER)
	@echo "Symbols:"
	@cat $(SYM_HEADER)

clean:
	rm -f $(OBJS) $(ELF) $(ROM) $(ROM_HEADER) $(SYM_HEADER) $(SYM_TXT)

# Install headers to tests directory
install: $(ROM_HEADER) $(SYM_HEADER)
	cp $(ROM_HEADER) $(SYM_HEADER) ../../tests/
	@echo "Installed headers to tests/"
