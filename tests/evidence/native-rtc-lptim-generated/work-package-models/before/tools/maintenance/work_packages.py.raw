#!/usr/bin/env python3
"""Derive review work packages from existing mode, SDK and test authorities.

This read-only navigation report adds no support policy, source build graph,
test runner or evidence importer. Source selection and reviewed assertion anchors
never establish executed software or physical qualification.
"""

import argparse
import copy
import hashlib
import importlib.util
import json
from pathlib import Path, PurePosixPath
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.maintenance import capabilities  # noqa: E402

CATALOGUE = "tools/testing/io_lifecycle_matrix.json"
MAX_CATALOGUE_BYTES = 2 * 1024 * 1024
MAX_SOURCE_BYTES = 16 * 1024 * 1024


def bounded_bytes(path, maximum=MAX_SOURCE_BYTES):
    """Read retained regular bytes without following links or unlimited input."""
    from tools.configure.metadata import regular
    path = regular(path)
    if not 0 < path.stat().st_size <= maximum:
        raise ValueError("Input size is outside the work package limit: " + str(path))
    with path.open("rb") as stream:
        data = stream.read(maximum + 1)
    if not 0 < len(data) <= maximum:
        raise ValueError("Input size changed beyond the work package limit")
    return data


def source_name(name):
    """Require canonical checkout-relative navigation, without path aliases."""
    if (not isinstance(name, str) or not name or "\\" in name or "\0" in name
            or PurePosixPath(name).is_absolute()
            or any(part in {".", ".."} for part in name.split("/"))
            or PurePosixPath(name).as_posix() != name):
        raise ValueError("Canonical relative source path required")
    return name


def catalogue_shape(data):
    """Bound navigation input; the existing validator owns assertion semantics."""
    required = {"schema", "scope", "properties", "unproven"}
    if (not isinstance(data, dict) or not required <= data.keys()
            or set(data) - required - {"contract_sources"}
            or type(data["schema"]) is not int or data["schema"] != 1
            or not isinstance(data["scope"], str) or not data["scope"]
            or not isinstance(data["properties"], list)
            or not 1 <= len(data["properties"]) <= 4096
            or not isinstance(data["unproven"], list)
            or not 1 <= len(data["unproven"]) <= 4096):
        raise ValueError("Invalid bounded lifecycle catalogue shape")
    sources = []
    for row in data["properties"]:
        if not isinstance(row, dict):
            raise ValueError("Lifecycle properties must be objects")
        for key, value in row.items():
            if key in {"production_sources", "model_sources", "assertions", "calls"}:
                if (not isinstance(value, list) or len(value) > 4096
                        or any(not isinstance(item, str) or not item for item in value)):
                    raise ValueError("Lifecycle property requires bounded string arrays")
            elif not isinstance(value, str) or not value:
                raise ValueError("Lifecycle property requires nonempty strings")
        for key in ("production_sources", "model_sources"):
            values = row.get(key, [])
            if len(set(values)) != len(values):
                raise ValueError("Duplicate lifecycle source path")
            sources.extend(source_name(name) for name in values)
        if "test_source" in row:
            sources.append(source_name(row["test_source"]))
    contracts = data.get("contract_sources", [])
    if (not isinstance(contracts, list) or len(contracts) > 4096
            or any(not isinstance(name, str) for name in contracts)
            or len(set(contracts)) != len(contracts)):
        raise ValueError("Invalid lifecycle contract source array")
    sources.extend(source_name(name) for name in contracts)
    for row in data["unproven"]:
        if (not isinstance(row, dict) or set(row) != {"scope", "reason"}
                or any(not isinstance(value, str) or not value for value in row.values())):
            raise ValueError("Explicit unproven scope and reason required")
    return set(sources)


def reviewed_catalogue(root, catalogue):
    """Reuse real test bodies, assertion anchors and owned-source validation."""
    from scripts.ci.io_lifecycle_matrix import catalogue_inputs
    from tools.configure.metadata import parse_json
    root, catalogue = Path(root).absolute(), Path(catalogue).absolute()
    if not catalogue.is_relative_to(root):
        raise ValueError("Lifecycle catalogue must be a source checkout input")
    raw = bounded_bytes(catalogue, MAX_CATALOGUE_BYTES)
    try:
        navigation = parse_json(raw)
    except RecursionError as error:
        raise ValueError("Lifecycle catalogue nesting exceeds parser limits") from error
    sources = catalogue_shape(navigation)
    observed = {name: bounded_bytes(root / name) for name in sources}
    data, sources, _ = catalogue_inputs(root, catalogue)
    if raw != bounded_bytes(catalogue, MAX_CATALOGUE_BYTES) or any(
            contents != bounded_bytes(root / name) for name, contents in observed.items()):
        raise ValueError("Lifecycle catalogue inputs changed during navigation")
    return data, sources


