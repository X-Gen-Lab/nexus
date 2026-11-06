"""Apply one reviewed source diff without external tools, offsets or fuzz.

Callers verify the pinned upstream identity before applying a patch and retain
responsibility for atomic output publication. Patch paths name repository-owned
inputs; file headers are descriptive and never select filesystem destinations.
"""

from pathlib import Path
import re

HUNK = re.compile(r"@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@[^\r\n]*[\r\n]*")


def apply_reviewed_patch(source: str, patch: Path) -> str:
    """Require exact old/new coordinates, full context and declared counts."""
    lines = patch.read_bytes().decode("utf-8").splitlines(keepends=True)
    if len(lines) < 3 or not lines[0].startswith("--- ") or \
            not lines[1].startswith("+++ "):
        raise ValueError("reviewed patch requires one complete file diff")
    original = source.splitlines(keepends=True)
    result = []
    source_cursor = 0
    cursor = 2
    while cursor < len(lines):
        match = HUNK.fullmatch(lines[cursor])
        if match is None:
            raise ValueError("reviewed patch contains an invalid hunk")
        old_start, old_count, new_start, new_count = (
            int(value) if value is not None else 1 for value in match.groups())
        old_index = old_start - (old_count != 0)
        new_index = new_start - (new_count != 0)
        if old_index < source_cursor or old_index > len(original):
            raise ValueError("reviewed patch old position is out of order")
        result.extend(original[source_cursor:old_index])
        if new_index != len(result):
            raise ValueError("reviewed patch new position does not match")
        source_cursor = old_index
        cursor += 1
        consumed = produced = 0
        while cursor < len(lines) and not lines[cursor].startswith("@@ "):
            line = lines[cursor]
            operation = line[:1]
            if operation not in {" ", "+", "-"}:
                raise ValueError("reviewed patch has an invalid operation")
            content = line[1:]
            if operation in {" ", "-"}:
                if source_cursor >= len(original) or \
                        original[source_cursor] != content:
                    raise ValueError("reviewed patch context does not match")
                source_cursor += 1
                consumed += 1
            if operation in {" ", "+"}:
                result.append(content)
                produced += 1
            cursor += 1
        if consumed != old_count or produced != new_count or \
                consumed + produced == 0:
            raise ValueError("reviewed patch hunk counts do not match")
    result.extend(original[source_cursor:])
    return "".join(result)
