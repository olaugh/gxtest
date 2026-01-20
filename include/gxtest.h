/**
 * gxtest - A GoogleTest-compatible verification harness for Sega Genesis ROMs
 *
 * This library wraps the Genesis Plus GX emulator core, providing a clean C++
 * interface for headless execution and memory instrumentation.
 *
 * THREAD SAFETY WARNING:
 * The Genesis Plus GX emulator uses global state throughout its implementation
 * (config, bitmap, cart.rom, work_ram, zram, input state, VDP state, etc.).
 * This means:
 *
 *   - Multiple GX::Emulator instances CANNOT run concurrently in threads
 *   - Creating multiple Emulator objects shares the same underlying state
 *   - For parallel test execution, use process-based parallelism (fork())
 *     instead of thread-based parallelism (std::thread, std::async)
 *
 * Example of SAFE parallel execution (fork-based):
 *
 *   pid_t pid = fork();
 *   if (pid == 0) {
 *       // Child process - has its own copy of global state
 *       GX::Emulator emu;
 *       emu.LoadRom("game.bin");
 *       emu.RunFrames(1000);
 *       // Write results to pipe, exit
 *   }
 *   // Parent collects results from children
 *
 * Example of UNSAFE parallel execution (will crash or corrupt state):
 *
 *   // DON'T DO THIS - threads share global state
 *   std::thread t1([](){ GX::Emulator e1; e1.LoadRom("a.bin"); e1.RunFrames(100); });
 *   std::thread t2([](){ GX::Emulator e2; e2.LoadRom("b.bin"); e2.RunFrames(100); });
 *
 * Basic usage example:
 *
 *   #include <gxtest.h>
 *
 *   class MyGameTest : public GX::Test {
 *   protected:
 *       void SetUp() override {
 *           LoadRom("game.bin");
 *           RunFrames(60);  // Skip intro
 *       }
 *   };
 *
 *   TEST_F(MyGameTest, CheckMemoryState) {
 *       RunFrames(1);
 *       EXPECT_EQ(ReadByte(0xFF0000), 0x42);
 *   }
 */

#ifndef GXTEST_H
#define GXTEST_H

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <cstdint>
#include <functional>

namespace GX {

/**
 * Input state for a single controller
 */
struct Input {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool a = false;
    bool b = false;
    bool c = false;
    bool start = false;
    bool x = false;      // 6-button
    bool y = false;      // 6-button
    bool z = false;      // 6-button
    bool mode = false;   // 6-button

    void Clear() {
        up = down = left = right = false;
        a = b = c = start = false;
        x = y = z = mode = false;
    }
};

/**
 * Emulator wrapper class providing the test harness interface
 *
 * WARNING: NOT THREAD-SAFE. Genesis Plus GX uses global state, so only one
 * Emulator can be active per process. For parallel execution, use fork()
 * to run each emulator in a separate process. See file header for details.
 */
class Emulator {
public:
    Emulator();
    ~Emulator();

    // Prevent copying (singleton emulator state - there's only one underlying emulator)
    Emulator(const Emulator&) = delete;
    Emulator& operator=(const Emulator&) = delete;

    /**
     * Load a ROM file from disk
     * @param path Path to the ROM file (.bin, .md, .gen, .smd)
     * @return true if loaded successfully
     */
    bool LoadRom(const std::string& path);

    /**
     * Load a ROM from memory buffer
     * @param data ROM data
     * @param size Size of ROM in bytes
     * @return true if loaded successfully
     */
    bool LoadRom(const uint8_t* data, size_t size);

    /**
     * Reset the emulated system (soft reset)
     */
    void Reset();

    /**
     * Hard reset (power cycle)
     */
    void HardReset();

    /**
     * Run the emulator for a specified number of frames
     * @param frames Number of frames to execute
     */
    void RunFrames(int frames);

    /**
     * Run until a memory condition is met or max frames reached
     * @param address Memory address to monitor (68k address space)
     * @param expected Expected value
     * @param max_frames Maximum frames before timeout
     * @return Frame number when condition was met, or -1 if timeout
     */
    int RunUntilMemoryEquals(uint32_t address, uint8_t expected, int max_frames);

    /**
     * Run until a callback returns true or max frames reached
     * @param condition Callback invoked each frame, return true to stop
     * @param max_frames Maximum frames before timeout
     * @return Frame number when condition was met, or -1 if timeout
     */
    int RunUntil(std::function<bool()> condition, int max_frames);

