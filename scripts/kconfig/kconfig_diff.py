#!/usr/bin/env python3
r"""
\file            kconfig_diff.py
\brief           Kconfig configuration difference tool
\author          Nexus Team
\version         1.0.0
\date            2026-01-17

\copyright       Copyright (c) 2026 Nexus Team

\details         Compares two Kconfig configuration files and generates
                 detailed difference reports.
"""

import argparse
import os
import re
import sys
from typing import Dict, List, Tuple, Set, Optional
from enum import Enum


class DiffType(Enum):
    """Type of configuration difference"""
    ADDED = 'added'
    REMOVED = 'removed'
    CHANGED = 'changed'
    UNCHANGED = 'unchanged'


class ConfigEntry:
    """Configuration entry"""

    def __init__(self, symbol: str, value: Optional[str] = None,
                 is_set: bool = True):
        self.symbol = symbol
        self.value = value
        self.is_set = is_set

    def __eq__(self, other) -> bool:
        if not isinstance(other, ConfigEntry):
            return False
        return (self.symbol == other.symbol and
                self.value == other.value and
                self.is_set == other.is_set)

    def __str__(self) -> str:
        if not self.is_set:
            return f"# {self.symbol} is not set"
        elif self.value:
            return f"{self.symbol}={self.value}"
        else:
            return f"{self.symbol}=y"


class ConfigDiff:
    """Configuration difference"""

    def __init__(self, symbol: str, diff_type: DiffType,
                 old_value: Optional[ConfigEntry] = None,
                 new_value: Optional[ConfigEntry] = None):
        self.symbol = symbol
        self.diff_type = diff_type
        self.old_value = old_value
        self.new_value = new_value

    def __str__(self) -> str:
        if self.diff_type == DiffType.ADDED:
            return f"+ {self.new_value}"
        elif self.diff_type == DiffType.REMOVED:
            return f"- {self.old_value}"
        elif self.diff_type == DiffType.CHANGED:
            return f"  {self.old_value}\n→ {self.new_value}"
        else:
            return f"  {self.old_value}"


class ConfigParser:
    """Configuration file parser"""

    def __init__(self):
        self.entries: Dict[str, ConfigEntry] = {}

    def parse_file(self, config_path: str) -> bool:
        """Parse configuration file"""
        try:
            with open(config_path, 'r', encoding='utf-8') as f:
                for line in f:
                    line = line.strip()

                    # Skip empty lines and comments (except "is not set")
                    if not line or (line.startswith('#') and 'is not set' not in line):
                        continue

                    # Parse "is not set" lines
                    match = re.match(r'#\s*(CONFIG_\w+)\s+is not set', line)
                    if match:
                        symbol = match.group(1)
                        self.entries[symbol] = ConfigEntry(symbol, is_set=False)
                        continue

                    # Parse config lines
                    match = re.match(r'(CONFIG_\w+)=(.+)', line)
                    if match:
                        symbol = match.group(1)
                        value = match.group(2)
                        self.entries[symbol] = ConfigEntry(symbol, value, is_set=True)
                        continue

            return True

        except FileNotFoundError:
            print(f"Error: File not found: {config_path}", file=sys.stderr)
            return False
        except Exception as e:
            print(f"Error parsing file: {e}", file=sys.stderr)
            return False

    def get_symbols(self) -> Set[str]:
        """Get all configuration symbols"""
        return set(self.entries.keys())

    def get_entry(self, symbol: str) -> Optional[ConfigEntry]:
        """Get configuration entry"""
        return self.entries.get(symbol)