def derive(rows, properties, providers, sdk_required):
    """Join exact scopes without aliases, inferred coverage or pass markers."""
    packages = []
    identities = set()
    mapped = set()
    for row in sorted(rows, key=lambda item: (
            item["family"], item["controller"], item["mode"])):
        family, kind, mode = row["family"], row["kind"], row["mode"]
        identity = ".".join((family, row["controller"], mode))
        if identity in identities:
            raise ValueError("Duplicate derived work package: " + identity)
        identities.add(identity)
        if family not in providers:
            raise ValueError("No maintained provider contract: " + family)
        sources, fixtures = set(row["exports"]), set(row["fixtures"])
        if bool(sources) != bool(fixtures):
            raise ValueError("Selected source requires a resolver fixture: " + identity)
        missing = sources - set(sdk_required)
        if missing:
            raise ValueError("Selected source missing from SDK required contract: "
                             + ", ".join(sorted(missing)))
        matching = []
        for property_row in properties:
            scope = (property_row["family"], property_row["peripheral"],
                     property_row["mode"])
            if (scope == (family, kind, mode)
                    and sources.intersection(property_row["production_sources"])):
                matching.append(copy.deepcopy(property_row))
                mapped.add(property_row["id"])
        packages.append({
            "identity": identity, "family": family,
            "controller": row["controller"], "kind": kind, "mode": mode,
            "provider": copy.deepcopy(providers[family]),
            "reviewed_routes": sorted(set(row["routes"])),
            "declared_boards": sorted(set(row["boards"])),
            "resolver_fixtures": sorted(fixtures),
            "selected_ops_sources": sorted(sources),
            "sdk_required_sources": sorted(sources),
            "implementation_status": ("source_selected" if sources
                                      else "not_selected_by_fixture"),
            "contract_status": "reviewed_bounded" if matching else "not_catalogued",
            "reviewed_properties": sorted(matching, key=lambda item: item["id"]),
            "software_status": "not_executed", "hardware_status": "not_executed",
        })
    if not packages:
        raise ValueError("No maintained mode work packages")
    unmapped = sorted((copy.deepcopy(row) for row in properties
                       if row["id"] not in mapped), key=lambda item: item["id"])
    return {
        "schema_version": 1, "identity_kind": "derived-work-package-plan",
        "status": "derived_consistent", "software_qualification": False,
        "physical_qualification": False,
        "scope": "Exact mode and selected ops source navigation; not a complete "
                 "CMake source graph, SDK verification or execution record.",
        "work_packages": packages,
        "unmapped_properties": unmapped,
        "summary": {
            "work_packages": len(packages),
            "source_selected": sum(bool(row["selected_ops_sources"])
                                   for row in packages),
            "with_reviewed_contract": sum(bool(row["reviewed_properties"])
                                          for row in packages),
            "unmapped_properties": len(unmapped),
            "software_executed": 0, "hardware_executed": 0,
        },
        "limitations": [
            "Matching properties retain their original controller-independent "
            "bounded scope; they do not prove every selected fixture or route.",
            "Unmapped properties include Native/common and different aggregate "
            "mode scopes; aliases and nearby hardware are never inferred.",
            "not_catalogued means no exact lifecycle catalogue association; "
            "other tests may exist and are not assessed here.",
            "Software and hardware remain not_executed in this plan regardless "
            "of historical reports; use existing qualification and HIL admission.",
        ],
    }


def provider_contracts():
    """Project declared integration inputs; do not verify installed SDK bytes."""
    from tools.configure.providers import descriptor
    return {item.family: {
        "configuration_modules": list(item.configuration_modules),
        "fact_files": list(item.fact_files),
        "git_dependencies": [{"role": role, "path": path}
                             for role, path in item.git_dependencies],
        "import_lock": item.import_lock,
        "dependency_identity_status": "declared_unverified",
    } for item in descriptor.families() if item.family != "native"}


