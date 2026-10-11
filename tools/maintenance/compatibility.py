#!/usr/bin/env python3
"""Check compatibility metadata against existing source authorities.

The policy declares supported source interface generations. It never selects
hardware, migrates an input, or establishes a binary ABI or qualification.
"""
from __future__ import annotations
import argparse
import ast
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.configure.metadata import (  # noqa: E402
    POLICY, fields, identity, load_policy, parse_json, policy_contract,
    read_json, regular, snapshot,
)


def validate(policy, observations, expected_factory_epoch=None):
    policy_contract(policy, expected_factory_epoch)
    if observations["factory_abi_epoch"] != policy["factory_abi_epoch"]:
        raise ValueError("Actual supported source factory epoch differs from policy")
    if observations["semantic_version"] != policy["semantic_version"]:
        raise ValueError("Actual project semantic version differs from policy")
    if observations["schemas"] != policy["schemas"]:
        raise ValueError("Actual schema epochs differ from reviewed policy")
    if observations["source_sdk_exact_only"] is not True:
        raise ValueError("Actual source SDK package must require exact version")
    return {"status": "consistent", "semantic_version": policy["semantic_version"],
            "factory_abi_epoch": policy["factory_abi_epoch"],
            "binary_abi_supported": False, "physical_qualification": False,
            "software_candidate_qualification": False}


def one(values, context):
    values = set(values)
    if len(values) != 1:
        raise ValueError(f"Cannot identify one actual {context} authority")
    return values.pop()


def integer_call(tree, label):
    values = []
    for node in ast.walk(tree):
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and node.func.id == "integer" and len(node.args) == 4:
            if isinstance(node.args[3], ast.Constant) and node.args[3].value == label:
                low, high = node.args[1:3]
                if isinstance(low, ast.Constant) and isinstance(high, ast.Constant) and type(low.value) is int and low.value == high.value:
                    values.append(low.value)
    return one(values, label)


def dict_schema(tree, field, required_keys):
    values = []
    for node in ast.walk(tree):
        if not isinstance(node, ast.Dict):
            continue
        keys = [key.value if isinstance(key, ast.Constant) else None for key in node.keys]
        if field in keys and set(required_keys).issubset(keys):
            value = node.values[keys.index(field)]
            if isinstance(value, ast.Constant) and type(value.value) is int:
                values.append(value.value)
    return one(values, field + " dictionary")


def guard_schema(tree, variable):
    values = []
    for node in ast.walk(tree):
        if not isinstance(node, ast.Compare) or len(node.ops) != 1 or not isinstance(node.ops[0], ast.NotEq):
            continue
        left = node.left
        if isinstance(left, ast.Subscript) and isinstance(left.value, ast.Name) and left.value.id == variable and isinstance(left.slice, ast.Constant) and left.slice.value == "schema_version":
            right = node.comparators[0]
            if isinstance(right, ast.Constant) and type(right.value) is int:
                values.append(right.value)
    return one(values, variable + " schema guard")


def package_exact(template, version):
    """Execute the actual version template, including equal-version ranges."""
    return package_exact_text(snapshot(template)[0].decode("utf-8"), version)


def package_exact_text(text, version):
    text = text.replace("@NEXUS_SDK_VERSION@", version)
    if "@" in text:
        raise ValueError("Unexpected unresolved source SDK version template")
    lines = []
    for requested, is_range, expected in ((version, False, True),
                                          (version, True, False),
                                          (version + ".1", False, False),
                                          (str(int(version.split(".")[0]) + 1) + ".0.0", False, False)):
        lines.extend([f'set(PACKAGE_FIND_VERSION "{requested}")',
                      f'set(PACKAGE_FIND_VERSION_RANGE "{"range" if is_range else ""}")',
                      text,
                      f'if(NOT "${{PACKAGE_VERSION_COMPATIBLE}}" STREQUAL "{"TRUE" if expected else "FALSE"}")',
                      '  message(FATAL_ERROR "Source SDK must require exact version")',
                      'endif()'])
    with tempfile.TemporaryDirectory(prefix="nexus-version-check-") as directory:
        script = Path(directory) / "check.cmake"
        script.write_text("\n".join(lines) + "\n")
        try:
            result = subprocess.run(["cmake", "-P", str(script)], capture_output=True, text=True, timeout=20)
        except (OSError, subprocess.SubprocessError) as error:
            raise ValueError("Cannot execute source SDK version check: " + str(error)) from error
    if result.returncode:
        raise ValueError("Source SDK exact-version execution rejected: " + result.stderr)
    return True


