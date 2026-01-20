/**
 * gxtest - Prime Sieve Test
 *
 * This test runs a Genesis ROM that computes the first 100 primes using
 * the Sieve of Eratosthenes, then verifies the results by reading them
 * directly from emulated memory.
 *
 * This demonstrates:
 * 1. Loading a ROM from an embedded byte array
 * 2. Running the emulator headlessly at maximum speed
 * 3. Polling memory for a completion flag
 * 4. Asserting on memory values to verify correctness
 */

#include <gxtest.h>
#include "prime_sieve_rom.h"

namespace {

using namespace GX::TestRoms;

class PrimeSieveTest : public GX::Test {
protected:
    void SetUp() override {
        // Load ROM from embedded byte array
        ASSERT_TRUE(emu.LoadRom(PRIME_SIEVE_ROM, PRIME_SIEVE_ROM_SIZE))
            << "Failed to load embedded prime sieve ROM";
    }
};

/**
 * Test that the ROM loads and the emulator initializes correctly
 */
TEST_F(PrimeSieveTest, RomLoads) {
    EXPECT_TRUE(emu.IsRomLoaded());
    EXPECT_EQ(emu.GetFrameCount(), 0u);
}

/**
 * Test that the sieve completes within a reasonable number of frames
 */
TEST_F(PrimeSieveTest, SieveCompletes) {
    // The sieve should complete very quickly (well under 1 second of game time)
    // Run up to 60 frames (1 second at 60fps)
    const int MAX_FRAMES = 60;

    int completion_frame = emu.RunUntilMemoryEquals(
        DONE_FLAG_ADDR + 1,  // Check low byte of done flag
        0xAD,                // Low byte of 0xDEAD
        MAX_FRAMES
    );

    EXPECT_GE(completion_frame, 0)
        << "Sieve did not complete within " << MAX_FRAMES << " frames";

    // Verify the done flag is fully set
    uint16_t done_flag = ReadWord(DONE_FLAG_ADDR);
    EXPECT_EQ(done_flag, DONE_FLAG_VALUE)
        << "Done flag should be 0xDEAD, got 0x" << std::hex << done_flag;
}

/**
 * Test that the correct number of primes were found
 */
TEST_F(PrimeSieveTest, CorrectPrimeCount) {
    // Run until completion
    emu.RunUntil([this]() {
        return ReadWord(DONE_FLAG_ADDR) == DONE_FLAG_VALUE;
    }, 60);

    uint16_t count = ReadWord(PRIME_COUNT_ADDR);
    EXPECT_EQ(count, NUM_PRIMES)
        << "Expected " << NUM_PRIMES << " primes, found " << count;
}

/**
 * Test that all 100 primes are correctly computed
 */
TEST_F(PrimeSieveTest, AllPrimesCorrect) {
    // Run until completion
    emu.RunUntil([this]() {
        return ReadWord(DONE_FLAG_ADDR) == DONE_FLAG_VALUE;
    }, 60);

    // Verify each prime
    for (size_t i = 0; i < NUM_PRIMES; i++) {
        uint32_t addr = PRIME_RESULTS_ADDR + (i * 2);
        uint16_t computed = ReadWord(addr);
        uint16_t expected = EXPECTED_PRIMES[i];

        EXPECT_EQ(computed, expected)
            << "Prime #" << (i + 1) << " mismatch at address 0x"
            << std::hex << addr << ": expected " << std::dec << expected
            << ", got " << computed;
    }
}

/**
 * Test specific prime values for quick sanity check
 */
TEST_F(PrimeSieveTest, SpotCheckPrimes) {
    // Run until completion
    emu.RunUntil([this]() {
        return ReadWord(DONE_FLAG_ADDR) == DONE_FLAG_VALUE;
    }, 60);

    // Check first prime (2)
    EXPECT_EQ(ReadWord(PRIME_RESULTS_ADDR), 2) << "First prime should be 2";

    // Check 10th prime (29)
    EXPECT_EQ(ReadWord(PRIME_RESULTS_ADDR + 18), 29) << "10th prime should be 29";

    // Check 25th prime (97)
    EXPECT_EQ(ReadWord(PRIME_RESULTS_ADDR + 48), 97) << "25th prime should be 97";

    // Check 50th prime (229)
    EXPECT_EQ(ReadWord(PRIME_RESULTS_ADDR + 98), 229) << "50th prime should be 229";

    // Check 100th prime (541)
    EXPECT_EQ(ReadWord(PRIME_RESULTS_ADDR + 198), 541) << "100th prime should be 541";
}

/**
 * Performance test: measure how fast the sieve runs
 */
TEST_F(PrimeSieveTest, PerformanceBenchmark) {
    auto start = std::chrono::high_resolution_clock::now();

    // Run until completion
    int frames = emu.RunUntil([this]() {
        return ReadWord(DONE_FLAG_ADDR) == DONE_FLAG_VALUE;
    }, 1000);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    ASSERT_GE(frames, 0) << "Sieve should complete";

    // Log performance info (not a failure condition)
    std::cout << "Prime sieve completed in " << frames << " frame(s)" << std::endl;
    std::cout << "Wall clock time: " << duration.count() << " microseconds" << std::endl;

    if (frames > 0) {
        double fps = (frames * 1000000.0) / duration.count();
        std::cout << "Effective frame rate: " << fps << " FPS" << std::endl;
    }
}

/**
 * Test save/load state preserves computation results
 * DISABLED: State save/load crashes due to incomplete stub implementations
 */
TEST_F(PrimeSieveTest, DISABLED_SaveStatePreservesResults) {
    // Run until completion
    emu.RunUntil([this]() {
        return ReadWord(DONE_FLAG_ADDR) == DONE_FLAG_VALUE;
    }, 60);

    // Save state
    auto state = emu.SaveState();
    ASSERT_FALSE(state.empty()) << "Save state should not be empty";

    // Record some prime values
    uint16_t prime1 = ReadWord(PRIME_RESULTS_ADDR);
    uint16_t prime50 = ReadWord(PRIME_RESULTS_ADDR + 98);
    uint16_t prime100 = ReadWord(PRIME_RESULTS_ADDR + 198);

    // Reset emulator (clears memory)
    emu.Reset();

    // Memory should be different after reset
    // (The ROM will start computing again from scratch)

    // Restore state
    ASSERT_TRUE(emu.LoadState(state)) << "Failed to load state";

    // Verify primes are restored
    EXPECT_EQ(ReadWord(PRIME_RESULTS_ADDR), prime1);
    EXPECT_EQ(ReadWord(PRIME_RESULTS_ADDR + 98), prime50);
    EXPECT_EQ(ReadWord(PRIME_RESULTS_ADDR + 198), prime100);
    EXPECT_EQ(ReadWord(DONE_FLAG_ADDR), DONE_FLAG_VALUE);
}

/**
 * Stress test: run the computation multiple times
 */
TEST_F(PrimeSieveTest, RepeatedExecution) {
    const int ITERATIONS = 10;

    for (int iter = 0; iter < ITERATIONS; iter++) {
        // Reset and run
        emu.Reset();

        int frames = emu.RunUntil([this]() {
            return ReadWord(DONE_FLAG_ADDR) == DONE_FLAG_VALUE;
        }, 60);

        ASSERT_GE(frames, 0) << "Iteration " << iter << " failed to complete";

        // Quick sanity check
        EXPECT_EQ(ReadWord(PRIME_COUNT_ADDR), NUM_PRIMES)
            << "Wrong prime count on iteration " << iter;
        EXPECT_EQ(ReadWord(PRIME_RESULTS_ADDR + 198), 541)
            << "100th prime wrong on iteration " << iter;
    }
}

} // namespace
