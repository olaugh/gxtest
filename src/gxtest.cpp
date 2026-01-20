/**
 * gxtest - Implementation of the Genesis Plus GX test harness
 */

#include "gxtest.h"
#include "osd.h"

#include <cstring>
#include <fstream>
#include <stdexcept>

// Genesis Plus GX core headers (C linkage)
extern "C" {
#include "shared.h"
#include "genesis.h"
#include "state.h"
#include "m68k.h"
#include "loadrom.h"

// Global variables required by the core (must have C linkage)
t_config config;
extern t_bitmap bitmap;  // Defined in core's system.c

// BIOS path strings required by the core
char GG_ROM[256] = "";
char AR_ROM[256] = "";
char SK_ROM[256] = "";
char SK_UPMEM[256] = "";
char GG_BIOS[256] = "";
char MD_BIOS[256] = "";
char CD_BIOS_EU[256] = "";
char CD_BIOS_US[256] = "";
char CD_BIOS_JP[256] = "";
char MS_BIOS_US[256] = "";
char MS_BIOS_EU[256] = "";
char MS_BIOS_JP[256] = "";
}

namespace GX {

// Frame buffer for headless rendering (required even if not displayed)
static uint16_t frame_buffer[720 * 576];

/**
 * Initialize default configuration for headless operation
 */
static void InitDefaultConfig() {
    memset(&config, 0, sizeof(config));

    // Version string
    strncpy(config.version, "GXTEST", sizeof(config.version) - 1);

    // Audio settings (minimal, we don't actually output audio)
    config.hq_fm = 1;
    config.hq_psg = 0;
    config.filter = 0;
    config.psg_preamp = 150;
    config.fm_preamp = 100;
    config.cdda_volume = 100;
    config.pcm_volume = 100;
    config.lp_range = 0x9999;
    config.low_freq = 880;
    config.high_freq = 5000;
    config.lg = 100;
    config.mg = 100;
    config.hg = 100;
    config.mono = 0;
    config.ym2612 = YM2612_DISCRETE;
    config.ym2413 = 2;

    // System settings
    config.system = 0;          // Auto-detect
    config.region_detect = 0;   // Auto-detect
    config.vdp_mode = 0;        // Auto-detect
    config.master_clock = 0;    // Auto-detect
    config.force_dtack = 0;
    config.addr_error = 1;
    config.bios = 0;
    config.lock_on = 0;
    config.add_on = 0;
    config.cd_latency = 1;

    // Video settings
    config.overscan = 0;
    config.aspect_ratio = 0;
    config.render = 0;
    config.ntsc = 0;
    config.lcd = 0;
    config.gg_extra = 0;
    config.left_border = 0;

    // Performance settings
    config.overclock = 0;
    config.no_sprite_limit = 1;  // Remove sprite limit for accuracy
    config.enhanced_vscroll = 0;
    config.enhanced_vscroll_limit = 8;

    // Input configuration (2 3-button pads by default)
    for (int i = 0; i < MAX_INPUTS; i++) {
        config.input[i].device = (i < 2) ? 1 : -1;  // DEVICE_PAD3B
        config.input[i].port = i < 2 ? i : 0xFF;
        config.input[i].padtype = 0;
    }
}

/**
 * Initialize bitmap structure for headless rendering
 */
static void InitBitmap() {
    memset(&bitmap, 0, sizeof(bitmap));
    bitmap.width = 720;
    bitmap.height = 576;
    bitmap.pitch = 720 * 2;  // 16-bit color
    bitmap.data = reinterpret_cast<uint8*>(frame_buffer);
    bitmap.viewport.x = 0;
    bitmap.viewport.y = 0;
    bitmap.viewport.w = 320;
    bitmap.viewport.h = 224;
}

// ---------------------------------------------------------------------------
// Emulator::Impl - Private implementation
// ---------------------------------------------------------------------------

class Emulator::Impl {
public:
    bool rom_loaded = false;
    uint64_t frame_count = 0;
    Input inputs[2];
    std::vector<uint8_t> rom_data;

    Impl() {
        InitDefaultConfig();
        InitBitmap();
    }

    ~Impl() {
        if (rom_loaded) {
            audio_shutdown();
        }
    }

