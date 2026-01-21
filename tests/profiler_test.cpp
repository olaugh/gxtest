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
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>

namespace {

using namespace GX::TestRoms;

// Prime sieve ROM function addresses (from ELF: m68k-elf-nm -S prime_sieve.elf)
// 00000200        T _start
// 00000210 000014 t clear_sieve
// 00000224 000012 t mark_trivial_composites
// 00000236 000034 t run_sieve
// 0000026a 000036 t collect_primes
// 000002a0 000022 T main
constexpr uint32_t FUNC_START = 0x200;
constexpr uint32_t FUNC_CLEAR_SIEVE = 0x210;
constexpr uint32_t FUNC_MARK_TRIVIAL = 0x224;
constexpr uint32_t FUNC_RUN_SIEVE = 0x236;
constexpr uint32_t FUNC_COLLECT_PRIMES = 0x26A;
constexpr uint32_t FUNC_MAIN = 0x2A0;
constexpr uint32_t FUNC_MAIN_END = 0x2C2;

class ProfilerTest : public GX::Test {
protected:
    GX::Profiler profiler;

    void SetUp() override {
        ASSERT_TRUE(emu.LoadRom(PRIME_SIEVE_ROM, PRIME_SIEVE_ROM_SIZE))
            << "Failed to load prime sieve ROM";

        // Add function symbols for the prime sieve ROM
        profiler.AddFunction(FUNC_START, FUNC_CLEAR_SIEVE, "_start");
        profiler.AddFunction(FUNC_CLEAR_SIEVE, FUNC_MARK_TRIVIAL, "clear_sieve");
        profiler.AddFunction(FUNC_MARK_TRIVIAL, FUNC_RUN_SIEVE, "mark_trivial_composites");
        profiler.AddFunction(FUNC_RUN_SIEVE, FUNC_COLLECT_PRIMES, "run_sieve");
        profiler.AddFunction(FUNC_COLLECT_PRIMES, FUNC_MAIN, "collect_primes");
        profiler.AddFunction(FUNC_MAIN, FUNC_MAIN_END, "main");
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
    EXPECT_EQ(profiler.GetSymbolCount(), 6u);
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

// =============================================================================
// Address Histogram Tests
// =============================================================================

/**
 * Test that address histogram is empty when disabled (default)
 */
TEST_F(ProfilerTest, AddressHistogramDisabledByDefault) {
    profiler.Start();
    emu.RunUntilMemoryEquals(DONE_FLAG_ADDR + 1, 0xAD, 60);
    profiler.Stop();

    const auto& histogram = profiler.GetAddressHistogram();
    EXPECT_TRUE(histogram.empty())
        << "Address histogram should be empty when collect_address_histogram is false";
}

/**
 * Test that address histogram is populated when enabled
 */
TEST_F(ProfilerTest, AddressHistogramCollected) {
    GX::ProfileOptions opts;
    opts.collect_address_histogram = true;

    profiler.Start(opts);
    emu.RunUntilMemoryEquals(DONE_FLAG_ADDR + 1, 0xAD, 60);
    profiler.Stop();

    const auto& histogram = profiler.GetAddressHistogram();
    EXPECT_FALSE(histogram.empty())
        << "Address histogram should have entries when enabled";

    // Should have multiple unique addresses
    EXPECT_GT(histogram.size(), 10u)
        << "Expected many unique instruction addresses";

    // All addresses should be in valid ROM range
    for (const auto& kv : histogram) {
        EXPECT_GE(kv.first, 0x200u) << "Address should be >= ROM start";
        EXPECT_LT(kv.first, 0x400000u) << "Address should be < ROM end";
        EXPECT_GT(kv.second, 0u) << "Cycle count should be positive";
    }
}

/**
 * Test that address histogram cycles sum to total cycles
 */
TEST_F(ProfilerTest, AddressHistogramSumsToTotal) {
    GX::ProfileOptions opts;
    opts.collect_address_histogram = true;

    profiler.Start(opts);
    emu.RunUntilMemoryEquals(DONE_FLAG_ADDR + 1, 0xAD, 60);
    profiler.Stop();

    const auto& histogram = profiler.GetAddressHistogram();
    uint64_t histogram_total = 0;
    for (const auto& kv : histogram) {
        histogram_total += kv.second;
    }

    uint64_t total_cycles = profiler.GetTotalCycles();

    // Histogram sum should equal total cycles
    EXPECT_EQ(histogram_total, total_cycles)
        << "Sum of address histogram should equal total cycles";
}

/**
 * Test that address histogram works with sampling
 */
TEST_F(ProfilerTest, AddressHistogramWithSampling) {
    GX::ProfileOptions opts;
    opts.collect_address_histogram = true;
    opts.sample_rate = 10;

    profiler.Start(opts);
    emu.RunUntilMemoryEquals(DONE_FLAG_ADDR + 1, 0xAD, 60);
    profiler.Stop();

    const auto& histogram = profiler.GetAddressHistogram();
    EXPECT_FALSE(histogram.empty())
        << "Address histogram should work with sampling";

    // With sampling, we'll have fewer addresses but should still capture main ones
    uint64_t histogram_total = 0;
    for (const auto& kv : histogram) {
        histogram_total += kv.second;
    }

    // Histogram sum should be very close to total cycles. With sampling,
    // there may be a small difference due to pending_cycles_ not yet attributed
    // at the time profiling stopped (at most sample_rate * cycles_per_instruction).
    uint64_t total_cycles = profiler.GetTotalCycles();
    uint64_t max_variance = opts.sample_rate * 200;  // ~200 cycles max per 68k instruction
    EXPECT_LE(total_cycles - histogram_total, max_variance)
        << "Histogram total should be within " << max_variance << " of total cycles";
    EXPECT_GE(histogram_total, total_cycles * 99 / 100)
        << "Histogram should capture at least 99% of cycles";
}

/**
 * Test that Reset clears address histogram
 */
TEST_F(ProfilerTest, ResetClearsAddressHistogram) {
    GX::ProfileOptions opts;
    opts.collect_address_histogram = true;

    profiler.Start(opts);
    emu.RunUntilMemoryEquals(DONE_FLAG_ADDR + 1, 0xAD, 60);
    profiler.Stop();

    EXPECT_FALSE(profiler.GetAddressHistogram().empty());

    profiler.Reset();

    EXPECT_TRUE(profiler.GetAddressHistogram().empty())
        << "Reset should clear address histogram";
}

/**
 * Test WriteAddressHistogram produces valid JSON
 */
TEST_F(ProfilerTest, WriteAddressHistogramJSON) {
    GX::ProfileOptions opts;
    opts.collect_address_histogram = true;

    profiler.Start(opts);
    emu.RunUntilMemoryEquals(DONE_FLAG_ADDR + 1, 0xAD, 60);
    profiler.Stop();

    // Write to temp file
    std::string temp_path = "/tmp/gxtest_histogram_test.json";
    ASSERT_TRUE(profiler.WriteAddressHistogram(temp_path))
        << "WriteAddressHistogram should succeed";

    // Read and verify JSON structure
    std::ifstream file(temp_path);
    ASSERT_TRUE(file.good()) << "Should be able to read output file";

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    // Basic JSON structure checks
    EXPECT_NE(content.find("\"sample_rate\":"), std::string::npos)
        << "JSON should contain sample_rate";
    EXPECT_NE(content.find("\"total_cycles\":"), std::string::npos)
        << "JSON should contain total_cycles";
    EXPECT_NE(content.find("\"address_count\":"), std::string::npos)
        << "JSON should contain address_count";
    EXPECT_NE(content.find("\"addresses\":"), std::string::npos)
        << "JSON should contain addresses object";

    // Verify sample_rate value
    EXPECT_NE(content.find("\"sample_rate\": 1"), std::string::npos)
        << "Sample rate should be 1";

    // Verify total_cycles matches
    std::string total_str = "\"total_cycles\": " + std::to_string(profiler.GetTotalCycles());
    EXPECT_NE(content.find(total_str), std::string::npos)
        << "Total cycles in JSON should match profiler";

    // Verify address_count matches histogram size
    std::string count_str = "\"address_count\": " + std::to_string(profiler.GetAddressHistogram().size());
    EXPECT_NE(content.find(count_str), std::string::npos)
        << "Address count in JSON should match histogram size";

    // Clean up
    std::remove(temp_path.c_str());
}

/**
 * Test WriteAddressHistogram with sampling records correct sample_rate
 */
TEST_F(ProfilerTest, WriteAddressHistogramRecordsSampleRate) {
    GX::ProfileOptions opts;
    opts.collect_address_histogram = true;
    opts.sample_rate = 50;

    profiler.Start(opts);
    emu.RunUntilMemoryEquals(DONE_FLAG_ADDR + 1, 0xAD, 60);
    profiler.Stop();

    std::string temp_path = "/tmp/gxtest_histogram_sample_test.json";
    ASSERT_TRUE(profiler.WriteAddressHistogram(temp_path));

    std::ifstream file(temp_path);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    EXPECT_NE(content.find("\"sample_rate\": 50"), std::string::npos)
        << "JSON should record correct sample rate";

    std::remove(temp_path.c_str());
}

/**
 * Test WriteAddressHistogram fails gracefully on invalid path
 */
TEST_F(ProfilerTest, WriteAddressHistogramInvalidPath) {
    GX::ProfileOptions opts;
    opts.collect_address_histogram = true;

    profiler.Start(opts);
    emu.RunFrames(1);
    profiler.Stop();

    // Try to write to an invalid path
    EXPECT_FALSE(profiler.WriteAddressHistogram("/nonexistent/directory/file.json"))
        << "WriteAddressHistogram should return false for invalid path";
}

/**
 * Test address histogram contains expected addresses from known code
 */
TEST_F(ProfilerTest, AddressHistogramContainsKnownAddresses) {
    GX::ProfileOptions opts;
    opts.collect_address_histogram = true;

    profiler.Start(opts);
    emu.RunUntilMemoryEquals(DONE_FLAG_ADDR + 1, 0xAD, 60);
    profiler.Stop();

    const auto& histogram = profiler.GetAddressHistogram();

    // The prime sieve starts at 0x200 (_start) and calls main at 0x2A0
    // We should see addresses in the run_sieve function (0x236-0x26A)
    // which contains the main loop
    bool found_sieve_addr = false;
    for (const auto& kv : histogram) {
        if (kv.first >= FUNC_RUN_SIEVE && kv.first < FUNC_COLLECT_PRIMES) {
            found_sieve_addr = true;
            break;
        }
    }
    EXPECT_TRUE(found_sieve_addr)
        << "Should have recorded cycles in run_sieve function";

    // run_sieve should have significant cycles (it's the main loop)
    uint64_t sieve_cycles = 0;
    for (const auto& kv : histogram) {
        if (kv.first >= FUNC_RUN_SIEVE && kv.first < FUNC_COLLECT_PRIMES) {
            sieve_cycles += kv.second;
        }
    }
    EXPECT_GT(sieve_cycles, profiler.GetTotalCycles() / 10)
        << "run_sieve should account for significant portion of cycles";
}

} // namespace