class ConfigComparator:
    """Configuration comparator"""

    def __init__(self):
        self.diffs: List[ConfigDiff] = []

    def compare(self, old_config: ConfigParser,
                new_config: ConfigParser) -> List[ConfigDiff]:
        """Compare two configurations"""
        self.diffs = []

        old_symbols = old_config.get_symbols()
        new_symbols = new_config.get_symbols()

        all_symbols = old_symbols | new_symbols

        for symbol in sorted(all_symbols):
            old_entry = old_config.get_entry(symbol)
            new_entry = new_config.get_entry(symbol)

            if old_entry is None and new_entry is not None:
                # Added
                self.diffs.append(ConfigDiff(symbol, DiffType.ADDED,
                                            new_value=new_entry))

            elif old_entry is not None and new_entry is None:
                # Removed
                self.diffs.append(ConfigDiff(symbol, DiffType.REMOVED,
                                            old_value=old_entry))

            elif old_entry != new_entry:
                # Changed
                self.diffs.append(ConfigDiff(symbol, DiffType.CHANGED,
                                            old_value=old_entry,
                                            new_value=new_entry))

            else:
                # Unchanged (only include if requested)
                pass

        return self.diffs

    def get_stats(self) -> Dict[str, int]:
        """Get difference statistics"""
        stats = {
            'added': 0,
            'removed': 0,
            'changed': 0,
            'total': len(self.diffs)
        }

        for diff in self.diffs:
            if diff.diff_type == DiffType.ADDED:
                stats['added'] += 1
            elif diff.diff_type == DiffType.REMOVED:
                stats['removed'] += 1
            elif diff.diff_type == DiffType.CHANGED:
                stats['changed'] += 1

        return stats


