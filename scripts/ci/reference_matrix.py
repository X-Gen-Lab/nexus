"""Derive firmware reference jobs and optional HIL preparation from CMake presets."""

import json
from pathlib import Path
import re
import sys
import tomllib

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools.dev.presets import Presets


def reference_matrix(root):
    root = root.resolve()
    presets = Presets(root)
    jobs = []
    for name, (entry, origin) in presets.rows["configure"].items():
        if not origin.is_relative_to(root / "cmake/presets") or entry.get("hidden"):
            continue
        if not presets.files[origin].get("vendor", {}).get("nexus/reference-jobs", False):
            continue
        if not re.fullmatch(r"[a-zA-Z0-9_-]+", name):
            raise ValueError("Invalid reference preset name: " + name)
        preset = presets.selected("configure", name)
        directory = Path(preset["binaryDir"])
        if not directory.is_relative_to(root) or directory == root:
            raise ValueError(f"Reference build directory must be inside the project: {directory}")
        build_dir = directory.relative_to(root).as_posix()
        # The upload-artifact action interprets its paths as glob expressions.
        if any(char in build_dir for char in "\r\n*?[]!"):
            raise ValueError(f"Unsupported artifact directory: {directory}")
        source = Path(preset["cacheVariables"]["NEXUS_ASSEMBLY_FILE"])
        assembly = tomllib.loads(source.read_text())
        board = (root / "boards" / assembly["board"] if "board" in assembly
                 else source.parent / assembly["board_package"]).resolve()
        board.relative_to(root)
        if not (board / "board.json").is_file():
            raise ValueError(f"Missing board for reference preset {name}: {board}")
        fixture = root / "tools/hil/fixtures" / (board.name + ".json")
        station = ""
        if board.parent == root / "boards" and fixture.is_file():
            facts = json.loads((board / "board.json").read_text())
            plan = json.loads(fixture.read_text())
            if (plan["board"], plan["part"]) != (facts["id"], facts["soc"]):
                raise ValueError(f"HIL fixture differs from reference board: {fixture}")
            station = fixture.relative_to(root).as_posix()
        jobs.append({"preset": name, "build_dir": build_dir, "fixture": station})
    if not jobs:
        raise ValueError("No public firmware reference presets selected")
    return {"include": jobs}


if __name__ == "__main__":
    print("matrix=" + json.dumps(reference_matrix(ROOT), separators=(",", ":")))
