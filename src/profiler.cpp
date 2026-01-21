/**
 * profiler.cpp - 68k CPU cycle profiler implementation
 */

#include "profiler.h"
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cstring>

// Genesis Plus GX headers (C linkage)
extern "C" {
#include "shared.h"
#include "cpuhook.h"
}

namespace GX {

// Global profiler for cpu_hook callback
static Profiler* g_active_profiler = nullptr;

// cpu_hook callback - called before each 68k instruction
static void ProfilerHook(hook_type_t type, int /*width*/, unsigned int address, unsigned int /*value*/) {
    if (type == HOOK_M68K_E && g_active_profiler) {
        g_active_profiler->OnExecute(address);
    }
}

Profiler* GetActiveProfiler() {
    return g_active_profiler;
}

// ---------------------------------------------------------------------------
// Profiler Implementation
// ---------------------------------------------------------------------------

Profiler::Profiler() {}

Profiler::~Profiler() {
    if (running_) {
        Stop();
    }
}

void Profiler::AddFunction(uint32_t start_addr, uint32_t end_addr, const std::string& name) {
    // Validate function range
    if (end_addr <= start_addr) {
        return;  // Invalid range, ignore
    }

    FunctionDef func = {start_addr, end_addr, name};

    // Insert in sorted order by start_addr
    auto it = std::lower_bound(functions_.begin(), functions_.end(), func,
        [](const FunctionDef& a, const FunctionDef& b) {
            return a.start_addr < b.start_addr;
        });
    functions_.insert(it, func);

    // Initialize stats for this function
    stats_[start_addr] = FunctionStats();
}

int Profiler::LoadSymbolsFromELF(const std::string& elf_path) {
    // Use nm to extract symbols
    // Format: "address type name"
    // Shell-quote the path to prevent command injection
    std::string quoted_path = "'";
    for (char c : elf_path) {
        if (c == '\'') {
            quoted_path += "'\\''";  // End quote, escaped quote, start quote
        } else {
            quoted_path += c;
        }
    }
    quoted_path += "'";
    std::string cmd = "nm -S --defined-only " + quoted_path + " 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return -1;
    }

    int count = 0;
    char line[512];
    while (fgets(line, sizeof(line), pipe)) {
        uint32_t addr, size;
        char type;
        char name[256];

        // Parse nm -S output: "hex_address hex_size type name" (both in hex, with size) or "hex_address type name" (without size)
        if (sscanf(line, "%x %x %c %255s", &addr, &size, &type, name) == 4) {
            // Has size - use it
            if (type == 'T' || type == 't') {  // Text (code) symbols only
                // Guard against overflow
                uint32_t end_addr = (size <= UINT32_MAX - addr) ? addr + size : UINT32_MAX;
                AddFunction(addr, end_addr, name);
                count++;
            }
        } else if (sscanf(line, "%x %c %255s", &addr, &type, name) == 3) {
            // No size - estimate from next symbol (done after loading all)
            if (type == 'T' || type == 't') {
                // Guard against overflow (0x100 = 256 byte default)
                uint32_t end_addr = (addr <= UINT32_MAX - 0x100) ? addr + 0x100 : UINT32_MAX;
                AddFunction(addr, end_addr, name);
                count++;
            }
        }
    }

    pclose(pipe);

    // Fix up end addresses based on next function start
    for (size_t i = 0; i + 1 < functions_.size(); i++) {
        if (functions_[i].end_addr > functions_[i + 1].start_addr) {
            functions_[i].end_addr = functions_[i + 1].start_addr;
        }
    }

    return count;
}

int Profiler::LoadSymbolsFromFile(const std::string& path) {
    // Load symbols from a simple text file format:
    //   <hex_address> <decimal_size> <name>
    // Example:
    //   00000200 16 _start
    //   00000210 94 main
    //
    // Parsing note:
    //   - This function parses the address as hex (%x) and the size as decimal (%u).
    //   - This intentionally differs from LoadSymbolsFromELF (nm output), which uses
    //     hex for both address and size (%x for each).
    std::ifstream file(path);
    if (!file) {
        return -1;
    }

    int count = 0;
    std::string line;
    while (std::getline(file, line)) {
        uint32_t addr, size;
        char name[256];

        if (sscanf(line.c_str(), "%x %u %255s", &addr, &size, name) == 3) {
            // Guard against overflow
            uint32_t end_addr = (size <= UINT32_MAX - addr) ? addr + size : UINT32_MAX;
            AddFunction(addr, end_addr, name);
            count++;
        }
    }

    return count;
}

void Profiler::ClearSymbols() {
    functions_.clear();
    stats_.clear();
}

