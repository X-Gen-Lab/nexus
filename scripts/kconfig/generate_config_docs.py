#!/usr/bin/env python3
"""
Generate configuration options reference documentation from Kconfig files.

This script parses Kconfig files and generates a comprehensive Markdown
documentation of all configuration options, including their types, defaults,
ranges, dependencies, and help text.
"""

import argparse
import os
import sys
from datetime import datetime
from typing import Dict, List, Optional, Tuple

try:
    import kconfiglib
except ImportError:
    print("Error: kconfiglib not installed. Install with: pip install kconfiglib")
    sys.exit(1)


def parse_kconfig(kconfig_file: str) -> kconfiglib.Kconfig:
    """
    Parse Kconfig file using kconfiglib.

    Args:
        kconfig_file: Path to root Kconfig file

    Returns:
        Parsed Kconfig object
    """
    try:
        kconf = kconfiglib.Kconfig(kconfig_file)
        return kconf
    except Exception as e:
        print(f"Error parsing Kconfig: {e}")
        sys.exit(1)


def get_symbol_category(symbol_name: str) -> str:
    """
    Determine the category of a configuration symbol based on its name.

    Args:
        symbol_name: Name of the configuration symbol

    Returns:
        Category name
    """
    if 'PLATFORM' in symbol_name:
        return 'Platform Configuration'
    elif 'OSAL' in symbol_name:
        return 'OSAL Configuration'
    elif 'HAL' in symbol_name:
        return 'HAL Configuration'
    elif 'UART' in symbol_name:
        return 'UART Configuration'
    elif 'SPI' in symbol_name:
        return 'SPI Configuration'
    elif 'I2C' in symbol_name:
        return 'I2C Configuration'
    elif 'GPIO' in symbol_name:
        return 'GPIO Configuration'
    elif 'TIMER' in symbol_name:
        return 'Timer Configuration'
    elif 'ADC' in symbol_name:
        return 'ADC Configuration'
    elif 'DAC' in symbol_name:
        return 'DAC Configuration'
    elif 'DMA' in symbol_name:
        return 'DMA Configuration'
    elif 'LINKER' in symbol_name:
        return 'Linker Configuration'
    else:
        return 'General Configuration'


def get_symbol_type_str(sym: kconfiglib.Symbol) -> str:
    """
    Get human-readable type string for a symbol.

    Args:
        sym: Kconfig symbol

    Returns:
        Type string
    """
    type_map = {
        kconfiglib.BOOL: 'bool',
        kconfiglib.TRISTATE: 'tristate',
        kconfiglib.STRING: 'string',
        kconfiglib.INT: 'int',
        kconfiglib.HEX: 'hex',
    }
    return type_map.get(sym.type, 'unknown')


def get_symbol_default(sym: kconfiglib.Symbol) -> Optional[str]:
    """
    Get the default value of a symbol.

    Args:
        sym: Kconfig symbol

    Returns:
        Default value as string, or None
    """
    if not sym.defaults:
        return None

    # Get the first default value
    default_expr, cond = sym.defaults[0]

    if sym.type == kconfiglib.BOOL:
        if default_expr.tri_value == 2:  # y
            return 'y'
        elif default_expr.tri_value == 0:  # n
            return 'n'
        else:
            return 'm'
    elif sym.type in (kconfiglib.STRING, kconfiglib.INT, kconfiglib.HEX):
        return default_expr.str_value

    return None


def get_symbol_range(sym: kconfiglib.Symbol) -> Optional[Tuple[str, str]]:
    """
    Get the range constraint of a symbol.

    Args:
        sym: Kconfig symbol

    Returns:
        Tuple of (min, max) as strings, or None
    """
    if not sym.ranges:
        return None

    # Get the first range
    low, high, cond = sym.ranges[0]

    if sym.type == kconfiglib.INT:
        return (str(low.str_value), str(high.str_value))
    elif sym.type == kconfiglib.HEX:
        return (f"0x{low.str_value}", f"0x{high.str_value}")

    return None


def get_symbol_dependencies(sym: kconfiglib.Symbol) -> List[str]:
    """
    Get the dependencies of a symbol.

    Args:
        sym: Kconfig symbol

    Returns:
        List of dependency symbol names
    """
    deps = []

    if sym.direct_dep is not sym.kconfig.y:
        # Extract symbol names from dependency expression
        deps_str = kconfiglib.expr_str(sym.direct_dep)
        # Simple parsing - just extract symbol names
        for token in deps_str.split():
            if token.isupper() or '_' in token:
                if token not in ['&&', '||', '!', '(', ')']:
                    deps.append(token)

    return deps


