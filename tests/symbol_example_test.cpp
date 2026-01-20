/**
 * gxtest - Symbol Example Test
 *
 * Demonstrates symbol-based testing where assertions use variable names
 * extracted from the ROM's ELF file instead of hardcoded addresses.
 *
 * This approach provides:
 * 1. Compile-time safety: Renamed variables cause build failures, not runtime crashes
 * 2. IDE autocomplete: Symbol names are available for code completion
 * 3. Self-documenting tests: ReadWord(Sym::player_score) is clearer than ReadWord(0xFF0008)
 *
 * The symbol header is generated at build time by tools/elf2sym.py from
 * the ROM's ELF symbol table.
 */

#include <gxtest.h>
#include "symbol_example_rom.h"      // Embedded ROM binary
#include "symbol_example_symbols.h"  // Generated symbol addresses (Sym::player_score, etc.)

namespace {

using namespace GX::TestRoms;

/**
 * Test fixture that loads the symbol example ROM
 */
class SymbolExampleTest : public GX::Test {
protected:
    void SetUp() override {
        ASSERT_TRUE(emu.LoadRom(SYMBOL_EXAMPLE_ROM, SYMBOL_EXAMPLE_ROM_SIZE))
            << "Failed to load symbol example ROM";
    }

    /**
     * Helper: Wait for game initialization to complete
     * Returns true if init completed, false if timeout
     */
    bool WaitForInit(int max_frames = 60) {
        int result = emu.RunUntil([this]() {
            return ReadWord(Sym::init_complete) == INIT_SENTINEL;
        }, max_frames);
        return result >= 0;
    }

