cmake_minimum_required(VERSION 3.14)
project(gxtest VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# Build type defaults to Release for maximum speed
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Optimization flags for headless performance
if(MSVC)
    set(CMAKE_C_FLAGS_RELEASE "/O2 /DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
else()
    set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
endif()

# Platform detection
include(TestBigEndian)
TEST_BIG_ENDIAN(IS_BIG_ENDIAN)

# Export compile commands for IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -----------------------------------------------------------------------------
# Genesis Plus GX Core Library
# -----------------------------------------------------------------------------

# Core source files (excluding CD hardware for simplicity)
set(GENPLUSGX_SOURCES
    # Main system
    vendor/genplusgx/genesis.c
    vendor/genplusgx/system.c
    vendor/genplusgx/state.c
    vendor/genplusgx/loadrom.c
    vendor/genplusgx/io_ctrl.c
    vendor/genplusgx/mem68k.c
    vendor/genplusgx/memz80.c
    vendor/genplusgx/membnk.c
    vendor/genplusgx/vdp_ctrl.c
    vendor/genplusgx/vdp_render.c

    # M68K CPU
    vendor/genplusgx/m68k/m68kcpu.c
    vendor/genplusgx/m68k/s68kcpu.c

    # Z80 CPU
    vendor/genplusgx/z80/z80.c

    # Sound
    vendor/genplusgx/sound/sound.c
    vendor/genplusgx/sound/psg.c
    vendor/genplusgx/sound/ym2612.c
    vendor/genplusgx/sound/ym2413.c
    vendor/genplusgx/sound/ym3438.c
    vendor/genplusgx/sound/opll.c
    vendor/genplusgx/sound/blip_buf.c
    vendor/genplusgx/sound/eq.c

    # NTSC filter
    vendor/genplusgx/ntsc/md_ntsc.c
    vendor/genplusgx/ntsc/sms_ntsc.c

    # Input hardware
    vendor/genplusgx/input_hw/input.c
    vendor/genplusgx/input_hw/gamepad.c
    vendor/genplusgx/input_hw/mouse.c
    vendor/genplusgx/input_hw/lightgun.c
    vendor/genplusgx/input_hw/paddle.c
    vendor/genplusgx/input_hw/sportspad.c
    vendor/genplusgx/input_hw/teamplayer.c
    vendor/genplusgx/input_hw/xe_1ap.c
    vendor/genplusgx/input_hw/activator.c
    vendor/genplusgx/input_hw/graphic_board.c
    vendor/genplusgx/input_hw/terebi_oekaki.c
    vendor/genplusgx/input_hw/smash.c

    # Cart hardware
    vendor/genplusgx/cart_hw/md_cart.c
    vendor/genplusgx/cart_hw/sms_cart.c
    vendor/genplusgx/cart_hw/sram.c
    vendor/genplusgx/cart_hw/eeprom_93c.c
    vendor/genplusgx/cart_hw/eeprom_i2c.c
    vendor/genplusgx/cart_hw/eeprom_spi.c
    vendor/genplusgx/cart_hw/flash_cfi.c
    vendor/genplusgx/cart_hw/ggenie.c
    vendor/genplusgx/cart_hw/areplay.c
    # Excluded: megasd.c and yx5200.c require MP3 library (minimp3)

    # SVP (Virtua Racing chip)
    vendor/genplusgx/cart_hw/svp/svp.c
    vendor/genplusgx/cart_hw/svp/ssp16.c

    # Stubs for Sega CD, MegaSD, YX5200 (not needed for cartridge games)
    src/stubs.c
)

# Create the core library
add_library(genplusgx_core STATIC ${GENPLUSGX_SOURCES})

# Include directories for the core
target_include_directories(genplusgx_core PRIVATE
    ${CMAKE_SOURCE_DIR}/src              # For our osd.h
    ${CMAKE_SOURCE_DIR}/vendor/genplusgx
    ${CMAKE_SOURCE_DIR}/vendor/genplusgx/m68k
    ${CMAKE_SOURCE_DIR}/vendor/genplusgx/z80
    ${CMAKE_SOURCE_DIR}/vendor/genplusgx/sound
    ${CMAKE_SOURCE_DIR}/vendor/genplusgx/input_hw
    ${CMAKE_SOURCE_DIR}/vendor/genplusgx/cart_hw
    ${CMAKE_SOURCE_DIR}/vendor/genplusgx/cart_hw/svp
    ${CMAKE_SOURCE_DIR}/vendor/genplusgx/ntsc
    ${CMAKE_SOURCE_DIR}/vendor/genplusgx/cd_hw
)

# Compiler definitions for the core
target_compile_definitions(genplusgx_core PRIVATE
    $<$<NOT:$<BOOL:${IS_BIG_ENDIAN}>>:LSB_FIRST>  # Little-endian (x86/ARM)
    USE_16BPP_RENDERING          # 16-bit color rendering
    HAVE_NO_SPRITE_LIMIT         # Remove sprite flicker limit
    MAXROMSIZE=33554432          # 32MB max ROM size
    HAVE_YM3438_CORE             # Nuked OPN2 core
    HAVE_OPLL_CORE               # Nuked OPLL core
)

# Platform-specific settings
if(UNIX AND NOT APPLE)
    # Linux requires explicit math library linking
    target_link_libraries(genplusgx_core PRIVATE m)
endif()

# Suppress warnings in vendored code
if(MSVC)
    target_compile_options(genplusgx_core PRIVATE
        /W0
    )
else()
    target_compile_options(genplusgx_core PRIVATE
        -Wno-unused-parameter
        -Wno-sign-compare
        -Wno-implicit-function-declaration
        -Wno-missing-field-initializers
        -Wno-unused-variable
        -Wno-pointer-sign
    )
endif()

# -----------------------------------------------------------------------------
# GoogleTest Integration (must be before gxtest since gxtest.h includes gtest)
# -----------------------------------------------------------------------------

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
# Prevent GoogleTest from overriding compiler/linker options
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# -----------------------------------------------------------------------------
# gxtest Library (C++ wrapper)
# -----------------------------------------------------------------------------

add_library(gxtest STATIC
    src/gxtest.cpp
)

target_include_directories(gxtest PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_include_directories(gxtest PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor/genplusgx
    ${CMAKE_SOURCE_DIR}/vendor/genplusgx/m68k
    ${CMAKE_SOURCE_DIR}/vendor/genplusgx/z80
    ${CMAKE_SOURCE_DIR}/vendor/genplusgx/sound
    ${CMAKE_SOURCE_DIR}/vendor/genplusgx/input_hw
    ${CMAKE_SOURCE_DIR}/vendor/genplusgx/cart_hw
    ${CMAKE_SOURCE_DIR}/vendor/genplusgx/cart_hw/svp
    ${CMAKE_SOURCE_DIR}/vendor/genplusgx/ntsc
    ${CMAKE_SOURCE_DIR}/vendor/genplusgx/cd_hw
)

# Must match core's endianness definition for correct ROM handling
target_compile_definitions(gxtest PRIVATE
    $<$<NOT:$<BOOL:${IS_BIG_ENDIAN}>>:LSB_FIRST>
)

# gxtest.h includes gtest headers, so GTest::gtest must be PUBLIC
target_link_libraries(gxtest PUBLIC GTest::gtest PRIVATE genplusgx_core)

# -----------------------------------------------------------------------------
# Example Test
# -----------------------------------------------------------------------------

add_executable(gxtest_example
    tests/example_test.cpp
)

target_link_libraries(gxtest_example
    gxtest
    genplusgx_core
    GTest::gtest_main
)

target_include_directories(gxtest_example PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/vendor/genplusgx
)

include(GoogleTest)
gtest_discover_tests(gxtest_example)

# -----------------------------------------------------------------------------
# Prime Sieve Test (verifies emulator correctness)
# -----------------------------------------------------------------------------

add_executable(gxtest_prime_sieve
    tests/prime_sieve_test.cpp
)

target_link_libraries(gxtest_prime_sieve
    gxtest
    genplusgx_core
    GTest::gtest_main
)

target_include_directories(gxtest_prime_sieve PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/vendor/genplusgx
)

gtest_discover_tests(gxtest_prime_sieve)

# -----------------------------------------------------------------------------
# Symbol Example Test (demonstrates symbol-based testing)
# -----------------------------------------------------------------------------

# Option to build ROMs from source (requires m68k-elf toolchain)
option(GXTEST_BUILD_ROMS "Build test ROMs from source (requires m68k-elf-gcc)" OFF)

# Find Python for symbol extraction
find_package(Python3 COMPONENTS Interpreter)

if(GXTEST_BUILD_ROMS)
    # Find m68k cross-compiler
    find_program(M68K_GCC m68k-elf-gcc)
    find_program(M68K_AS m68k-elf-as)
    find_program(M68K_LD m68k-elf-ld)
    find_program(M68K_NM m68k-elf-nm)
    find_program(M68K_OBJCOPY m68k-elf-objcopy)

    if(NOT Python3_FOUND)
        message(WARNING "Python3 not found, ROM builds disabled (needed for symbol extraction)")
    elseif(M68K_GCC AND M68K_AS AND M68K_LD AND M68K_NM AND M68K_OBJCOPY)
        message(STATUS "m68k-elf toolchain and Python3 found, enabling ROM builds")

        set(SYMBOL_EXAMPLE_DIR ${CMAKE_SOURCE_DIR}/roms/symbol_example)
        set(SYMBOL_EXAMPLE_ROM ${SYMBOL_EXAMPLE_DIR}/symbol_example.bin)
        set(SYMBOL_EXAMPLE_ELF ${SYMBOL_EXAMPLE_DIR}/symbol_example.elf)
        set(SYMBOL_EXAMPLE_SYMBOLS_TXT ${SYMBOL_EXAMPLE_DIR}/symbol_example_symbols.txt)
        set(GENERATED_INCLUDE_DIR ${CMAKE_BINARY_DIR}/generated)

        # Create generated include directory
        file(MAKE_DIRECTORY ${GENERATED_INCLUDE_DIR})

        # Build the ROM using the Makefile
        add_custom_command(
            OUTPUT ${SYMBOL_EXAMPLE_ROM} ${SYMBOL_EXAMPLE_ELF}
            COMMAND make -C ${SYMBOL_EXAMPLE_DIR} ${SYMBOL_EXAMPLE_ROM}
            DEPENDS
                ${SYMBOL_EXAMPLE_DIR}/main.c
                ${SYMBOL_EXAMPLE_DIR}/crt0.s
                ${SYMBOL_EXAMPLE_DIR}/genesis.ld
                ${SYMBOL_EXAMPLE_DIR}/Makefile
            COMMENT "Building symbol_example ROM..."
        )

        # Extract symbols from ELF
        add_custom_command(
            OUTPUT ${SYMBOL_EXAMPLE_SYMBOLS_TXT}
            COMMAND ${M68K_NM} -n ${SYMBOL_EXAMPLE_ELF} > ${SYMBOL_EXAMPLE_SYMBOLS_TXT}
            DEPENDS ${SYMBOL_EXAMPLE_ELF}
            COMMENT "Extracting symbols from symbol_example.elf..."
        )

        # Generate C++ symbol header
        add_custom_command(
            OUTPUT ${GENERATED_INCLUDE_DIR}/symbol_example_symbols.h
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/elf2sym.py
                    ${SYMBOL_EXAMPLE_SYMBOLS_TXT} > ${GENERATED_INCLUDE_DIR}/symbol_example_symbols.h
            DEPENDS
                ${SYMBOL_EXAMPLE_SYMBOLS_TXT}
                ${CMAKE_SOURCE_DIR}/tools/elf2sym.py
            COMMENT "Generating symbol_example_symbols.h..."
        )

        # Generate C++ ROM header with embedded binary
        add_custom_command(
            OUTPUT ${GENERATED_INCLUDE_DIR}/symbol_example_rom.h
            COMMAND make -C ${SYMBOL_EXAMPLE_DIR} symbol_example_rom.h
            COMMAND ${CMAKE_COMMAND} -E copy
                    ${SYMBOL_EXAMPLE_DIR}/symbol_example_rom.h
                    ${GENERATED_INCLUDE_DIR}/symbol_example_rom.h
            DEPENDS ${SYMBOL_EXAMPLE_ROM}
            COMMENT "Generating symbol_example_rom.h..."
        )

        # Custom target for ROM generation
        add_custom_target(symbol_example_rom_gen
            DEPENDS
                ${GENERATED_INCLUDE_DIR}/symbol_example_symbols.h
                ${GENERATED_INCLUDE_DIR}/symbol_example_rom.h
        )

        # Symbol-based test executable
        add_executable(gxtest_symbol_example
            tests/symbol_example_test.cpp
        )

        add_dependencies(gxtest_symbol_example symbol_example_rom_gen)

        target_link_libraries(gxtest_symbol_example
            gxtest
            genplusgx_core
            GTest::gtest_main
        )

        target_include_directories(gxtest_symbol_example PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${GENERATED_INCLUDE_DIR}
            ${CMAKE_SOURCE_DIR}/vendor/genplusgx
        )

        gtest_discover_tests(gxtest_symbol_example)

        message(STATUS "Symbol example test enabled")
    else()
        message(WARNING "m68k-elf toolchain not found, ROM builds disabled")
    endif()
else()
    # When GXTEST_BUILD_ROMS is OFF, check for pre-built headers
    set(PREBUILT_SYMBOLS ${CMAKE_SOURCE_DIR}/tests/symbol_example_symbols.h)
    set(PREBUILT_ROM ${CMAKE_SOURCE_DIR}/tests/symbol_example_rom.h)

    if(EXISTS ${PREBUILT_SYMBOLS} AND EXISTS ${PREBUILT_ROM})
        message(STATUS "Using pre-built symbol_example headers")

        add_executable(gxtest_symbol_example
            tests/symbol_example_test.cpp
        )

        target_link_libraries(gxtest_symbol_example
            gxtest
            genplusgx_core
            GTest::gtest_main
        )

        target_include_directories(gxtest_symbol_example PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/vendor/genplusgx
        )

        gtest_discover_tests(gxtest_symbol_example)
    else()
        message(STATUS "Symbol example test disabled (no pre-built headers and GXTEST_BUILD_ROMS=OFF)")
        message(STATUS "To enable: cmake -DGXTEST_BUILD_ROMS=ON or run 'make install' in roms/symbol_example/")
    endif()
endif()

# -----------------------------------------------------------------------------
# Installation
# -----------------------------------------------------------------------------

install(TARGETS gxtest genplusgx_core
    ARCHIVE DESTINATION lib
)

install(FILES include/gxtest.h
    DESTINATION include
)
