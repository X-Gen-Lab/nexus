#!/usr/bin/env python3
"""Compile all owned Git-tracked Python sources without executing them."""
import argparse
import ast
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def tracked_python(root):
    result = subprocess.run(
        ['git', '-C', str(root), 'ls-files', '--stage', '-z', '--', '*.py'],
        check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    paths = []
    for entry in result.stdout.split(b'\0'):
        if not entry:
            continue
        metadata, separator, filename = entry.partition(b'\t')
        if not separator:
            raise ValueError('malformed Git index record')
        fields = metadata.split()
        if len(fields) != 3:
            raise ValueError('malformed Git index metadata')
        mode, _, stage = fields
        if mode == b'160000':
            continue  # Gitlinks are dependencies; never traverse their checkout.
        if mode not in (b'100644', b'100755') or stage != b'0':
            raise ValueError(f'non-regular or unmerged Python index entry: {filename!r}')
        path = Path(filename.decode('utf-8'))
        if path.is_absolute() or '..' in path.parts or path.suffix != '.py':
            raise ValueError(f'invalid tracked Python path: {path}')
        paths.append(path)
    if not paths:
        raise ValueError('zero owned tracked Python sources')
    return sorted(set(paths))


def check_sources(root, paths):
    if not paths:
        raise ValueError('zero owned tracked Python sources')
    errors = []
    for relative in paths:
        path = root / relative
        if path.is_symlink() or not path.is_file():
            errors.append(f'{relative}: missing regular tracked source')
            continue
        try:
            tree = ast.parse(path.read_bytes(), filename=str(relative))
            # AST parsing alone accepts return/break outside their legal scope.
            compile(tree, str(relative), 'exec', dont_inherit=True)
        except (SyntaxError, ValueError, OSError) as error:
            errors.append(f'{relative}: {error}')
    return errors


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    args = parser.parse_args(argv)
    root = args.root.resolve()
    try:
        paths = tracked_python(root)
        errors = check_sources(root, paths)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f'Python syntax gate failed: {error}', file=sys.stderr)
        return 1
    for error in errors:
        print(error, file=sys.stderr)
    print(f'Python syntax: {len(paths)} owned tracked sources, {len(errors)} errors')
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())