class DiffReporter:
    """Difference report generator"""

    def __init__(self, diffs: List[ConfigDiff]):
        self.diffs = diffs

    def generate_text_report(self, output_path: Optional[str] = None):
        """Generate text format report"""
        lines = []
        lines.append("=" * 80)
        lines.append("Kconfig Configuration Difference Report")
        lines.append("=" * 80)
        lines.append("")

        # Statistics
        stats = self._get_stats()
        lines.append("Summary:")
        lines.append(f"  Added:   {stats['added']}")
        lines.append(f"  Removed: {stats['removed']}")
        lines.append(f"  Changed: {stats['changed']}")
        lines.append(f"  Total:   {stats['total']}")
        lines.append("")

        # Group by category
        categories = self._group_by_category()

        for category, diffs in sorted(categories.items()):
            if not diffs:
                continue

            lines.append("-" * 80)
            lines.append(f"{category}")
            lines.append("-" * 80)

            for diff in diffs:
                if diff.diff_type == DiffType.ADDED:
                    lines.append(f"  [+] {diff.new_value}")
                elif diff.diff_type == DiffType.REMOVED:
                    lines.append(f"  [-] {diff.old_value}")
                elif diff.diff_type == DiffType.CHANGED:
                    lines.append(f"  [~] {diff.old_value}")
                    lines.append(f"      → {diff.new_value}")

            lines.append("")

        report = "\n".join(lines)

        if output_path:
            try:
                os.makedirs(os.path.dirname(output_path) or '.', exist_ok=True)
                with open(output_path, 'w', encoding='utf-8') as f:
                    f.write(report)
                print(f"Report saved to: {output_path}")
            except Exception as e:
                print(f"Error writing report: {e}", file=sys.stderr)
        else:
            print(report)

    def generate_html_report(self, output_path: str):
        """Generate HTML format report"""
        stats = self._get_stats()
        categories = self._group_by_category()

        html = []
        html.append("<!DOCTYPE html>")
        html.append("<html>")
        html.append("<head>")
        html.append("  <meta charset='utf-8'>")
        html.append("  <title>Kconfig Configuration Diff</title>")
        html.append("  <style>")
        html.append("    body { font-family: monospace; margin: 20px; }")
        html.append("    h1 { color: #333; }")
        html.append("    .stats { background: #f0f0f0; padding: 10px; margin: 10px 0; }")
        html.append("    .category { margin: 20px 0; }")
        html.append("    .category h2 { color: #666; border-bottom: 2px solid #ccc; }")
        html.append("    .diff { margin: 5px 0; padding: 5px; }")
        html.append("    .added { background: #d4edda; color: #155724; }")
        html.append("    .removed { background: #f8d7da; color: #721c24; }")
        html.append("    .changed { background: #fff3cd; color: #856404; }")
        html.append("    .arrow { color: #666; margin-left: 20px; }")
        html.append("  </style>")
        html.append("</head>")
        html.append("<body>")
        html.append("  <h1>Kconfig Configuration Difference Report</h1>")

        # Statistics
        html.append("  <div class='stats'>")
        html.append("    <h2>Summary</h2>")
        html.append(f"    <p>Added: {stats['added']}</p>")
        html.append(f"    <p>Removed: {stats['removed']}</p>")
        html.append(f"    <p>Changed: {stats['changed']}</p>")
        html.append(f"    <p>Total: {stats['total']}</p>")
        html.append("  </div>")

        # Differences by category
        for category, diffs in sorted(categories.items()):
            if not diffs:
                continue

            html.append(f"  <div class='category'>")
            html.append(f"    <h2>{category}</h2>")

            for diff in diffs:
                if diff.diff_type == DiffType.ADDED:
                    html.append(f"    <div class='diff added'>[+] {diff.new_value}</div>")
                elif diff.diff_type == DiffType.REMOVED:
                    html.append(f"    <div class='diff removed'>[-] {diff.old_value}</div>")
                elif diff.diff_type == DiffType.CHANGED:
                    html.append(f"    <div class='diff changed'>[~] {diff.old_value}")
                    html.append(f"      <div class='arrow'>→ {diff.new_value}</div>")
                    html.append(f"    </div>")

            html.append("  </div>")

        html.append("</body>")
        html.append("</html>")

        try:
            os.makedirs(os.path.dirname(output_path) or '.', exist_ok=True)
            with open(output_path, 'w', encoding='utf-8') as f:
                f.write("\n".join(html))
            print(f"HTML report saved to: {output_path}")
        except Exception as e:
            print(f"Error writing HTML report: {e}", file=sys.stderr)

    def _get_stats(self) -> Dict[str, int]:
        """Get statistics"""
        stats = {'added': 0, 'removed': 0, 'changed': 0, 'total': len(self.diffs)}
        for diff in self.diffs:
            if diff.diff_type == DiffType.ADDED:
                stats['added'] += 1
            elif diff.diff_type == DiffType.REMOVED:
                stats['removed'] += 1
            elif diff.diff_type == DiffType.CHANGED:
                stats['changed'] += 1
        return stats

    def _group_by_category(self) -> Dict[str, List[ConfigDiff]]:
        """Group differences by category"""
        categories: Dict[str, List[ConfigDiff]] = {}

        for diff in self.diffs:
            # Extract category from symbol name
            symbol = diff.symbol

            if 'PLATFORM' in symbol:
                category = 'Platform Configuration'
            elif 'OSAL' in symbol:
                category = 'OSAL Configuration'
            elif 'HAL' in symbol:
                category = 'HAL Configuration'
            elif 'UART' in symbol:
                category = 'UART Configuration'
            elif 'SPI' in symbol:
                category = 'SPI Configuration'
            elif 'I2C' in symbol:
                category = 'I2C Configuration'
            elif 'GPIO' in symbol:
                category = 'GPIO Configuration'
            elif 'TIMER' in symbol:
                category = 'Timer Configuration'
            elif 'ADC' in symbol:
                category = 'ADC Configuration'
            elif 'DAC' in symbol:
                category = 'DAC Configuration'
            else:
                category = 'Other Configuration'

            if category not in categories:
                categories[category] = []
            categories[category].append(diff)

        return categories


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description='Compare Kconfig configuration files'
    )
    parser.add_argument(
        'old_config',
        help='Old configuration file'
    )
    parser.add_argument(
        'new_config',
        help='New configuration file'
    )
    parser.add_argument(
        '-o', '--output',
        help='Output report file (default: stdout)'
    )
    parser.add_argument(
        '-f', '--format',
        choices=['text', 'html'],
        default='text',
        help='Report format (default: text)'
    )

    args = parser.parse_args()

    # Parse configurations
    old_parser = ConfigParser()
    if not old_parser.parse_file(args.old_config):
        return 1

    new_parser = ConfigParser()
    if not new_parser.parse_file(args.new_config):
        return 1

    # Compare configurations
    comparator = ConfigComparator()
    diffs = comparator.compare(old_parser, new_parser)

    # Generate report
    reporter = DiffReporter(diffs)

    if args.format == 'html':
        if not args.output:
            print("Error: HTML format requires -o/--output option", file=sys.stderr)
            return 1
        reporter.generate_html_report(args.output)
    else:
        reporter.generate_text_report(args.output)

    return 0


if __name__ == '__main__':
    sys.exit(main())