    // -------------------------------------------------------------------------
    // Memory Access (68k address space: 0x000000 - 0xFFFFFF)
    // -------------------------------------------------------------------------

    /** Read a byte from 68k address space */
    uint8_t ReadByte(uint32_t address) const;

    /** Read a 16-bit word from 68k address space (big-endian) */
    uint16_t ReadWord(uint32_t address) const;

    /** Read a 32-bit long from 68k address space (big-endian) */
    uint32_t ReadLong(uint32_t address) const;

    /** Write a byte to 68k address space */
    void WriteByte(uint32_t address, uint8_t value);

    /** Write a 16-bit word to 68k address space */
    void WriteWord(uint32_t address, uint16_t value);

    /** Write a 32-bit long to 68k address space */
    void WriteLong(uint32_t address, uint32_t value);

    // -------------------------------------------------------------------------
    // Direct RAM Access (faster, for known RAM regions)
    // -------------------------------------------------------------------------

    /** Direct access to 68k Work RAM (0xFF0000-0xFFFFFF, 64KB) */
    uint8_t* GetWorkRam();
    const uint8_t* GetWorkRam() const;

    /** Direct access to Z80 RAM (0xA00000-0xA01FFF, 8KB) */
    uint8_t* GetZ80Ram();
    const uint8_t* GetZ80Ram() const;

    // -------------------------------------------------------------------------
    // CPU Register Access
    // -------------------------------------------------------------------------

    /** Get 68k data register (D0-D7) */
    uint32_t GetDataRegister(int reg) const;

    /** Get 68k address register (A0-A7) */
    uint32_t GetAddressRegister(int reg) const;

    /** Get 68k program counter */
    uint32_t GetPC() const;

    /** Get 68k status register */
    uint16_t GetSR() const;

    // -------------------------------------------------------------------------
    // Input Control
    // -------------------------------------------------------------------------

    /** Set input state for player 1 or 2 (0-indexed) */
    void SetInput(int player, const Input& input);

    /** Get current input state */
    const Input& GetInput(int player) const;

    /** Press a button for one frame, then release */
    void PressButton(int player, const std::string& button);

    // -------------------------------------------------------------------------
    // State Management
    // -------------------------------------------------------------------------

    /** Save current state to buffer */
    std::vector<uint8_t> SaveState() const;

    /** Load state from buffer */
    bool LoadState(const std::vector<uint8_t>& state);

    // -------------------------------------------------------------------------
    // Info
    // -------------------------------------------------------------------------

    /** Get current frame count since reset */
    uint64_t GetFrameCount() const;

    /** Get ROM header info */
    std::string GetRomName() const;

    /** Check if ROM is loaded */
    bool IsRomLoaded() const;

private:
    class Impl;
    Impl* pImpl;
};

/**
 * GoogleTest fixture base class for Genesis ROM testing
 *
 * Inherit from this class to create test fixtures with automatic
 * emulator setup and teardown.
 */
class Test : public ::testing::Test {
protected:
    Emulator emu;

    // Convenience wrappers that forward to emu
    bool LoadRom(const std::string& path) { return emu.LoadRom(path); }
    void Reset() { emu.Reset(); }
    void HardReset() { emu.HardReset(); }
    void RunFrames(int frames) { emu.RunFrames(frames); }

    uint8_t ReadByte(uint32_t addr) const { return emu.ReadByte(addr); }
    uint16_t ReadWord(uint32_t addr) const { return emu.ReadWord(addr); }
    uint32_t ReadLong(uint32_t addr) const { return emu.ReadLong(addr); }

    void WriteByte(uint32_t addr, uint8_t val) { emu.WriteByte(addr, val); }
    void WriteWord(uint32_t addr, uint16_t val) { emu.WriteWord(addr, val); }
    void WriteLong(uint32_t addr, uint32_t val) { emu.WriteLong(addr, val); }

    uint32_t GetD(int reg) const { return emu.GetDataRegister(reg); }
    uint32_t GetA(int reg) const { return emu.GetAddressRegister(reg); }
    uint32_t GetPC() const { return emu.GetPC(); }

    void SetInput(int player, const Input& input) { emu.SetInput(player, input); }
    void PressButton(int player, const std::string& button) { emu.PressButton(player, button); }

    uint64_t GetFrameCount() const { return emu.GetFrameCount(); }
};

} // namespace GX

#endif // GXTEST_H
