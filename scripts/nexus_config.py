#!/usr/bin/env python3
"""Inspect or generate an explicit build configuration without modifying source."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
GENERATOR = ROOT / 'scripts/kconfig/generate_config.py'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    generate = commands.add_parser('generate', help='Generate a resolved bundle in a build directory')
    generate.add_argument('--build-dir', type=Path, required=True)
    generate.add_argument('--config', type=Path, required=True)
    generate.add_argument('--set', action='append', default=[])
    validate = commands.add_parser('validate', help='Strictly validate a fragment and its dependencies')
    validate.add_argument('--config', type=Path, required=True)
    validate.add_argument('--set', action='append', default=[])
    info = commands.add_parser('info', help='Inspect an existing resolved build bundle')
    info.add_argument('--build-dir', type=Path, required=True)
    diff = commands.add_parser('diff', help='Compare two explicit configurations')
    diff.add_argument('old_config', type=Path)
    diff.add_argument('new_config', type=Path)
    args = parser.parse_args()
    if args.command == 'info':
        directory = args.build_dir.resolve() / 'generated'
        for name in ('effective.config', 'nexus_config.h', 'config.cmake'):
            path = directory / name
            if not path.is_file():
                print(f'Missing build artifact: {path}', file=sys.stderr)
                return 1
            print(path)
        return 0
    if args.command == 'diff':
        import difflib
        try:
            old = args.old_config.read_text().splitlines(keepends=True)
            new = args.new_config.read_text().splitlines(keepends=True)
            print(''.join(difflib.unified_diff(old, new, fromfile=str(args.old_config), tofile=str(args.new_config))), end='')
            return 0
        except OSError as error:
            print(error, file=sys.stderr)
            return 1
    if args.command == 'validate':
        with tempfile.TemporaryDirectory(prefix='nexus-validate-') as work:
            return generate_bundle(Path(work), args.config, args.set)
    directory = args.build_dir.resolve()
    if directory == ROOT or ROOT.is_relative_to(directory):
        parser.error('--build-dir must be a dedicated build directory, not the source root or its ancestor')
    return generate_bundle(directory / 'generated', args.config, args.set)


def generate_bundle(directory, config, settings):
    directory.mkdir(parents=True, exist_ok=True)
    command = [sys.executable, str(GENERATOR), '--kconfig', str(ROOT / 'Kconfig'),
               '--output', str(directory / 'nexus_config.h'),
               '--effective-config', str(directory / 'effective.config'),
               '--cmake-output', str(directory / 'config.cmake')]
    if config:
        command += ['--config', str(config.resolve())]
    for setting in settings:
        command += ['--set', setting]
    return subprocess.run(command, cwd=ROOT).returncode


if __name__ == '__main__':
    sys.exit(main())
