#!/usr/bin/env python3
"""
elf2sym.py - Extract symbols from Genesis ELF files and generate C++ headers

Reads nm output and generates a C++ header with symbol addresses for use
in gxtest fixtures. Only exports symbols in Genesis work RAM (0xFF0000-0xFFFFFF).

Usage:
    nm -n rom.elf | python3 elf2sym.py > symbols.h
    # or
    python3 elf2sym.py rom_symbols.txt > symbols.h
"""

import sys
import re
import argparse
from datetime import datetime


def parse_nm_output(lines):
    """Parse nm output and extract symbols with addresses."""
    # nm output format: ADDRESS TYPE NAME
    # Example: 00ff0000 D game_score
    # Types: D=data, B=bss, T=text, etc.
    regex = re.compile(r'^([0-9a-fA-F]+)\s+([a-zA-Z])\s+(\S+)')

    symbols = []
    for line in lines:
        line = line.strip()
        if not line:
            continue
        match = regex.match(line)
        if match:
            addr_hex, sym_type, name = match.groups()
            addr = int(addr_hex, 16)
            symbols.append({
                'address': addr,
                'type': sym_type,
                'name': name
            })
    return symbols


def filter_ram_symbols(symbols, ram_start=0xFF0000, ram_end=0xFFFFFF):
    """Filter to only include symbols in Genesis work RAM range."""
    return [s for s in symbols if ram_start <= s['address'] <= ram_end]


# Linker-generated symbols to exclude from output
LINKER_SYMBOLS = {
    '__bss_start', '__bss_end',
    '__data_start', '__data_end', '__data_load',
    '__text_start', '__text_end',
    '__stack', '__heap_start', '__heap_end',
    '_etext', '_edata', '_end',
}


def is_linker_symbol(name):
    """Check if symbol is a linker-generated marker."""
    return name in LINKER_SYMBOLS or name.startswith('__')


def sanitize_name(name):
    """Sanitize symbol name for C++ identifier use."""
    # Remove single leading underscore (common C compiler convention)
    # but preserve multiple underscores to avoid collisions
    # e.g., '_foo' -> 'foo', '__foo' -> '_foo'
    if name.startswith('_') and not name.startswith('__'):
        name = name[1:]
    elif name.startswith('__'):
        name = name[1:]  # '__foo' -> '_foo'
    # Replace any non-identifier characters
    name = re.sub(r'[^a-zA-Z0-9_]', '_', name)
    # Ensure it doesn't start with a digit
    if name and name[0].isdigit():
        name = '_' + name
    return name


def generate_header(symbols, namespace='Sym', source_file=None):
    """Generate C++ header content from symbol list."""
    lines = []

    # Header guard and includes
    lines.append('#pragma once')
    lines.append('')
    lines.append('// Auto-generated symbol table from Genesis ELF')
    lines.append(f'// Generated: {datetime.now().isoformat()}')
    if source_file:
        lines.append(f'// Source: {source_file}')
    lines.append('')
    lines.append('#include <cstdint>')
    lines.append('')
    lines.append(f'namespace {namespace} {{')
    lines.append('')

    # Sort by address for readability
    symbols_sorted = sorted(symbols, key=lambda s: s['address'])

    # Track seen names to avoid duplicates
    seen_names = set()

    for sym in symbols_sorted:
        # Skip linker-generated symbols
        if is_linker_symbol(sym['name']):
            continue
        clean_name = sanitize_name(sym['name'])
        if not clean_name:
            continue
        if clean_name in seen_names:
            print(f'Warning: Skipping duplicate symbol {clean_name} '
                  f'(from {sym["name"]})', file=sys.stderr)
            continue
        seen_names.add(clean_name)

        addr = sym['address']

        # Add comment with original name if different
        comment = ''
        if clean_name != sym['name']:
            comment = f'  // original: {sym["name"]}'

        lines.append(f'    constexpr uint32_t {clean_name} = 0x{addr:06X};{comment}')

    lines.append('')
    lines.append(f'}}  // namespace {namespace}')
    lines.append('')

    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(
        description='Generate C++ symbol header from nm output',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
    nm -n rom.elf | python3 elf2sym.py
    python3 elf2sym.py symbols.txt
    python3 elf2sym.py symbols.txt --namespace GameSymbols
        '''
    )
    parser.add_argument('input', nargs='?', default='-',
                        help='Input file (nm output), or - for stdin')
    parser.add_argument('--namespace', '-n', default='Sym',
                        help='C++ namespace name (default: Sym)')
    parser.add_argument('--ram-start', type=lambda x: int(x, 0), default=0xFF0000,
                        help='RAM region start address (default: 0xFF0000)')
    parser.add_argument('--ram-end', type=lambda x: int(x, 0), default=0xFFFFFF,
                        help='RAM region end address (default: 0xFFFFFF)')

    args = parser.parse_args()

    # Read input
    if args.input == '-':
        lines = sys.stdin.readlines()
        source_file = 'stdin'
    else:
        try:
            with open(args.input, 'r') as f:
                lines = f.readlines()
            source_file = args.input
        except FileNotFoundError:
            print(f'Error: File not found: {args.input}', file=sys.stderr)
            sys.exit(1)
        except IOError as e:
            print(f'Error reading file: {e}', file=sys.stderr)
            sys.exit(1)

    # Parse and filter symbols
    symbols = parse_nm_output(lines)
    ram_symbols = filter_ram_symbols(symbols, args.ram_start, args.ram_end)

    if not ram_symbols:
        print('Warning: No symbols found in RAM range '
              f'0x{args.ram_start:06X}-0x{args.ram_end:06X}', file=sys.stderr)

    # Generate and output header
    header = generate_header(ram_symbols, args.namespace, source_file)
    print(header)


if __name__ == '__main__':
    main()