def observe(root=ROOT):
    """Inspect real validators/emitters; assertions never configure the platform."""
    root = Path(root).absolute()
    inputs = []
    def source(relative):
        data, observed = snapshot(root / relative)
        inputs.append(observed)
        return data.decode("utf-8")
    project = re.sub(r"#[^\n]*", "", source("CMakeLists.txt"))
    version = one(re.findall(r"\bproject\s*\(\s*nexus\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)\b", project, re.IGNORECASE), "CMake project version")
    authored = ast.parse(source("tools/configure/authored.py"))
    configured = ast.parse(source("tools/configure/configure.py"))
    runtime = ast.parse(source("tools/configure/runtime.py"))
    package = ast.parse(source("cmake/package/package_source_sdk.py"))
    factory = ast.parse(source("tools/configure/factory.py"))
    editor = parse_json(source("tools/configure/assembly.schema.json"))
    author_epoch = integer_call(authored, "assembly schema")
    if editor["properties"]["schema"]["const"] != author_epoch:
        raise ValueError("Editor schema differs from actual authoring validator")
    schemas = {"assembly_authoring": author_epoch,
               "assembly_projection": dict_schema(authored, "schema_version", {"board_package"}),
               "typed_ir": dict_schema(configured, "schema_version", {"board", "soc_family"}),
               "board": integer_call(configured, "Board schema"),
               "soc": integer_call(configured, "SoC schema"),
               "routes": integer_call(configured, "routes schema"),
               "runtime_assembly": guard_schema(runtime, "choices"),
               "cpu_facts": guard_schema(runtime, "facts"),
               "runtime_resolved": dict_schema(runtime, "schema_version", {"cpu_profile"}),
               "source_sdk_manifest": dict_schema(package, "schema", {"kind", "source_revision"})}
    for pattern, name in (("boards/*/board.json", "board"), ("soc/*/soc.json", "soc"), ("soc/*/routes.json", "routes")):
        paths = sorted(root.glob(pattern))
        if not paths:
            raise ValueError(f"Missing actual {name} facts")
        for path in paths:
            data = parse_json(source(path.relative_to(root)))
            if type(data.get("schema_version")) is not int or data["schema_version"] != schemas[name]:
                raise ValueError(f"Actual {name} fact schema differs: {path}")
    factory_epochs = [node.value.value for node in factory.body
                      if isinstance(node, ast.Assign) and
                      any(isinstance(target, ast.Name) and target.id == "FACTORY_ABI_EPOCH"
                          for target in node.targets) and
                      isinstance(node.value, ast.Constant) and type(node.value.value) is int]
    factory_epoch = one(factory_epochs, "supported source factory epoch")
    template = source("cmake/package/NexusConfigVersion.cmake.in")
    return {"semantic_version": version, "factory_abi_epoch": factory_epoch, "schemas": schemas,
            "source_sdk_exact_only": package_exact_text(template, version), "inputs": inputs}


def check(root=ROOT, expected_factory_epoch=None, generated=None, sdk=None):
    root = Path(root).absolute()
    policy_data, policy_identity = snapshot(root / POLICY)
    policy = policy_contract(parse_json(policy_data))
    observations = observe(root)
    report = validate(policy, observations, expected_factory_epoch)
    report["inputs"] = [policy_identity, *observations["inputs"]]
    if generated is not None:
        content_data, generated_identity = snapshot(generated)
        content = content_data.decode("utf-8")
        epoch = one(re.findall(r"^#define\s+NEXUS_FACTORY_ABI_EPOCH\s+([0-9]+)U?\s*$", content, re.MULTILINE), "generated factory epoch")
        version = one(re.findall(r'^#define\s+NEXUS_PLATFORM_VERSION\s+"([^"]+)"\s*$', content, re.MULTILINE), "generated platform version")
        if int(epoch) != policy["factory_abi_epoch"]:
            raise ValueError("Generated factory epoch differs from policy")
        if version != policy["semantic_version"]:
            raise ValueError("Generated platform version differs from policy")
        report["inputs"].append(generated_identity)
    if sdk is not None:
        spec = importlib.util.spec_from_file_location("nexus_compat_package", regular(root / "cmake/package/package_source_sdk.py"))
        module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
        try:
            manifest_identity = identity(Path(sdk) / ".nexus-source-sdk.json")
            installed = module.verify(Path(sdk).absolute())
            sdk_policy_data, sdk_policy_identity = snapshot(Path(sdk) / POLICY)
            if policy_contract(parse_json(sdk_policy_data)) != policy:
                raise ValueError("Installed source SDK compatibility policy differs")
            sdk_observations = observe(Path(sdk))
            validate(policy, sdk_observations, expected_factory_epoch)
        except (OSError, ValueError, TypeError, KeyError) as error:
            raise ValueError("Installed source SDK verification failed: " + str(error)) from error
        report["installed_sdk_revision"] = installed["source_revision"]
        report["inputs"].extend([manifest_identity, sdk_policy_identity,
                                 *sdk_observations["inputs"]])
    for reference in report["inputs"]:
        if identity(reference["path"]) != reference:
            raise ValueError("Compatibility input changed during observation")
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--expected-factory-epoch", type=int)
    parser.add_argument("--generated", type=Path)
    parser.add_argument("--sdk", type=Path)
    args = parser.parse_args(argv)
    try:
        report = check(args.root, args.expected_factory_epoch, args.generated, args.sdk)
    except (OSError, ValueError, KeyError, TypeError, SyntaxError, subprocess.SubprocessError) as error:
        print(f"Compatibility rejected: {error}", file=sys.stderr)
        return 1
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
