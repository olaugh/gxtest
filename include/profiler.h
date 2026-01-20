/**
 * profiler.h - 68k CPU cycle profiler for Genesis Plus GX
 *
 * Provides per-function cycle counting using the emulator's native cpu_hook
 * mechanism. No ROM modification required - profiling is done entirely in
 * the emulator by tracking PC values and cycle counts.
 *
 * Usage:
 *   GX::Profiler profiler;
 *   profiler.AddFunction(0x001000, 0x001100, "generate_moves");
 *   profiler.AddFunction(0x001100, 0x001200, "score_move");
 *   // Or load from ELF: profiler.LoadSymbols("game.elf");
 *
 *   profiler.Start();
 *   emu.RunFrames(1000);
 *   profiler.Stop();
 *
 *   profiler.PrintReport(std::cout);
 */

#ifndef GXTEST_PROFILER_H
#define GXTEST_PROFILER_H

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <ostream>

namespace GX {

/**
 * Profiling mode
 */
enum class ProfileMode {
    Simple,     // Fast - just tracks which function PC is in
    CallStack   // Tracks call stack for inclusive cycle counts
};

/**
 * Profiling options
 */
struct ProfileOptions {
    ProfileMode mode = ProfileMode::Simple;
    uint32_t sample_rate = 1;  // 1 = every instruction, N = every Nth instruction
};

/**
 * Statistics for a single function
 */
struct FunctionStats {
    uint64_t call_count = 0;       // Number of times function was entered
    uint64_t cycles_exclusive = 0; // Cycles spent in this function only
    uint64_t cycles_inclusive = 0; // Cycles including callees (CallStack mode only)
};

/**
 * Call stack frame for tracking nested function calls
 */
struct CallFrame {
    uint32_t func_addr;      // Start address of function
    int64_t entry_cycles;    // Cycle count when function was entered
};

/**
 * Function definition from symbol table
 */
struct FunctionDef {
    uint32_t start_addr;
    uint32_t end_addr;
    std::string name;
};

/**
 * 68k CPU cycle profiler
 *
 * Tracks cycles per function using the emulator's cpu_hook callback.
 * Simply attributes cycles to whichever function the PC is currently in.
 * Minimal overhead - just a binary search lookup per instruction.
 */
class Profiler {
public:
    Profiler();
    ~Profiler();

    // Prevent copying (uses global hook)
    Profiler(const Profiler&) = delete;
    Profiler& operator=(const Profiler&) = delete;

    // -------------------------------------------------------------------------
    // Symbol Table Management
    // -------------------------------------------------------------------------

    /**
     * Add a function to the symbol table
     * @param start_addr Start address (inclusive)
     * @param end_addr End address (exclusive)
     * @param name Function name for reporting
     */
    void AddFunction(uint32_t start_addr, uint32_t end_addr, const std::string& name);

    /**
     * Load symbols from an ELF file
     * @param elf_path Path to ELF file with debug symbols
     * @return Number of functions loaded, or -1 on error
     */
    int LoadSymbolsFromELF(const std::string& elf_path);

    /**
     * Load symbols from nm-style text output
     * Format: "address size name" per line (hex address, decimal size)
     * @param path Path to symbol file
     * @return Number of functions loaded, or -1 on error
     */
    int LoadSymbolsFromFile(const std::string& path);

    /**
     * Clear all symbols
     */
    void ClearSymbols();

    /**
     * Get number of loaded symbols
     */
    size_t GetSymbolCount() const { return functions_.size(); }

    // -------------------------------------------------------------------------
    // Profiling Control
    // -------------------------------------------------------------------------

    /**
     * Start profiling - installs the cpu_hook callback
     * @param mode ProfileMode::Simple (fast) or ProfileMode::CallStack (inclusive cycles)
     */
    void Start(ProfileMode mode = ProfileMode::Simple);

    /**
     * Start profiling with options
     * @param options ProfileOptions struct with mode and sample_rate
     */
    void Start(const ProfileOptions& options);

    /**
     * Stop profiling - removes the cpu_hook callback
     */
    void Stop();

    /**
     * Check if profiling is active
     */
    bool IsRunning() const { return running_; }

    /**
     * Reset all statistics (keeps symbols)
     */
    void Reset();

    // -------------------------------------------------------------------------
    // Results
    // -------------------------------------------------------------------------

    /**
     * Get statistics for a specific function by address
     * @return Pointer to stats, or nullptr if not found
     */
    const FunctionStats* GetStats(uint32_t func_addr) const;

    /**
     * Get all function statistics
     * @return Map of function start address to stats
     */
    const std::unordered_map<uint32_t, FunctionStats>& GetAllStats() const { return stats_; }

    /**
     * Get total cycles recorded
     */
    uint64_t GetTotalCycles() const { return total_cycles_; }

    /**
     * Get current sample rate (1 = every instruction)
     */
    uint32_t GetSampleRate() const { return sample_rate_; }

    /**
     * Print a formatted profile report
     * @param out Output stream
     * @param max_functions Maximum functions to show (0 = all)
     */
    void PrintReport(std::ostream& out, size_t max_functions = 0) const;

    // -------------------------------------------------------------------------
    // Internal (called by cpu_hook)
    // -------------------------------------------------------------------------

    /** Called by cpu_hook on each instruction execute */
    void OnExecute(uint32_t pc);

private:
    /** Look up function containing address */
    const FunctionDef* LookupFunction(uint32_t addr) const;

    /** Read 16-bit word from 68k address space */
    uint16_t ReadWord(uint32_t addr) const;

    /** Check if opcode is JSR or BSR */
    bool IsCallOpcode(uint16_t opcode) const;

    /** Check if opcode is RTS or RTR */
    bool IsReturnOpcode(uint16_t opcode) const;

    std::vector<FunctionDef> functions_;  // Sorted by start_addr
    std::unordered_map<uint32_t, FunctionStats> stats_;
    std::vector<CallFrame> call_stack_;   // For CallStack mode

    ProfileMode mode_ = ProfileMode::Simple;
    bool running_ = false;
    uint32_t last_pc_ = 0;
    int64_t last_cycles_ = 0;
    uint64_t total_cycles_ = 0;
    uint32_t sample_rate_ = 1;
    uint32_t sample_counter_ = 0;
};

/** Global profiler instance (needed for cpu_hook callback) */
Profiler* GetActiveProfiler();

} // namespace GX

#endif // GXTEST_PROFILER_H