def collect(root=ROOT):
    """Validate current authorities and derive the report without executing tests."""
    root = Path(root).resolve()
    if root != ROOT.resolve():
        raise ValueError("Use the source checkout's own work package tool")
    from tools.configure import configure, matrix
    from tools.configure.providers import maintained

    specification = importlib.util.spec_from_file_location(
        "nexus_work_package_sdk_contract",
        root / "cmake/package/package_source_sdk.py")
    if specification is None or specification.loader is None:
        raise ValueError("Maintained Source SDK contract is unavailable")
    package_source_sdk = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(package_source_sdk)
    contracts = matrix.check_contracts(root)
    rows = capabilities.collect(root)
    catalogue, reviewed_sources = reviewed_catalogue(root, root / CATALOGUE)
    report = derive(rows, catalogue["properties"], provider_contracts(),
                    set(package_source_sdk.REQUIRED_FILES))
    inputs = set(reviewed_sources) | {
        CATALOGUE, "tools/maintenance/work_packages.py",
        "tools/maintenance/capabilities.py", "tools/maintenance/source_catalogue.py",
        "tools/configure/providers/descriptor.py", "tools/configure/matrix.py",
        "cmake/package/package_source_sdk.py", "scripts/ci/io_lifecycle_matrix.py",
        "CMakeLists.txt", ".gitmodules",
    }
    for package in report["work_packages"]:
        provider = maintained(package["family"])
        package["provider_implementation"] = provider.mode_implementation(
            package["kind"], package["mode"])
        for name in package["selected_ops_sources"]:
            if not package_source_sdk.selected(name):
                raise ValueError("Selected ops source excluded from SDK payload: " + name)
        contract = package["provider"]
        inputs.update(contract["configuration_modules"])
        inputs.update(contract["fact_files"])
        inputs.update(package["resolver_fixtures"])
        inputs.update(package["selected_ops_sources"])
        if contract["import_lock"] is not None:
            inputs.add(contract["import_lock"])
    inputs.update(path.relative_to(root).as_posix()
                  for path in (root / "boards").glob("*/board.json"))
    inputs.update(name for name in package_source_sdk.REQUIRED_FILES
                  if name.endswith(".lock.json"))
    fixtures = sorted({name for package in report["work_packages"]
                       for name in package["resolver_fixtures"]})
    configurations = []
    expected_inputs = {}
    for name in fixtures:
        resolved = configure.resolve(root / name, root)
        configurations.append({"assembly": name,
                               "configuration_sha256": resolved["configuration_sha256"],
                               "inputs": copy.deepcopy(resolved["inputs"])})
        for item in resolved["inputs"]:
            source_name(item["path"])
            prior = expected_inputs.setdefault(item["path"], item["sha256"])
            if prior != item["sha256"]:
                raise ValueError("Fixture input changed between resolutions")
            inputs.add(item["path"])
    identities = []
    for name in sorted(inputs):
        source_name(name)
        data = bounded_bytes(root / name)
        digest = hashlib.sha256(data).hexdigest()
        if name in expected_inputs and digest != expected_inputs[name]:
            raise ValueError("Fixture input changed before report publication")
        identities.append({"path": name, "size": len(data),
                           "sha256": digest})
    report["authority_inputs"] = identities
    report["fixture_configurations"] = configurations
    report["contracts"] = contracts
    report["authority_input_scope"] = (
        "Report authorities and linked review files, not complete Git, resolved "
        "configuration or dependency closure provenance.")
    report["unproven"] = copy.deepcopy(catalogue["unproven"])
    return report


def write_report(root, output, report):
    """Write a new derived artifact without replacing source or prior evidence."""
    root, output = Path(root).resolve(), Path(output).absolute()
    target = output.resolve()
    if output != target or any(path.is_symlink() for path in (output, *output.parents)):
        raise ValueError("Work package output must be canonical without symlinks")
    if target.is_relative_to(root) and not target.is_relative_to(root / "build"):
        raise ValueError("Work package output must not replace source authorities")
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("x", encoding="utf-8") as stream:
        stream.write(json.dumps(report, indent=2, sort_keys=True) + "\n")


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(arguments)
    try:
        report = collect()
        if args.output is None:
            print(json.dumps(report, indent=2, sort_keys=True))
        else:
            write_report(ROOT, args.output, report)
            print(f"Derived {len(report['work_packages'])} mode work packages; "
                  "software and hardware execution are not assessed")
    except (OSError, ValueError, TypeError, KeyError) as error:
        print(f"Work package derivation rejected: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
