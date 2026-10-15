#!/usr/bin/env python3
"""Check the Conventional Commit header used by the contribution guides."""

import argparse
from pathlib import Path
import re
import sys

HEADER = re.compile(
    r"^(feat|fix|docs|style|refactor|perf|test|build|ci|chore|revert)"
    r"(?:\([A-Za-z0-9_./-]+\))?!?: \S.*$"
)


def valid_message(message):
    """Accept conventional headers and Git-generated merge/revert messages."""
    lines = message.splitlines()
    if not lines:
        return False
    header = lines[0]
    return bool(HEADER.fullmatch(header)) or header.startswith(
        ("Merge branch '", "Merge remote-tracking branch '", 'Revert "')
    )


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("message_file", type=Path)
    args = parser.parse_args(arguments)
    try:
        message = args.message_file.read_text(encoding="utf-8")
        if not valid_message(message):
            print(
                "Commit header must follow <type>(<scope>): <subject>, "
                "for example: fix(hal): preserve buffer until settlement",
                file=sys.stderr,
            )
            return 1
        return 0
    except (OSError, UnicodeError) as error:
        print(f"Cannot check commit message: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
