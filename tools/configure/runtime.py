#!/usr/bin/env python3
"""Resolve one external CPU runtime assembly without inventing a SoC or board."""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import tomllib
from types import MappingProxyType

try:
    from . import cpu
except ImportError:
    import cpu


class RuntimeError(ValueError):
    """The assembly or output ownership contract was rejected."""


@dataclass(frozen=True)
class RuntimeIR:
    profile: object
    backend: str
    optimization: str
    clock_hz: int
    input_sha256: str
    configuration_sha256: str
    inputs: object
    snapshots: object


def _path(path):
    result = Path(os.path.abspath(path))
    if any(part.is_symlink() for part in (result, *result.parents)):
        raise RuntimeError(f"Runtime path must not contain a symlink: {result}")
    return result


def _fields(value, fields, context):
    if not isinstance(value, dict):
        raise RuntimeError(f"{context}: object required")
    missing, unknown = fields - value.keys(), value.keys() - fields
    if missing or unknown:
        raise RuntimeError(f"{context}: missing {sorted(missing)}, unknown {sorted(unknown)}")


def resolve(assembly):
    """Return immutable validated input identities and CPU contracts."""
    try:
        assembly = _path(assembly)
        authored = assembly.read_bytes()
        choices = tomllib.loads(authored.decode("utf-8"))
        _fields(choices, {"schema_version", "cpu_facts", "backend", "optimization"},
                "Runtime assembly")
        if type(choices["schema_version"]) is not int or choices["schema_version"] != 1:
            raise RuntimeError("Unsupported runtime assembly schema")
        relative = choices["cpu_facts"]
        if not isinstance(relative, str) or not relative or Path(relative).is_absolute():
            raise RuntimeError("cpu_facts must be a nonempty assembly-relative path")
        facts_path = _path(assembly.parent / relative)
        content = facts_path.read_bytes()
        facts = json.loads(content, object_pairs_hook=cpu.unique_keys)
        _fields(facts, {"schema_version", "cpu", "irq", "clock_hz"}, "CPU package")
        if type(facts["schema_version"]) is not int or facts["schema_version"] != 1:
            raise RuntimeError("Unsupported CPU facts schema")
        hz = facts["clock_hz"]
        if type(hz) is not int or not 1 <= hz <= 0xFFFFFFFF:
            raise RuntimeError("CPU clock_hz must be a positive uint32 fact")
        if choices["backend"] not in {"baremetal", "freertos"}:
            raise RuntimeError("Unsupported runtime backend")
        if choices["optimization"] not in {"O2", "Os", "O3"}:
            raise RuntimeError("Unsupported runtime optimization")
        profile = cpu.resolve(facts["cpu"], facts["irq"], choices["backend"])
        digest = hashlib.sha256()
        for label, data in ((b"assembly", authored), (b"cpu-facts", content)):
            digest.update(label + b"\0" + len(data).to_bytes(8, "big") + data)
        inputhash = digest.hexdigest()
        record = {"schema_version": 1, "cpu_profile": profile.to_dict(),
                  "clock_hz": hz, "optimization": choices["optimization"],
                  "input_sha256": inputhash}
        configuration = hashlib.sha256(json.dumps(
            record, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
        return RuntimeIR(profile, choices["backend"], choices["optimization"], hz,
                         inputhash, configuration,
                         MappingProxyType({"assembly": str(assembly),
                                           "cpu_facts": str(facts_path)}),
                         MappingProxyType({str(assembly): authored,
                                           str(facts_path): content}))
    except (OSError, ValueError, TypeError, KeyError) as error:
        if isinstance(error, RuntimeError):
            raise
        raise RuntimeError(str(error)) from error


def configure(assembly, output):
    """Invalidate only owned output, then publish one complete fresh bundle."""
    temporary = None
    try:
        assembly, output = _path(assembly), _path(output)
        if output == assembly or output in assembly.parents:
            raise RuntimeError("Runtime output must not replace an input or source parent")
        # Check input containment before deleting any old owned bundle. A
        # marker proves output ownership, never permission to erase inputs.
        try:
            authored = tomllib.loads(assembly.read_text(encoding="utf-8"))
            relative = authored.get("cpu_facts")
        except (OSError, ValueError):
            relative = None
        if isinstance(relative, str):
            source = Path(os.path.abspath(assembly.parent / relative)).resolve()
            if output == source or output in source.parents:
                raise RuntimeError("Runtime output must not replace input facts or their parent")
        if output.exists():
            marker = output / ".nexus-runtime-bundle"
            if not output.is_dir() or not marker.is_file() or marker.is_symlink():
                raise RuntimeError("Existing runtime output is not an owned bundle")
            shutil.rmtree(output)
        result = resolve(assembly)
        for source in result.inputs.values():
            if output == Path(source) or output in Path(source).parents:
                raise RuntimeError("Runtime output must not replace input facts or their parent")
        output.parent.mkdir(parents=True, exist_ok=True)
        temporary = Path(tempfile.mkdtemp(prefix=".nexus-runtime-", dir=output.parent))
        emitted = cpu.emit_profile(result.profile, temporary,
                                   clock_hz=result.clock_hz,
                                   optimization=result.optimization,
                                   inputhash=result.input_sha256)
        if emitted["configuration_sha256"] != result.configuration_sha256:
            raise RuntimeError("Resolved runtime identity changed during emission")
        with (temporary / "selection.cmake").open("a", encoding="utf-8") as stream:
            stream.write('set(NEXUS_SOC_FAMILY "cpu-runtime")\n')
        (temporary / "input_paths.json").write_text(
            json.dumps(dict(result.inputs), indent=2, sort_keys=True) + "\n",
            encoding="utf-8")
        (temporary / ".nexus-runtime-bundle").write_text(
            "Nexus authored CPU runtime bundle\n", encoding="utf-8")
        for source, snapshot in result.snapshots.items():
            if _path(source).read_bytes() != snapshot:
                raise RuntimeError("Runtime inputs changed during generation")
        os.replace(temporary, output)
        return result
    except (OSError, ValueError, TypeError, KeyError) as error:
        if isinstance(error, RuntimeError):
            raise
        raise RuntimeError(str(error)) from error
    finally:
        if temporary is not None and temporary.exists():
            shutil.rmtree(temporary)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assembly", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        configure(args.assembly, args.output)
        return 0
    except RuntimeError as error:
        print(f"Runtime configuration rejected: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
