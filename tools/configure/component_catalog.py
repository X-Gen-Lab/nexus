"""Shared component IDs and leaf entries; CMake owns all software dependencies."""

from dataclasses import dataclass
import json
from pathlib import Path
import re
from types import MappingProxyType


ROOT = Path(__file__).resolve().parents[2]
CATALOG = "components/catalog.json"


@dataclass(frozen=True)
class Component:
    """One logical choice with an optional leaf preparation entry."""
    entry: str
    target: str


def parse(value):
    """Reject executable/source-graph fields and ambiguous logical identities."""
    if (not isinstance(value, dict) or
            set(value) != {"schema_version", "components"} or
            type(value["schema_version"]) is not int or
            value["schema_version"] != 1 or
            not isinstance(value["components"], list)):
        raise ValueError("Invalid component catalogue")
    selected = {}
    for row in value["components"]:
        if (not isinstance(row, dict) or set(row) != {"id", "entry", "target"} or
                not isinstance(row["id"], str) or
                re.fullmatch(r"[a-z][a-z0-9-]*", row["id"]) is None or
                row["id"] in selected or
                not isinstance(row["target"], str) or
                re.fullmatch(r"Nexus::[A-Za-z][A-Za-z0-9_]*", row["target"]) is None or
                not isinstance(row["entry"], str)):
            raise ValueError("Invalid or duplicate component catalogue entry")
        entry = row["entry"]
        if entry and re.fullmatch(
                r"components/(?:[A-Za-z0-9_]+/)+[A-Za-z0-9_]+\.cmake", entry) is None:
            raise ValueError("Invalid component catalogue leaf path")
        selected[row["id"]] = Component(entry, row["target"])
    return MappingProxyType(selected)


def load(root=ROOT):
    """Read the catalogue belonging to this exact source tree."""
    return parse(json.loads((Path(root) / CATALOG).read_bytes()))