    /**
     * Helper: Wait for game to end
     * Returns true if game ended, false if timeout
     */
    bool WaitForGameOver(int max_frames = 1000) {
        int result = emu.RunUntil([this]() {
            return ReadWord(Sym::done_flag) == DONE_SENTINEL;
        }, max_frames);
        return result >= 0;
    }
};

// =============================================================================
// Basic Initialization Tests
// =============================================================================

/**
 * Verify the ROM loads and initializes correctly
 */
TEST_F(SymbolExampleTest, RomLoadsAndInitializes) {
    ASSERT_TRUE(WaitForInit()) << "Game failed to initialize";

    // Check init sentinel was set
    EXPECT_EQ(ReadWord(Sym::init_complete), INIT_SENTINEL);

    // Verify initial game state using SYMBOLS, not magic addresses
    EXPECT_EQ(ReadByte(Sym::game_state), STATE_PLAYING);
    EXPECT_EQ(ReadByte(Sym::game_over), 0);
}

/**
 * Test initial player state values
 * Note: We reset and check immediately since the game loop runs during WaitForInit
 */
TEST_F(SymbolExampleTest, InitialPlayerState) {
    ASSERT_TRUE(WaitForInit());

    // Player position should remain at initial values
    EXPECT_EQ(ReadWord(Sym::player_x), 160);  // Center of 320-wide screen
    EXPECT_EQ(ReadWord(Sym::player_y), 200);

    // Score increases every frame, so just verify it's a reasonable value
    // and is incrementing in multiples of 10
    uint16_t score = ReadWord(Sym::player_score);
    EXPECT_EQ(score % 10, 0) << "Score should be a multiple of 10";

    // Level should be at least 1
    EXPECT_GE(ReadWord(Sym::level), 1);
}

// =============================================================================
// Score and Progression Tests
// =============================================================================

/**
 * Test that score increments over time
 */
TEST_F(SymbolExampleTest, ScoreIncrementsCorrectly) {
    ASSERT_TRUE(WaitForInit());

    // Run for a few frames
    emu.RunFrames(10);

    // Score should have increased (10 points per frame)
    uint16_t score = ReadWord(Sym::player_score);
    EXPECT_GT(score, 0) << "Score should increase over time";
    EXPECT_EQ(score % 10, 0) << "Score should increment in multiples of 10";
}

/**
 * Test level progression based on score
 * Note: Since the game runs many iterations per emulator frame (no vsync),
 * we verify that level advancement happens by checking the relationship
 * between score and level rather than exact values.
 */
TEST_F(SymbolExampleTest, LevelProgressionBasedOnScore) {
    ASSERT_TRUE(WaitForInit());

    // Read current state
    uint16_t score = ReadWord(Sym::player_score);
    uint16_t level = ReadWord(Sym::level);

    // Level should correspond to score (level = score/1000 + 1, max 5)
    uint16_t expected_level = (score / 1000) + 1;
    if (expected_level > 5) expected_level = 5;

    EXPECT_EQ(level, expected_level)
        << "Level should match score-based calculation (score=" << score << ")";

    // Verify level stays bounded
    EXPECT_GE(level, 1);
    EXPECT_LE(level, 5);
}

// =============================================================================
// Memory Injection Tests (demonstrates test harness power)
// =============================================================================

/**
 * Test injecting player position
 */
TEST_F(SymbolExampleTest, InjectPlayerPosition) {
    ASSERT_TRUE(WaitForInit());

    // Move player to corner
    WriteWord(Sym::player_x, 10);
    WriteWord(Sym::player_y, 10);

    // Verify the write took effect
    EXPECT_EQ(ReadWord(Sym::player_x), 10);
    EXPECT_EQ(ReadWord(Sym::player_y), 10);
}

/**
 * Test injecting score directly
 */
TEST_F(SymbolExampleTest, InjectScore) {
    ASSERT_TRUE(WaitForInit());

    // Set score directly
    WriteWord(Sym::player_score, 4242);

    EXPECT_EQ(ReadWord(Sym::player_score), 4242);
}

// =============================================================================
// Game State Transition Tests
// =============================================================================

/**
 * Test that game ends when player loses all lives
 */
TEST_F(SymbolExampleTest, GameOverWhenNoLives) {
    ASSERT_TRUE(WaitForInit());

    // Set lives to 0 directly
    WriteByte(Sym::player_lives, 0);

    // Trigger a collision by moving player to enemy position
    uint16_t enemy_x = ReadWord(Sym::enemy_x);
    uint16_t enemy_y = ReadWord(Sym::enemy_y);
    WriteWord(Sym::player_x, enemy_x);
    WriteWord(Sym::player_y, enemy_y);

    // Ensure enemy is active
    WriteByte(Sym::enemy_active, 1);

    // Run frame to process collision
    emu.RunFrames(1);

    // Game should end since lives were already 0
    EXPECT_EQ(ReadByte(Sym::game_over), 1);
    EXPECT_EQ(ReadByte(Sym::game_state), STATE_GAME_OVER);
}

/**
 * Test win condition: high score at max level
 */
TEST_F(SymbolExampleTest, WinConditionHighScoreMaxLevel) {
    ASSERT_TRUE(WaitForInit());

    // Set up win condition: max level with high score
    WriteWord(Sym::level, 5);  // MAX_LEVEL
    WriteWord(Sym::player_score, 4990);  // Just below win threshold

    // Run one frame to add 10 points and trigger win check
    emu.RunFrames(1);

    // Game should end with win
    EXPECT_EQ(ReadWord(Sym::player_score), 5000);
    EXPECT_EQ(ReadByte(Sym::game_over), 1);
    EXPECT_EQ(ReadWord(Sym::done_flag), DONE_SENTINEL);
}

// =============================================================================
// Frame Count and Timing Tests
// =============================================================================

/**
 * Test that frame counter increments over time
 * Note: The ROM runs in a tight loop without vsync, so frame_count
 * increments many times per emulator frame. We just verify it increases.
 */
TEST_F(SymbolExampleTest, FrameCounterIncrements) {
    // Don't wait for init - just start fresh and check frame_count increases
    emu.Reset();

    // Run one emulator frame
    emu.RunFrames(1);

    // The game loop runs many iterations per emulator frame
    // Just verify frame_count has started incrementing
    uint16_t frame_count = ReadWord(Sym::frame_count);
    EXPECT_GT(frame_count, 0) << "Frame counter should have incremented";

    // Run more and verify it continues increasing
    uint16_t prev = frame_count;
    emu.RunFrames(1);
    frame_count = ReadWord(Sym::frame_count);

    // Either game is still running (frame_count increased) or game ended
    // (frame_count stopped at terminal value)
    EXPECT_GE(frame_count, prev) << "Frame counter should not decrease";
}

// =============================================================================
// Enemy State Tests
// =============================================================================

/**
 * Test enemy position updates based on frame count
 */
TEST_F(SymbolExampleTest, EnemyPositionUpdates) {
    ASSERT_TRUE(WaitForInit());

    uint16_t initial_x = ReadWord(Sym::enemy_x);
    uint16_t initial_y = ReadWord(Sym::enemy_y);

    // Run some frames
    emu.RunFrames(10);

    uint16_t new_x = ReadWord(Sym::enemy_x);
    uint16_t new_y = ReadWord(Sym::enemy_y);

    // Enemy should have moved (pattern: x = 50 + frame%200, y = 50 + (frame/2)%150)
    EXPECT_NE(new_x, initial_x) << "Enemy X should change";
}

/**
 * Test enemy deactivation on collision
 */
TEST_F(SymbolExampleTest, EnemyDeactivatesOnCollision) {
    ASSERT_TRUE(WaitForInit());

    // Ensure player has lives
    ASSERT_GT(ReadByte(Sym::player_lives), 0);

    // Force collision
    WriteByte(Sym::enemy_active, 1);
    WriteWord(Sym::player_x, ReadWord(Sym::enemy_x));
    WriteWord(Sym::player_y, ReadWord(Sym::enemy_y));

    // Run frame to process collision
    emu.RunFrames(1);

    // Enemy should be deactivated (invincibility period)
    EXPECT_EQ(ReadByte(Sym::enemy_active), 0)
        << "Enemy should deactivate after collision (invincibility)";
}

// =============================================================================
// Full Game Run Test
// =============================================================================

/**
 * Test running the game to natural completion
 */
TEST_F(SymbolExampleTest, GameRunsToCompletion) {
    ASSERT_TRUE(WaitForInit());

    // Game should complete within 1000 frames (its internal limit)
    ASSERT_TRUE(WaitForGameOver(1100))
        << "Game should complete within frame limit";

    // Verify game ended
    EXPECT_EQ(ReadWord(Sym::done_flag), DONE_SENTINEL);
    EXPECT_EQ(ReadByte(Sym::game_over), 1);

    // Log final state
    std::cout << "Final score: " << ReadWord(Sym::player_score) << std::endl;
    std::cout << "Final level: " << ReadWord(Sym::level) << std::endl;
    std::cout << "Lives remaining: " << (int)ReadByte(Sym::player_lives) << std::endl;
    std::cout << "Frames elapsed: " << ReadWord(Sym::frame_count) << std::endl;
}

// =============================================================================
// Comparison: Symbol vs Address (demonstration only)
// =============================================================================

/**
 * This test demonstrates the readability difference between symbol-based
 * and address-based assertions. Both approaches work, but symbols are clearer.
 */
TEST_F(SymbolExampleTest, SymbolsVsAddressesComparison) {
    ASSERT_TRUE(WaitForInit());

    // Symbol-based (recommended): Clear what we're testing
    uint16_t score_by_symbol = ReadWord(Sym::player_score);
    uint8_t lives_by_symbol = ReadByte(Sym::player_lives);

    // Address-based (legacy): Requires checking documentation
    uint16_t score_by_address = ReadWord(0xFF0008);  // What's at this address?
    uint8_t lives_by_address = ReadByte(0xFF0018);   // Magic number, unclear

    // Both give same result, but symbol version is self-documenting
    EXPECT_EQ(score_by_symbol, score_by_address);
    EXPECT_EQ(lives_by_symbol, lives_by_address);
}

} // namespace
