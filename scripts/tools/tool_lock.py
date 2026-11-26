"""Read the exact development-tool pins shared by setup and formatting."""

import re

PACKAGES = ("pre-commit", "clang-format", "gersemi")
PIN_PATTERN = re.compile(
    r"(pre-commit|clang-format|gersemi)=="
    r"((?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*))"
)


class ToolLockError(ValueError):
    """The development-tool lock is missing or invalid."""


def read_lock(path):
    """Accept exactly one stable semantic-version pin per allowed package."""
    try:
        contents = path.read_text(encoding="utf-8")
    except OSError as error:
        raise ToolLockError(f"Cannot read tool lock {path}: {error}") from error
    pins = {}
    for number, raw in enumerate(contents.splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        # pip treats an inline comment as such only after whitespace.
        line = re.sub(r"\s+#.*$", "", line).strip()
        match = PIN_PATTERN.fullmatch(line)
        if not match:
            raise ToolLockError(
                f"Invalid tool lock {path}:{number}: expected an allowed name==X.Y.Z pin"
            )
        name, version = match.groups()
        if name in pins:
            raise ToolLockError(f"Duplicate tool pin {name} in {path}:{number}")
        pins[name] = version
    missing = set(PACKAGES) - set(pins)
    if missing:
        raise ToolLockError(f"Tool lock {path} is missing: {', '.join(sorted(missing))}")
    return pins
