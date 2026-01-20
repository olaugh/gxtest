/**
 * gxtest - Example test file
 *
 * This file demonstrates how to use the gxtest framework to write
 * unit tests for Sega Genesis ROMs.
 */

#include <gxtest.h>
#include <stdexcept>

namespace {

// ---------------------------------------------------------------------------
// Basic Emulator Tests (no ROM required)
// ---------------------------------------------------------------------------

TEST(GXTestBasic, EmulatorInitializes) {
    GX::Emulator emu;
    EXPECT_FALSE(emu.IsRomLoaded());
    EXPECT_EQ(emu.GetFrameCount(), 0u);
}

TEST(GXTestBasic, LoadNonexistentRomFails) {
    GX::Emulator emu;
    EXPECT_FALSE(emu.LoadRom("/nonexistent/path/to/rom.bin"));
    EXPECT_FALSE(emu.IsRomLoaded());
}

/**
 * Test that creating multiple Emulator instances throws an exception.
 * This verifies the thread-safety guard documented in issue #2.
 */
TEST(GXTestBasic, MultipleInstancesThrows) {
    GX::Emulator emu1;  // First instance should succeed
    EXPECT_FALSE(emu1.IsRomLoaded());

    // Second instance should throw
    EXPECT_THROW({
        GX::Emulator emu2;
    }, std::runtime_error);

    // First instance should still be valid
    EXPECT_FALSE(emu1.IsRomLoaded());
}

// ---------------------------------------------------------------------------
// Example ROM Test Fixture
// ---------------------------------------------------------------------------

/**
 * Example test fixture that demonstrates the recommended pattern
 * for testing a specific ROM.
 *
 * To use this with your own ROM:
 * 1. Set the ROM_PATH constant to your ROM file
 * 2. Customize SetUp() to skip past any boot screens
 * 3. Write tests that assert on memory states
 */
class ExampleRomTest : public GX::Test {
protected:
    // Set this to your ROM path
    static constexpr const char* ROM_PATH = "test_rom.bin";

    void SetUp() override {
        // Skip test if ROM doesn't exist
        if (!LoadRom(ROM_PATH)) {
            GTEST_SKIP() << "Test ROM not found at: " << ROM_PATH;
        }

        // Example: Skip past the SEGA logo (typically ~180 frames)
        // RunFrames(180);
    }
};

// This test will be skipped if the ROM isn't present
TEST_F(ExampleRomTest, RomLoads) {
    EXPECT_TRUE(emu.IsRomLoaded());
    // EXPECT_EQ(emu.GetRomName(), "YOUR GAME NAME");
}

TEST_F(ExampleRomTest, MemoryAccessWorks) {
    // Run a few frames
    RunFrames(10);

    // Example memory checks (addresses depend on your ROM)
    // uint8_t value = ReadByte(0xFF0000);
    // EXPECT_NE(value, 0) << "Expected work RAM to be initialized";

    // Example: Check a specific variable
    // uint16_t score = ReadWord(0xFF1000);
    // EXPECT_EQ(score, 0) << "Score should start at 0";
}

// ---------------------------------------------------------------------------
// Input Test Example
// ---------------------------------------------------------------------------

TEST_F(ExampleRomTest, InputStateWorks) {
    GX::Input input;
    input.start = true;

    emu.SetInput(0, input);
    RunFrames(1);

    // After pressing start, check if game state changed
    // uint8_t game_state = ReadByte(0xFF0100);
    // EXPECT_EQ(game_state, 1) << "Game should transition to playing state";
}

// ---------------------------------------------------------------------------
// State Save/Load Example
// ---------------------------------------------------------------------------

TEST_F(ExampleRomTest, SaveStateWorks) {
    // Run some frames
    RunFrames(60);

    // Save state
    auto state1 = emu.SaveState();
    EXPECT_FALSE(state1.empty());

    // Run more frames
    RunFrames(60);

    // Memory should have changed
    // auto value_after = ReadByte(0xFF0000);

    // Restore state
    EXPECT_TRUE(emu.LoadState(state1));

    // Memory should be back to saved state
    // EXPECT_EQ(ReadByte(0xFF0000), value_before);
}

// ---------------------------------------------------------------------------
// Performance Test Example
// ---------------------------------------------------------------------------

TEST_F(ExampleRomTest, CanRunManyFrames) {
    // This demonstrates headless performance
    // In headless mode, this should complete very quickly
    RunFrames(600);  // 10 seconds of game time at 60fps

    EXPECT_EQ(GetFrameCount(), 600u);
}

// ---------------------------------------------------------------------------
// Conditional Execution Example
// ---------------------------------------------------------------------------

TEST_F(ExampleRomTest, RunUntilCondition) {
    // Example: Run until a specific memory location changes
    // This is useful for waiting for game state transitions

    // int frame = emu.RunUntilMemoryEquals(0xFF0100, 0x01, 300);
    // EXPECT_GE(frame, 0) << "Condition should be met within 300 frames";
    // EXPECT_LT(frame, 300) << "Should not timeout";
}

// ---------------------------------------------------------------------------
// Custom Condition Example
// ---------------------------------------------------------------------------

TEST_F(ExampleRomTest, RunUntilCustomCondition) {
    // Example: Run until a complex condition is met
    // auto frame = emu.RunUntil([this]() {
    //     // Check multiple memory locations
    //     uint8_t state = ReadByte(0xFF0100);
    //     uint16_t score = ReadWord(0xFF0102);
    //     return state == 1 && score > 0;
    // }, 600);
    //
    // EXPECT_GE(frame, 0) << "Game should start and score within 600 frames";
}

} // namespace
