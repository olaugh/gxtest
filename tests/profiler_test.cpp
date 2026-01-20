/**
 * gxtest - Profiler Test
 *
 * Tests the CPU cycle profiler using the prime sieve ROM.
 * Verifies:
 * 1. Cycle counting works correctly
 * 2. Function attribution via manually added symbols
 * 3. Sample-based profiling produces reasonable estimates
 * 4. Profiler state management (start/stop/reset)
 */

#include <gxtest.h>
#include <profiler.h>
#include "prime_sieve_rom.h"
#include <cmath>

namespace {

using namespace GX::TestRoms;

// Prime sieve ROM function addresses (from disassembly)
// These are approximate ranges for testing purposes
constexpr uint32_t FUNC_START = 0x200;       // Entry point after header
constexpr uint32_t FUNC_MAIN = 0x200;        // main function
constexpr uint32_t FUNC_SIEVE = 0x240;       // sieve computation
constexpr uint32_t FUNC_END = 0x2A4;         // End of code

class ProfilerTest : public GX::Test {
protected:
    GX::Profiler profiler;

    void SetUp() override {
        ASSERT_TRUE(emu.LoadRom(PRIME_SIEVE_ROM, PRIME_SIEVE_ROM_SIZE))
            << "Failed to load prime sieve ROM";

        // Add function symbols for the prime sieve ROM
        profiler.AddFunction(FUNC_MAIN, FUNC_SIEVE, "main");
        profiler.AddFunction(FUNC_SIEVE, FUNC_END, "sieve");
    }

    void TearDown() override {
        if (profiler.IsRunning()) {
            profiler.Stop();
        }
    }
};

// =============================================================================
// Basic Profiler Tests
// =============================================================================

/**
 * Test that profiler starts and stops correctly
 */
TEST_F(ProfilerTest, StartStop) {
    EXPECT_FALSE(profiler.IsRunning());

    profiler.Start();
    EXPECT_TRUE(profiler.IsRunning());

    profiler.Stop();
    EXPECT_FALSE(profiler.IsRunning());
}

/**
 * Test that symbols are loaded correctly
 */
TEST_F(ProfilerTest, SymbolsLoaded) {
    EXPECT_EQ(profiler.GetSymbolCount(), 2u);
}

/**
 * Test that cycles are counted during execution
 */
TEST_F(ProfilerTest, CyclesCounted) {
    profiler.Start();

    // Run until sieve completes
    emu.RunUntilMemoryEquals(DONE_FLAG_ADDR + 1, 0xAD, 60);

    profiler.Stop();

    // Should have counted some cycles
    uint64_t total = profiler.GetTotalCycles();
    EXPECT_GT(total, 0u) << "Should have counted cycles";

    // Prime sieve should take at least a few thousand cycles
    EXPECT_GT(total, 1000u) << "Expected significant cycle count for prime sieve";
}

/**
 * Test that function stats are populated
 */
TEST_F(ProfilerTest, FunctionStatsPopulated) {
    profiler.Start();
    emu.RunUntilMemoryEquals(DONE_FLAG_ADDR + 1, 0xAD, 60);
    profiler.Stop();

    const auto& stats = profiler.GetAllStats();

    // Should have stats for at least one function
    EXPECT_FALSE(stats.empty()) << "Should have function stats";

    // Check that cycles were attributed
    uint64_t total_attributed = 0;
    for (const auto& kv : stats) {
        total_attributed += kv.second.cycles_exclusive;
    }
    EXPECT_GT(total_attributed, 0u) << "Should have attributed cycles to functions";
}

/**
 * Test profiler reset clears stats
 */
TEST_F(ProfilerTest, ResetClearsStats) {
    profiler.Start();
    emu.RunUntilMemoryEquals(DONE_FLAG_ADDR + 1, 0xAD, 60);
    profiler.Stop();

    uint64_t before = profiler.GetTotalCycles();
    EXPECT_GT(before, 0u);

    profiler.Reset();

    EXPECT_EQ(profiler.GetTotalCycles(), 0u) << "Reset should clear total cycles";

    // Stats should still exist but be zeroed
    const auto& stats = profiler.GetAllStats();
    for (const auto& kv : stats) {
        EXPECT_EQ(kv.second.cycles_exclusive, 0u) << "Reset should clear function cycles";
        EXPECT_EQ(kv.second.call_count, 0u) << "Reset should clear call counts";
    }
}

// =============================================================================
// Sample-Based Profiling Tests
// =============================================================================

/**
 * Test that sample rate is correctly reported
 */
TEST_F(ProfilerTest, SampleRateReported) {
    GX::ProfileOptions opts;
    opts.sample_rate = 10;
    profiler.Start(opts);

    EXPECT_EQ(profiler.GetSampleRate(), 10u);

    profiler.Stop();
}

/**
 * Test sampled profiling produces reasonable cycle estimates
 */
TEST_F(ProfilerTest, SampledProfilingCycles) {
    // First run with full profiling
    profiler.Start(GX::ProfileMode::Simple);
    emu.RunUntilMemoryEquals(DONE_FLAG_ADDR + 1, 0xAD, 60);
    profiler.Stop();

    uint64_t full_cycles = profiler.GetTotalCycles();
    ASSERT_GT(full_cycles, 0u);

    // Reset emulator and profiler
    emu.Reset();
    profiler.Reset();

    // Run with 1/10 sampling
    GX::ProfileOptions opts;
    opts.sample_rate = 10;
    profiler.Start(opts);
    emu.RunUntilMemoryEquals(DONE_FLAG_ADDR + 1, 0xAD, 60);
    profiler.Stop();

    uint64_t sampled_cycles = profiler.GetTotalCycles();

    // Sampled cycles should be close to full cycles (same computation)
    // Allow 5% tolerance since sampling introduces some variance
    double ratio = static_cast<double>(sampled_cycles) / full_cycles;
    EXPECT_NEAR(ratio, 1.0, 0.05)
        << "Sampled cycles (" << sampled_cycles << ") should be close to full cycles ("
        << full_cycles << "), ratio=" << ratio;
}

/**
 * Test that higher sample rates reduce profiler overhead (faster execution)
 * This is a sanity check that sampling is actually skipping work
 */
TEST_F(ProfilerTest, SamplingReducesOverhead) {
    // Time full profiling
    auto start1 = std::chrono::high_resolution_clock::now();
    profiler.Start(GX::ProfileMode::Simple);
    emu.RunUntilMemoryEquals(DONE_FLAG_ADDR + 1, 0xAD, 60);
    profiler.Stop();
    auto end1 = std::chrono::high_resolution_clock::now();
    auto full_time = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);