void Profiler::Start(ProfileMode mode) {
    ProfileOptions opts;
    opts.mode = mode;
    opts.sample_rate = 1;
    Start(opts);
}

void Profiler::Start(const ProfileOptions& options) {
    if (running_) return;

    mode_ = options.mode;
    sample_rate_ = options.sample_rate > 0 ? options.sample_rate : 1;
    collect_address_histogram_ = options.collect_address_histogram;
    sample_counter_ = 0;
    pending_cycles_ = 0;
    g_active_profiler = this;
    set_cpu_hook(ProfilerHook);
    running_ = true;
    last_pc_ = 0;
    last_cycles_ = m68k.cycles;
    call_stack_.clear();
}

void Profiler::Stop() {
    if (!running_) return;

    set_cpu_hook(nullptr);
    g_active_profiler = nullptr;
    running_ = false;
}

void Profiler::Reset() {
    for (auto& kv : stats_) {
        kv.second = FunctionStats();
    }
    address_cycles_.clear();
    call_stack_.clear();
    total_cycles_ = 0;
    pending_cycles_ = 0;
    last_pc_ = 0;
    if (running_) {
        last_cycles_ = m68k.cycles;
    }
}

const FunctionStats* Profiler::GetStats(uint32_t func_addr) const {
    auto it = stats_.find(func_addr);
    return it != stats_.end() ? &it->second : nullptr;
}

const FunctionDef* Profiler::LookupFunction(uint32_t addr) const {
    if (functions_.empty()) return nullptr;

    // Binary search for function containing addr
    auto it = std::upper_bound(functions_.begin(), functions_.end(), addr,
        [](uint32_t a, const FunctionDef& f) {
            return a < f.start_addr;
        });

    if (it == functions_.begin()) return nullptr;
    --it;

    if (addr >= it->start_addr && addr < it->end_addr) {
        return &(*it);
    }
    return nullptr;
}

uint16_t Profiler::ReadWord(uint32_t addr) const {
    // Read from ROM (cart.rom is byteswapped on little-endian)
    if (addr < 0x400000 && addr + 1 < cart.romsize) {
#ifdef LSB_FIRST
        return (cart.rom[addr ^ 1] << 8) | cart.rom[(addr + 1) ^ 1];
#else
        return (cart.rom[addr] << 8) | cart.rom[addr + 1];
#endif
    }
    return 0;
}

void Profiler::OnExecute(uint32_t pc) {
    // Get cycles since last instruction
    int64_t current_cycles = m68k.cycles;
    int64_t delta = current_cycles - last_cycles_;
    last_cycles_ = current_cycles;

    if (delta <= 0) return;  // First call or cycle counter wrapped

    total_cycles_ += delta;

    // Sampling: only do expensive work every Nth instruction
    if (sample_rate_ > 1) {
        pending_cycles_ += delta;
        sample_counter_++;
        if (sample_counter_ < sample_rate_) {
            // Update last_pc_ even on skipped samples so CallStack mode
            // can track call/return instructions correctly
            last_pc_ = pc;
            return;
        }
        sample_counter_ = 0;
        // Use accumulated cycles since last sample (more accurate than scaling)
        delta = pending_cycles_;
        pending_cycles_ = 0;
    }

    // Collect per-address histogram if enabled
    if (collect_address_histogram_) {
        address_cycles_[pc] += delta;
    }

    // Attribute cycles to current function
    const FunctionDef* func = LookupFunction(pc);
    if (func) {
        auto& s = stats_[func->start_addr];
        s.cycles_exclusive += delta;

        // Count function entry (PC moved into this function from outside)
        // Note: With sampling, this undercounts entries that happen between samples
        if (last_pc_ != 0) {
            const FunctionDef* last_func = LookupFunction(last_pc_);
            if (last_func != func) {
                s.call_count++;
            }
        }
    }

    // CallStack mode: track JSR/BSR/RTS for inclusive cycles
    // Note: With sampling enabled, we only check every Nth instruction for
    // call/return opcodes, so inclusive timing will be less accurate.
    if (mode_ == ProfileMode::CallStack && last_pc_ != 0) {
        uint16_t opcode = ReadWord(last_pc_);

        if (IsCallOpcode(opcode)) {
            // Entering a new function - push frame
            // Limit stack depth to prevent unbounded growth from unbalanced calls
            // (e.g., indirect jumps, exceptions, or non-standard control flow)
            constexpr size_t MAX_CALL_STACK_DEPTH = 256;
            if (func && call_stack_.size() < MAX_CALL_STACK_DEPTH) {
                call_stack_.push_back({func->start_addr, current_cycles});
            }
        } else if (IsReturnOpcode(opcode) && !call_stack_.empty()) {
            // Returning from function - pop frame and accumulate inclusive time
            CallFrame frame = call_stack_.back();
            call_stack_.pop_back();

            int64_t inclusive = current_cycles - frame.entry_cycles;
            if (inclusive > 0) {
                stats_[frame.func_addr].cycles_inclusive += inclusive;
            }
        }
    }

    last_pc_ = pc;
}

