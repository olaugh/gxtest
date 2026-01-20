# gxtest

A headless Genesis/Mega Drive test harness built on [Genesis Plus GX](https://github.com/ekeeke/Genesis-Plus-GX). gxtest enables fast, automated testing of Genesis ROMs using GoogleTest, with direct memory access for assertions and symbol-based debugging.

## Features

- **Headless Execution**: Run Genesis ROMs at maximum speed without graphics or audio output
- **GoogleTest Integration**: Write tests using familiar GoogleTest patterns and assertions
- **Direct Memory Access**: Read/write emulated RAM for state verification and injection
- **Symbol-Based Testing**: Assert on variable names (`Sym::player_score`) instead of magic addresses (`0xFF0008`)
- **CPU State Inspection**: Access M68K registers (D0-D7, A0-A7, PC, SR)
- **Input Simulation**: Programmatically control gamepad inputs
- **State Save/Load**: Capture and restore emulator state
- **Conditional Execution**: Run until memory conditions are met

## Thread Safety

**Important:** Genesis Plus GX uses global variables for emulator state, making it **not thread-safe**. Only one `GX::Emulator` instance may exist at a time per process.

```cpp
// This will throw std::runtime_error:
GX::Emulator emu1;
GX::Emulator emu2;  // ERROR: Only one instance allowed
```

### Parallel Test Execution

For parallel testing, use **process-based parallelism** instead of threads:

```bash
# CTest runs test executables in parallel (separate processes)
ctest -j 8

# Or use GoogleTest's parallel runner
./my_test --gtest_parallel
```

Each process gets its own copy of the global emulator state, avoiding conflicts.

**Do NOT use:**
- `std::thread` with multiple Emulator instances
- `std::async` with multiple Emulator instances
- Thread pools running different ROMs

See [Issue #2](https://github.com/olaugh/gxtest/issues/2) for details.

## Quick Start

### Building

```bash
mkdir build && cd build
cmake ..
make -j4
```

To enable building test ROMs from source (requires `m68k-elf-gcc`):

```bash
cmake -DGXTEST_BUILD_ROMS=ON ..
```

### Running Tests

```bash
ctest --output-on-failure
```

## Writing Tests

### Basic Test Fixture

```cpp
#include <gxtest.h>

class MyRomTest : public GX::Test {
protected:
    void SetUp() override {
        ASSERT_TRUE(emu.LoadRom("path/to/rom.bin"));
    }
};

TEST_F(MyRomTest, GameInitializes) {
    emu.RunFrames(60);  // Run 1 second at 60fps

    // Assert on memory values
    EXPECT_EQ(ReadWord(0xFF0100), 0x0001);
}
```

### Loading ROMs

```cpp
// From file
emu.LoadRom("roms/game.bin");

// From embedded byte array
emu.LoadRom(ROM_DATA, ROM_SIZE);
```

### Memory Access

```cpp
// Read memory (handles Genesis byte-swapping automatically)
uint8_t  byte  = ReadByte(0xFF0000);
uint16_t word  = ReadWord(0xFF0000);
uint32_t dword = ReadLong(0xFF0000);

// Write memory
WriteByte(0xFF0000, 0x42);
WriteWord(0xFF0000, 0x1234);
WriteLong(0xFF0000, 0xDEADBEEF);

// Direct RAM access (faster for bulk operations)
uint8_t* work_ram = emu.GetWorkRam();  // 64KB at 0xFF0000
uint8_t* z80_ram  = emu.GetZ80Ram();   // 8KB at 0xA00000
```

### Conditional Execution

```cpp
// Run until memory equals a value (or timeout)
int frames = emu.RunUntilMemoryEquals(0xFF0502, 0xAD, 1000);
ASSERT_GE(frames, 0) << "Condition not met within timeout";

// Run until custom condition
emu.RunUntil([this]() {
    return ReadWord(0xFF0100) >= 100;
}, 500);
```

### Input Simulation

```cpp
GX::Input input;
input.up = true;
input.a = true;
emu.SetInput(0, input);  // Player 1

emu.RunFrames(1);

// Or use convenience method
emu.PressButton(0, GX::Button::Start);
```

## Symbol-Based Testing

For readable tests that use variable names instead of addresses, gxtest supports extracting symbols from ELF files.

### Pipeline Overview

1. **Compile ROM** with debug symbols (`m68k-elf-gcc` produces `.elf`)
2. **Extract symbols** using `tools/elf2sym.py`
3. **Include generated header** in your tests
4. **Assert using symbol names**

### Example

ROM source (`main.c`):
```c
volatile uint16_t player_score = 0;
volatile uint8_t  player_lives = 3;

void main(void) {
    while (!game_over) {
        player_score += 10;
        // ...
    }
}
```

Extract symbols:
```bash
m68k-elf-nm -n rom.elf | python3 tools/elf2sym.py > symbols.h
```

Generated header (`symbols.h`):
```cpp
namespace Sym {
    constexpr uint32_t player_score = 0xFF0008;
    constexpr uint32_t player_lives = 0xFF0018;
}
```

Test using symbols:
```cpp
#include "symbols.h"

TEST_F(GameTest, ScoreIncrements) {
    emu.RunFrames(10);

    // Readable: assert on player_score, not 0xFF0008
    uint16_t score = ReadWord(Sym::player_score);
    EXPECT_GT(score, 0);
    EXPECT_EQ(score % 10, 0);
}

TEST_F(GameTest, InjectState) {
    // Set score directly for testing edge cases
    WriteWord(Sym::player_score, 9990);
    WriteByte(Sym::player_lives, 1);

    emu.RunFrames(1);

    // Verify game-over triggers at 10000 points
    EXPECT_EQ(ReadByte(Sym::game_over), 1);
}
```

### Benefits

- **Compile-time safety**: Renamed variables cause build failures, not runtime crashes
- **IDE autocomplete**: Symbol names appear in code completion
- **Self-documenting**: `Sym::player_score` is clearer than `0xFF0008`

## API Reference

### GX::Emulator

| Method | Description |
|--------|-------------|
| `LoadRom(path)` | Load ROM from file |
| `LoadRom(data, size)` | Load ROM from memory |
| `Reset()` | Soft reset |
| `HardReset()` | Hard reset (power cycle) |
| `RunFrames(n)` | Run n emulator frames |
| `RunUntilMemoryEquals(addr, val, max)` | Run until memory matches |
| `RunUntil(condition, max)` | Run until lambda returns true |
| `ReadByte/Word/Long(addr)` | Read memory |
| `WriteByte/Word/Long(addr, val)` | Write memory |
| `GetWorkRam()` | Direct pointer to 68K work RAM |
| `GetZ80Ram()` | Direct pointer to Z80 RAM |
| `GetDataRegister(n)` | Read D0-D7 |
| `GetAddressRegister(n)` | Read A0-A7 |
| `GetPC()` | Read program counter |
| `GetSR()` | Read status register |
| `SetInput(player, state)` | Set controller state |
| `SaveState()` | Capture emulator state |
| `LoadState(state)` | Restore emulator state |

### GX::Test

Base class for test fixtures. Provides:
- `emu` - Emulator instance
- `ReadByte/Word/Long()` - Convenience wrappers
- `WriteByte/Word/Long()` - Convenience wrappers

## Project Structure

```
gxtest/
├── include/
│   └── gxtest.h           # Public API
├── src/
│   ├── gxtest.cpp         # Implementation
│   ├── osd.h              # Platform abstraction
│   └── stubs.c            # Sega CD stubs
├── tests/
│   ├── example_test.cpp   # Basic test patterns
│   ├── prime_sieve_test.cpp
│   └── symbol_example_test.cpp
├── tools/
│   └── elf2sym.py         # Symbol extraction
├── roms/
│   ├── prime_sieve/       # Verification ROM
│   └── symbol_example/    # Symbol testing demo
└── vendor/
    └── genplusgx/         # Genesis Plus GX core
```

## Requirements

- CMake 3.14+
- C++17 compiler
- Python 3 (for symbol extraction)
- m68k-elf-gcc (optional, for building test ROMs)

## License

gxtest framework is provided under the MIT license. Genesis Plus GX is licensed under its own terms (see `vendor/genplusgx/`).