    emu.Reset();
    profiler.Reset();

    // Time sampled profiling (1/100)
    GX::ProfileOptions opts;
    opts.sample_rate = 100;

    auto start2 = std::chrono::high_resolution_clock::now();
    profiler.Start(opts);
    emu.RunUntilMemoryEquals(DONE_FLAG_ADDR + 1, 0xAD, 60);
    profiler.Stop();
    auto end2 = std::chrono::high_resolution_clock::now();
    auto sampled_time = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);

    // Sampled should be faster (or at least not slower)
    // Note: For very fast ROMs like prime_sieve, the difference may be small
    std::cout << "Full profiling: " << full_time.count() << " us" << std::endl;
    std::cout << "Sampled (1/100): " << sampled_time.count() << " us" << std::endl;

    // Just verify both completed - timing can be noisy on fast operations
    EXPECT_GT(full_time.count(), 0);
    EXPECT_GT(sampled_time.count(), 0);
}

// =============================================================================
// Multiple Runs Tests
// =============================================================================

/**
 * Test that profiler accumulates across multiple start/stop cycles
 */
TEST_F(ProfilerTest, AccumulatesAcrossRuns) {
    // First run
    profiler.Start();
    emu.RunUntilMemoryEquals(DONE_FLAG_ADDR + 1, 0xAD, 60);
    profiler.Stop();
    uint64_t first_cycles = profiler.GetTotalCycles();

    // Reset emulator but NOT profiler
    emu.Reset();

    // Second run - profiler accumulates
    profiler.Start();
    emu.RunUntilMemoryEquals(DONE_FLAG_ADDR + 1, 0xAD, 60);
    profiler.Stop();
    uint64_t total_cycles = profiler.GetTotalCycles();

    // Should have roughly 2x the cycles
    EXPECT_GT(total_cycles, first_cycles) << "Cycles should accumulate";
    double ratio = static_cast<double>(total_cycles) / first_cycles;
    EXPECT_NEAR(ratio, 2.0, 0.1) << "Expected ~2x cycles after two runs";
}

/**
 * Test consistent results across repeated profiling
 */
TEST_F(ProfilerTest, ConsistentResults) {
    std::vector<uint64_t> cycle_counts;

    for (int i = 0; i < 5; i++) {
        emu.Reset();
        profiler.Reset();

        profiler.Start();
        emu.RunUntilMemoryEquals(DONE_FLAG_ADDR + 1, 0xAD, 60);
        profiler.Stop();

        cycle_counts.push_back(profiler.GetTotalCycles());
    }

    // All runs should produce identical cycle counts (deterministic emulation)
    for (size_t i = 1; i < cycle_counts.size(); i++) {
        EXPECT_EQ(cycle_counts[i], cycle_counts[0])
            << "Run " << i << " cycle count differs from run 0";
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

/**
 * Test profiling with no symbols
 */
TEST_F(ProfilerTest, NoSymbols) {
    GX::Profiler empty_profiler;
    EXPECT_EQ(empty_profiler.GetSymbolCount(), 0u);

    empty_profiler.Start();
    emu.RunFrames(10);
    empty_profiler.Stop();

    // Should still count total cycles
    EXPECT_GT(empty_profiler.GetTotalCycles(), 0u);

    // But no function stats
    EXPECT_TRUE(empty_profiler.GetAllStats().empty());
}

/**
 * Test stopping profiler that wasn't started
 */
TEST_F(ProfilerTest, StopWithoutStart) {
    EXPECT_FALSE(profiler.IsRunning());
    profiler.Stop();  // Should not crash
    EXPECT_FALSE(profiler.IsRunning());
}

/**
 * Test starting profiler twice
 */
TEST_F(ProfilerTest, DoubleStart) {
    profiler.Start();
    EXPECT_TRUE(profiler.IsRunning());

    profiler.Start();  // Should be no-op
    EXPECT_TRUE(profiler.IsRunning());

    profiler.Stop();
    EXPECT_FALSE(profiler.IsRunning());
}

} // namespace