def generate_markdown_docs(kconf: kconfiglib.Kconfig, output_file: str):
    """
    Generate Markdown documentation from parsed Kconfig.

    Args:
        kconf: Parsed Kconfig object
        output_file: Output Markdown file path
    """
    # Organize symbols by category
    categories: Dict[str, List[kconfiglib.Symbol]] = {}

    for sym in kconf.unique_defined_syms:
        # Skip internal symbols
        if not sym.name or sym.name.startswith('_'):
            continue

        # Skip choice symbols
        if sym.choice:
            continue

        category = get_symbol_category(sym.name)
        if category not in categories:
            categories[category] = []
        categories[category].append(sym)

    # Generate Markdown
    lines = []

    # Header
    lines.append('# Nexus Configuration Options Reference')
    lines.append('')
    lines.append(f'Generated: {datetime.now().strftime("%Y-%m-%d %H:%M:%S")}')
    lines.append('')
    lines.append('This document provides a comprehensive reference of all configuration ')
    lines.append('options available in the Nexus platform.')
    lines.append('')

    # Table of contents
    lines.append('## Table of Contents')
    lines.append('')
    for i, category in enumerate(sorted(categories.keys()), 1):
        anchor = category.lower().replace(' ', '-')
        lines.append(f'{i}. [{category}](#{anchor})')
    lines.append('')

    # Configuration options by category
    for category in sorted(categories.keys()):
        lines.append(f'## {category}')
        lines.append('')

        symbols = sorted(categories[category], key=lambda s: s.name)

        for sym in symbols:
            # Symbol name
            lines.append(f'### {sym.name}')
            lines.append('')

            # Type
            type_str = get_symbol_type_str(sym)
            lines.append(f'**Type**: `{type_str}`')
            lines.append('')

            # Default value
            default = get_symbol_default(sym)
            if default:
                lines.append(f'**Default**: `{default}`')
                lines.append('')

            # Range
            range_val = get_symbol_range(sym)
            if range_val:
                min_val, max_val = range_val
                lines.append(f'**Range**: `{min_val}` to `{max_val}`')
                lines.append('')

            # Dependencies
            deps = get_symbol_dependencies(sym)
            if deps:
                lines.append(f'**Depends on**: {", ".join(f"`{d}`" for d in deps)}')
                lines.append('')

            # Help text
            if sym.nodes and sym.nodes[0].help:
                help_text = sym.nodes[0].help.strip()
                lines.append('**Description**:')
                lines.append('')
                for line in help_text.split('\n'):
                    lines.append(line)
                lines.append('')

            # Prompt
            if sym.nodes and sym.nodes[0].prompt:
                prompt_text = sym.nodes[0].prompt[0]
                lines.append(f'**Prompt**: "{prompt_text}"')
                lines.append('')

            lines.append('---')
            lines.append('')

    # Write to file
    try:
        os.makedirs(os.path.dirname(output_file) or '.', exist_ok=True)
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write('\n'.join(lines))
        print(f'Generated: {output_file}')
    except Exception as e:
        print(f'Error writing output file: {e}')
        sys.exit(1)


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description='Generate configuration options reference documentation'
    )
    parser.add_argument(
        '--kconfig', '-k',
        default='Kconfig',
        help='Path to root Kconfig file (default: Kconfig)'
    )
    parser.add_argument(
        '--output', '-o',
        default='docs/config_reference.md',
        help='Output Markdown file path (default: docs/config_reference.md)'
    )

    args = parser.parse_args()

    # Check if Kconfig file exists
    if not os.path.exists(args.kconfig):
        print(f'Error: Kconfig file not found: {args.kconfig}')
        sys.exit(1)

    # Parse Kconfig
    print(f'Parsing Kconfig file: {args.kconfig}')
    kconf = parse_kconfig(args.kconfig)

    # Generate documentation
    print(f'Generating documentation...')
    generate_markdown_docs(kconf, args.output)

    print('Done!')
    return 0


if __name__ == '__main__':
    sys.exit(main())