    bool LoadRomData(const uint8_t* data, size_t size) {
        if (size == 0 || size > static_cast<size_t>(MAXROMSIZE)) {
            fprintf(stderr, "ROM size invalid: %zu (max %d)\n", size, MAXROMSIZE);
            return false;
        }

        // Store ROM data
        rom_data.assign(data, data + size);

        // Initialize audio (required by core even in headless mode)
        // Note: audio_init returns 0 on success, negative on failure
        if (audio_init(48000, 60.0) < 0) {
            fprintf(stderr, "audio_init failed\n");
            return false;
        }

        // Copy to cart ROM buffer
        memset(cart.rom, 0, sizeof(cart.rom));
        memcpy(cart.rom, rom_data.data(), rom_data.size());

        // Set ROM size
        cart.romsize = static_cast<int>(size);

        // Set system hardware to Mega Drive (Genesis)
        system_hw = SYSTEM_MD;

        // Byteswap ROM for little-endian systems
        // This is required because Genesis Plus GX's memory map assumes byteswapped ROM
#ifdef LSB_FIRST
        for (size_t i = 0; i < size; i += 2) {
            uint8_t temp = cart.rom[i];
            cart.rom[i] = cart.rom[i + 1];
            cart.rom[i + 1] = temp;
        }
#endif

        // Extract ROM header info
        getrominfo(reinterpret_cast<char*>(cart.rom));

        // Set console region from ROM header
        get_region(reinterpret_cast<char*>(cart.rom));

        // Save detected system type
        romtype = system_hw;

        // Initialize system
        system_init();
        system_reset();

        rom_loaded = true;
        frame_count = 0;

        return true;
    }

    void RunFrame() {
        if (!rom_loaded) return;

        // Update input state before frame
        UpdateInputState();

        // Run one frame
        if (system_hw == SYSTEM_MCD) {
            system_frame_scd(0);
        } else if ((system_hw & SYSTEM_PBC) == SYSTEM_MD) {
            system_frame_gen(0);
        } else {
            system_frame_sms(0);
        }

        frame_count++;
    }

    void UpdateInputState() {
        // Clear input state
        input.pad[0] = 0;
        input.pad[1] = 0;

        for (int p = 0; p < 2; p++) {
            const Input& in = inputs[p];
            uint16_t state = 0;

            if (in.up)    state |= INPUT_UP;
            if (in.down)  state |= INPUT_DOWN;
            if (in.left)  state |= INPUT_LEFT;
            if (in.right) state |= INPUT_RIGHT;
            if (in.a)     state |= INPUT_A;
            if (in.b)     state |= INPUT_B;
            if (in.c)     state |= INPUT_C;
            if (in.start) state |= INPUT_START;
            if (in.x)     state |= INPUT_X;
            if (in.y)     state |= INPUT_Y;
            if (in.z)     state |= INPUT_Z;
            if (in.mode)  state |= INPUT_MODE;

            input.pad[p] = state;
        }
    }
};

// ---------------------------------------------------------------------------
// Emulator - Public interface implementation
// ---------------------------------------------------------------------------

Emulator::Emulator() : pImpl(new Impl()) {}

Emulator::~Emulator() {
    delete pImpl;
}

bool Emulator::LoadRom(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return false;
    }

    return pImpl->LoadRomData(buffer.data(), buffer.size());
}

bool Emulator::LoadRom(const uint8_t* data, size_t size) {
    return pImpl->LoadRomData(data, size);
}

void Emulator::Reset() {
    if (pImpl->rom_loaded) {
        system_reset();
        pImpl->frame_count = 0;
    }
}

void Emulator::HardReset() {
    Reset();
}

void Emulator::RunFrames(int frames) {
    for (int i = 0; i < frames; i++) {
        pImpl->RunFrame();
    }
}

int Emulator::RunUntilMemoryEquals(uint32_t address, uint8_t expected, int max_frames) {
    for (int i = 0; i < max_frames; i++) {
        if (ReadByte(address) == expected) {
            return i;
        }
        pImpl->RunFrame();
    }
    return -1;
}