void Profiler::PrintReport(std::ostream& out, size_t max_functions) const {
    // Build sorted list by cycles (descending)
    struct FuncReport {
        std::string name;
        uint32_t addr;
        uint64_t cycles_excl;
        uint64_t cycles_incl;
        uint64_t calls;
    };

    std::vector<FuncReport> report;
    for (const auto& func : functions_) {
        auto it = stats_.find(func.start_addr);
        if (it != stats_.end() && it->second.cycles_exclusive > 0) {
            report.push_back({
                func.name,
                func.start_addr,
                it->second.cycles_exclusive,
                it->second.cycles_inclusive,
                it->second.call_count
            });
        }
    }

    std::sort(report.begin(), report.end(),
        [](const FuncReport& a, const FuncReport& b) {
            return a.cycles_excl > b.cycles_excl;
        });

    if (max_functions > 0 && report.size() > max_functions) {
        report.resize(max_functions);
    }

    // Print header
    bool show_inclusive = (mode_ == ProfileMode::CallStack);
    out << "\n";
    if (sample_rate_ > 1) {
        out << "Sample rate: 1/" << sample_rate_ << " (estimated cycles)\n";
    }
    out << std::setw(30) << std::left << "Function"
        << std::setw(12) << std::right << "Cycles";
    if (show_inclusive) {
        out << std::setw(12) << "Inclusive";
    }
    out << std::setw(10) << "Calls"
        << std::setw(8) << "%"
        << std::setw(10) << "Cyc/Call"
        << "\n";
    out << std::string(show_inclusive ? 82 : 70, '-') << "\n";

    // Print functions
    for (const auto& r : report) {
        double pct = total_cycles_ > 0 ? 100.0 * r.cycles_excl / total_cycles_ : 0.0;
        uint64_t per_call = r.calls > 0 ? r.cycles_excl / r.calls : 0;

        out << std::setw(30) << std::left << r.name
            << std::setw(12) << std::right << r.cycles_excl;
        if (show_inclusive) {
            out << std::setw(12) << r.cycles_incl;
        }
        out << std::setw(10) << r.calls
            << std::setw(7) << std::fixed << std::setprecision(2) << pct << "%"
            << std::setw(10) << per_call
            << "\n";
    }

    out << std::string(show_inclusive ? 82 : 70, '-') << "\n";
    out << std::setw(30) << std::left << "Total"
        << std::setw(12) << std::right << total_cycles_
        << "\n";
}

bool Profiler::WriteAddressHistogram(const std::string& path) const {
    std::ofstream out(path);
    if (!out) {
        return false;
    }

    // Write JSON format for use with disassembly viewer
    out << "{\n";
    out << "  \"sample_rate\": " << sample_rate_ << ",\n";
    out << "  \"total_cycles\": " << total_cycles_ << ",\n";
    out << "  \"address_count\": " << address_cycles_.size() << ",\n";
    out << "  \"addresses\": {\n";

    // Sort addresses for deterministic output
    std::vector<std::pair<uint32_t, uint64_t>> sorted_addrs(
        address_cycles_.begin(), address_cycles_.end());
    std::sort(sorted_addrs.begin(), sorted_addrs.end());

    bool first = true;
    for (const auto& kv : sorted_addrs) {
        if (!first) out << ",\n";
        first = false;
        out << "    \"" << std::hex << std::setfill('0') << std::setw(8)
            << kv.first << "\": " << std::dec << kv.second;
    }

    out << "\n  }\n";
    out << "}\n";

    return out.good();
}

bool Profiler::IsCallOpcode(uint16_t opcode) const {
    // JSR: 0100 1110 10xx xxxx (0x4E80-0x4EBF)
    // BSR: 0110 0001 xxxx xxxx (0x6100-0x61FF)
    return ((opcode & 0xFFC0) == 0x4E80) || ((opcode & 0xFF00) == 0x6100);
}

bool Profiler::IsReturnOpcode(uint16_t opcode) const {
    // RTS: 0x4E75
    // RTR: 0x4E77
    return opcode == 0x4E75 || opcode == 0x4E77;
}

} // namespace GX
