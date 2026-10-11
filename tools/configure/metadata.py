"""Strict source compatibility policy loader, shared by production emitters.

The JSON policy is the sole source interface generation metadata. This module
never resolves hardware, executes tools, migrates input or promises binary ABI.
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
POLICY = "dependencies/platform-compatibility.json"
SCHEMA_NAMES = {"assembly_authoring", "assembly_projection", "typed_ir", "board",
                "soc", "routes", "runtime_assembly", "cpu_facts",
                "runtime_resolved", "source_sdk_manifest"}
VERSION = re.compile(r"(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)")


def regular(path):
    path = Path(path).absolute()
    if any(part.is_symlink() for part in (path, *path.parents)) or not path.is_file():
        raise ValueError(f"Input must be a regular file without symlinks: {path}")
    return path


def identity(path):
    return snapshot(path)[1]


def snapshot(path):
    path = regular(path)
    data = path.read_bytes()
    return data, {"path": str(path), "sha256": hashlib.sha256(data).hexdigest(),
                  "size": len(data)}


def pairs(items):
    result = {}
    for key, value in items:
        if key in result:
            raise ValueError(f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def read_json(path):
    return parse_json(snapshot(path)[0])


def parse_json(data):
    try:
        return json.loads(data, object_pairs_hook=pairs,
                          parse_constant=lambda value: (_ for _ in ()).throw(
                              ValueError(f"Invalid JSON constant: {value}")))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ValueError(str(error)) from error


def fields(value, names, context):
    if not isinstance(value, dict) or set(value) != set(names):
        raise ValueError(f"{context}: exact fields required")


def policy_contract(policy, expected_factory_epoch=None):
    names = {"schema_version", "kind", "semantic_version",
             "factory_abi_epoch", "source_sdk", "schemas"}
    if isinstance(policy, dict) and "configured_backends" in policy:
        names.add("configured_backends")
    fields(policy, names, "policy")
    if "configured_backends" in policy:
        backends = policy["configured_backends"]
        if (not isinstance(backends, list) or not backends or
                any(not isinstance(name, str) or name not in
                    {"baremetal", "native", "freertos", "threadx"} for name in backends) or
                len(set(backends)) != len(backends)):
            raise ValueError("Unknown or duplicate configured backend names")
    if type(policy["schema_version"]) is not int or policy["schema_version"] != 1:
        raise ValueError("Unsupported compatibility policy schema")
    if policy["kind"] != "nexus-platform-compatibility":
        raise ValueError("Unsupported compatibility policy kind")
    if not isinstance(policy["semantic_version"], str) or not VERSION.fullmatch(policy["semantic_version"]):
        raise ValueError("Canonical semantic version required")
    if type(policy["factory_abi_epoch"]) is not int or not 1 <= policy["factory_abi_epoch"] <= 0xffffffff:
        raise ValueError("Positive uint32 source factory epoch required")
    if policy["source_sdk"] != {"delivery": "source_only",
                                "version_matching": "exact",
                                "binary_abi": "unsupported"}:
        raise ValueError("Source-only exact SDK policy cannot promise binary ABI")
    fields(policy["schemas"], SCHEMA_NAMES, "schema epochs")
    for value in policy["schemas"].values():
        if type(value) is not int or not 1 <= value <= 0xffffffff:
            raise ValueError("Positive uint32 schema epochs required")
    if expected_factory_epoch is not None and (
            type(expected_factory_epoch) is not int or
            expected_factory_epoch != policy["factory_abi_epoch"]):
        raise ValueError("Unknown or incompatible requested source factory epoch")
    return policy


def header_lines(policy):
    policy_contract(policy)
    return [f'#define NEXUS_PLATFORM_VERSION {json.dumps(policy["semantic_version"])}',
            f'#define NEXUS_FACTORY_ABI_EPOCH {policy["factory_abi_epoch"]}U']


def load_policy(root=ROOT, expected_factory_epoch=None):
    return policy_contract(read_json(Path(root) / POLICY), expected_factory_epoch)
