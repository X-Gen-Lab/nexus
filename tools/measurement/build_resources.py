"""Internal CMake entry for a projection bound by the configured build graph."""

import argparse
from functools import partial
import hashlib
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools.configure import dependencies, sdk
from tools.evidence.common import EvidenceError, digest, file_identity, load_json
from tools.evidence.identity import resolved_identity, resolved_input_files
from tools.measurement import resources


def fingerprint(path, allow_reference):
    files = (path, *(path.parent / name for name in (
        "input_paths.json", "configuration_code_paths.json", "generator-identity.json")),
        *(ROOT / name for name in (
            "tools/evidence/identity.py", "tools/evidence/common.py",
            "tools/measurement/elf.py", "tools/measurement/resources.py",
            "tools/measurement/build_resources.py")))
    values = [str(allow_reference), *(digest(item) for item in files)]
    return hashlib.sha256("\n".join(values).encode()).hexdigest()


def check_generator(path):
    paths = load_json(path.parent / "configuration_code_paths.json")
    identity = load_json(path.parent / "generator-identity.json")
    records = identity["files"]
    if not records or {item["path"] for item in records} != set(paths):
        raise EvidenceError("Configuration generator inputs differ from their identities")
    try:
        dependencies.verify(paths, identity)
    except ValueError as error:
        raise EvidenceError(str(error)) from error
    return paths


def bind(path, *, allow_reference):
    """Replay authoring once, before embedding the digest in the Ninja command."""
    before = fingerprint(path, allow_reference)
    selected, _ = resolved_identity(path, allow_reference=allow_reference)
    paths = check_generator(path)
    # A rewritten generator receipt cannot remove or redirect private sources.
    owned, _ = dependencies.snapshot(mode="platform", family=selected["soc_family"],
        sdk=sdk.installed(ROOT), test_support=selected["soc_family"] == "native",
        backend=selected["backend"])
    declared = {item["file"]["path"] for item in resolved_input_files(path, selected)}
    if (any(paths.get(label) != origin for label, origin in owned.items()) or
            any(not label.startswith("soc-package/") or paths[label] not in declared
                for label in paths.keys() - owned.keys())):
        raise EvidenceError("Configuration generator paths differ from the selected sources")
    if fingerprint(path, allow_reference) != before:
        raise EvidenceError("Configuration changed while binding the firmware graph")
    return before


def from_graph(path, expected, *, allow_reference):
    """The expected digest comes from the build command, outside the bundle."""
    if fingerprint(path, allow_reference) != expected:
        raise EvidenceError("Configured firmware projection or checker changed")
    selected, artifact = load_json(path), file_identity(path)
    check_generator(path)
    declared = resolved_input_files(path, selected)
    if fingerprint(path, allow_reference) != expected:
        raise EvidenceError("Configuration changed during firmware measurement")
    return selected, artifact, declared


def main(argv):
    if argv[:1] == ["--bind"]:
        parser = argparse.ArgumentParser(description=bind.__doc__, allow_abbrev=False)
        parser.add_argument("--bind", type=Path, required=True)
        parser.add_argument("--allow-reference", action="store_true")
        args = parser.parse_args(argv)
        try:
            print(bind(args.bind, allow_reference=args.allow_reference))
        except (EvidenceError, OSError, KeyError, TypeError) as error:
            parser.exit(1, str(error) + "\n")
        return 0
    if not argv:
        raise SystemExit("Supply the configured firmware projection digest or --bind")
    return resources.main(argv[1:], read_inputs=partial(from_graph, expected=argv[0]))


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
