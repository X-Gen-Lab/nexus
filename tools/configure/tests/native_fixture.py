"""Explicit host-fixture scope for configuration tests and their CLI calls."""

import json
from pathlib import Path
import tomllib


def native_fixture(assembly):
    """Keep physical fixtures outside the optional host-model scope."""
    try:
        values = tomllib.loads(Path(assembly).read_text(encoding="utf-8"))
    except (OSError, TypeError, ValueError):
        return False
    if values.get("board") == "native_reference":
        return True
    package = values.get("board_package")
    if isinstance(package, str) and any(
            part in {"native", "native_reference"}
            for part in Path(package).parts):
        return True
    return values.get("backend") == "native"


def with_native_support(arguments):
    """Opt in only when a test command explicitly selects a host fixture."""
    values = list(arguments)
    if "--test-support" in values:
        return values
    for index, value in enumerate(values[:-1]):
        if ((value == "--assembly" and native_fixture(values[index + 1])) or
                (value == "--board" and values[index + 1] == "native_reference")):
            return [*values, "--test-support"]
        if value == "--board-package":
            try:
                board = json.loads((Path(values[index + 1]) /
                                    "board.json").read_text())
            except (OSError, TypeError, ValueError):
                continue
            if board.get("soc_family") == "native":
                return [*values, "--test-support"]
    return values


def canonical_assembly(root, name):
    """Locate a maintained production input or an explicit host test input."""
    directory = ("tests/support/native/assemblies" if name.startswith("native")
                 else "tools/configure/assemblies")
    return Path(root) / directory / name