int Emulator::RunUntil(std::function<bool()> condition, int max_frames) {
    for (int i = 0; i < max_frames; i++) {
        if (condition()) {
            return i;
        }
        pImpl->RunFrame();
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Memory Access
// ---------------------------------------------------------------------------

uint8_t Emulator::ReadByte(uint32_t address) const {
    address &= 0xFFFFFF;  // 24-bit address space

    // Work RAM: 0xFF0000 - 0xFFFFFF
    if (address >= 0xFF0000) {
        // On little-endian systems, work RAM is byteswapped for 16-bit optimization
#ifdef LSB_FIRST
        return work_ram[(address & 0xFFFF) ^ 1];
#else
        return work_ram[address & 0xFFFF];
#endif
    }

    // Z80 RAM: 0xA00000 - 0xA01FFF (8-bit bus, no byteswap needed)
    if (address >= 0xA00000 && address < 0xA02000) {
        return zram[address & 0x1FFF];
    }

    // ROM: 0x000000 - 0x3FFFFF
    if (address < 0x400000) {
        // On little-endian systems, ROM is byteswapped for 16-bit optimization
#ifdef LSB_FIRST
        return cart.rom[address ^ 1];
#else
        return cart.rom[address];
#endif
    }

    // For other regions (VDP, I/O), return open bus (0xFF)
    // In a full implementation, you would use the memory map
    return 0xFF;
}

uint16_t Emulator::ReadWord(uint32_t address) const {
    // Genesis is big-endian
    return (static_cast<uint16_t>(ReadByte(address)) << 8) |
           static_cast<uint16_t>(ReadByte(address + 1));
}

uint32_t Emulator::ReadLong(uint32_t address) const {
    return (static_cast<uint32_t>(ReadWord(address)) << 16) |
           static_cast<uint32_t>(ReadWord(address + 2));
}

void Emulator::WriteByte(uint32_t address, uint8_t value) {
    address &= 0xFFFFFF;

    // Work RAM: 0xFF0000 - 0xFFFFFF
    if (address >= 0xFF0000) {
        // On little-endian systems, work RAM is byteswapped for 16-bit optimization
#ifdef LSB_FIRST
        work_ram[(address & 0xFFFF) ^ 1] = value;
#else
        work_ram[address & 0xFFFF] = value;
#endif
        return;
    }

    // Z80 RAM: 0xA00000 - 0xA01FFF (8-bit bus, no byteswap needed)
    if (address >= 0xA00000 && address < 0xA02000) {
        zram[address & 0x1FFF] = value;
        return;
    }

    // For other regions (VDP, I/O), writes are ignored
    // In a full implementation, you would use the memory map
}

void Emulator::WriteWord(uint32_t address, uint16_t value) {
    WriteByte(address, value >> 8);
    WriteByte(address + 1, value & 0xFF);
}

void Emulator::WriteLong(uint32_t address, uint32_t value) {
    WriteWord(address, value >> 16);
    WriteWord(address + 2, value & 0xFFFF);
}

uint8_t* Emulator::GetWorkRam() {
    return work_ram;
}

const uint8_t* Emulator::GetWorkRam() const {
    return work_ram;
}

uint8_t* Emulator::GetZ80Ram() {
    return zram;
}

const uint8_t* Emulator::GetZ80Ram() const {
    return zram;
}

// ---------------------------------------------------------------------------
// CPU Registers
// ---------------------------------------------------------------------------

uint32_t Emulator::GetDataRegister(int reg) const {
    if (reg < 0 || reg > 7) return 0;
    return m68k_get_reg(static_cast<m68k_register_t>(M68K_REG_D0 + reg));
}

uint32_t Emulator::GetAddressRegister(int reg) const {
    if (reg < 0 || reg > 7) return 0;
    return m68k_get_reg(static_cast<m68k_register_t>(M68K_REG_A0 + reg));
}

uint32_t Emulator::GetPC() const {
    return m68k_get_reg(M68K_REG_PC);
}

uint16_t Emulator::GetSR() const {
    return static_cast<uint16_t>(m68k_get_reg(M68K_REG_SR));
}

// ---------------------------------------------------------------------------
// Input Control
// ---------------------------------------------------------------------------

void Emulator::SetInput(int player, const Input& inp) {
    if (player >= 0 && player < 2) {
        pImpl->inputs[player] = inp;
    }
}

const Input& Emulator::GetInput(int player) const {
    static Input empty;
    if (player >= 0 && player < 2) {
        return pImpl->inputs[player];
    }
    return empty;
}

void Emulator::PressButton(int player, const std::string& button) {
    if (player < 0 || player > 1) return;

    Input& inp = pImpl->inputs[player];

    if (button == "up") inp.up = true;
    else if (button == "down") inp.down = true;
    else if (button == "left") inp.left = true;
    else if (button == "right") inp.right = true;
    else if (button == "a" || button == "A") inp.a = true;
    else if (button == "b" || button == "B") inp.b = true;
    else if (button == "c" || button == "C") inp.c = true;
    else if (button == "start" || button == "Start") inp.start = true;
    else if (button == "x" || button == "X") inp.x = true;
    else if (button == "y" || button == "Y") inp.y = true;
    else if (button == "z" || button == "Z") inp.z = true;
    else if (button == "mode" || button == "Mode") inp.mode = true;

    // Run one frame with button pressed
    pImpl->RunFrame();

    // Release all buttons
    inp.Clear();
}

// ---------------------------------------------------------------------------
// State Management
// ---------------------------------------------------------------------------

std::vector<uint8_t> Emulator::SaveState() const {
    // Get state size
    int size = state_save(nullptr);
    if (size <= 0) return {};

    std::vector<uint8_t> buffer(size);
    state_save(buffer.data());

    return buffer;
}

bool Emulator::LoadState(const std::vector<uint8_t>& state) {
    if (state.empty()) return false;
    return state_load(const_cast<uint8_t*>(state.data())) != 0;
}

// ---------------------------------------------------------------------------
// Info
// ---------------------------------------------------------------------------

uint64_t Emulator::GetFrameCount() const {
    return pImpl->frame_count;
}

std::string Emulator::GetRomName() const {
    if (!pImpl->rom_loaded) return "";

    // Extract name from ROM header (domestic name at offset 0x120)
    char name[49] = {0};
    memcpy(name, &cart.rom[0x120], 48);

    // Trim trailing spaces
    for (int i = 47; i >= 0 && name[i] == ' '; i--) {
        name[i] = '\0';
    }

    return std::string(name);
}

bool Emulator::IsRomLoaded() const {
    return pImpl->rom_loaded;
}

} // namespace GX
