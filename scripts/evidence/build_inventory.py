#!/usr/bin/env python3
"""Generate build provenance and a CycloneDX SBOM from map-linked archives."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
from pathlib import Path
import re
import subprocess
import sys
import uuid
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.evidence.common import (EvidenceError, atomic_json, command, digest, fields,
                                     file_identity, identifier, load_json, regular_file,
                                     run_adapter, sha256_value, verify_file_identity)
from scripts.ci.vendor_import_identity import reviewed_import, locked_files


def git(root: Path, *args: str) -> str:
    try:
        result = subprocess.run(["git", "-C", str(root), *args], check=True,
                                stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                stderr=subprocess.DEVNULL, timeout=20)
    except (OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired) as error:
        raise EvidenceError("Git source identity unavailable") from error
    return result.stdout.decode("utf-8", errors="strict").strip()


def source_identity(root: Path) -> dict:
    root = root.resolve()
    revision = git(root, "rev-parse", "HEAD")
    if not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise EvidenceError("source revision must be a full commit id")
    top = Path(git(root, "rev-parse", "--show-toplevel")).resolve()
    if top != root:
        raise EvidenceError("source_root must be a Git repository root")
    paths = git(root, "ls-files", "-z").split("\0")
    content_hash = hashlib.sha256()
    tracked_files = 0
    gitlinks = []
    missing_files = []
    for path in sorted(filter(None, paths)):
        file = root / path
        if file.is_dir():
            # A Git submodule is represented by its pinned gitlink in the tree;
            # actual linked dependency identities are recorded separately below.
            gitlinks.append(path)
            continue
        if not file.exists():
            # Intentional working-tree deletions are representable in an
            # analysis inventory, but dirty builds can never be promoted.
            missing_files.append(path)
            content_hash.update(path.encode("utf-8") + b"\0MISSING\0")
            continue
        with regular_file(file, nonempty=False).open("rb") as stream:
            content_digest = hashlib.file_digest(stream, "sha256").hexdigest()
        content_hash.update(path.encode("utf-8") + b"\0" + content_digest.encode() + b"\0")
        tracked_files += 1
    if tracked_files == 0:
        raise EvidenceError("empty source inventory")
    return {"root": str(root), "commit": revision, "tree": git(root, "rev-parse", "HEAD^{tree}"),
            "tracked_content_sha256": content_hash.hexdigest(), "tracked_files": tracked_files,
            "dirty": bool(git(root, "status", "--porcelain", "--untracked-files=normal")),
            "gitlinks": gitlinks, "missing_files": missing_files}


def component_source(kind: str, root: Path, repository: Path) -> dict:
    if kind == "git":
        return source_identity(root)
    if kind != "vendor-source-import":
        raise EvidenceError("unsupported component source_kind")
    try:
        identity = reviewed_import(root, repository)
        lock = load_json(root / "source.lock.json")
        licenses = sorted({entry["license"] for entry in locked_files(lock)})
    except (ValueError, OSError, TypeError, KeyError) as error:
        raise EvidenceError("vendor import differs from its reviewed source/lock/notices") from error
    return {**identity, "root": str(root.absolute()), "repository_root": str(repository.resolve()),
            "dirty": False, "licenses": licenses}


def license_expression(value: str) -> str:
    """Accept single identifiers or an explicit conjunction, without licensing inference."""
    if not isinstance(value, str) or len(value) > 1024:
        raise EvidenceError("invalid component license expression")
    terms = value.split(" AND ")
    for term in terms:
        identifier(term, "license identifier")
    if len(terms) != len(set(terms)):
        raise EvidenceError("duplicate component license term")
    return value


def validate_component_origin(component: dict) -> None:
    source = component["source"]
    if not isinstance(source, dict):
        raise EvidenceError("component source identity must be an object")
    if source.get("kind", "git") != "vendor-source-import":
        return
    if component["version"] != source["upstream"]["version"]:
        raise EvidenceError("vendor import component version differs from its source lock")
    if set(component["license"].split(" AND ")) != set(source["licenses"]):
        raise EvidenceError("vendor import license expression must include every locked source license")
    root = Path(source["root"])
    notice_paths = {str(root / notice["path"]) for notice in source["notices"]
                    if notice["path"].startswith("LICENSES/")}
    if component["license_file"]["path"] not in notice_paths:
        raise EvidenceError("vendor import license_file must be one of its reviewed license notices")


def sbom_component(component: dict) -> dict:
    source = component["source"]
    expression = component["license"]
    licenses = ([{"expression": expression}] if " AND " in expression or expression.startswith("LicenseRef-")
                else [{"license": {"id": expression}}])
    properties = [{"name": "nexus:license-file:sha256", "value": component["license_file"]["sha256"]}]
    if source.get("kind", "git") == "vendor-source-import":
        properties.extend([{"name": "nexus:source:kind", "value": "vendor-source-import"},
                           {"name": "nexus:source:owning-repository-commit", "value": source["source_commit"]},
                           {"name": "nexus:source:lock:sha256", "value": source["lock_sha256"]},
                           {"name": "nexus:source:upstream-archive:sha256", "value": source["upstream"]["download_sha256"]},
                           {"name": "nexus:source:nested-archive:sha256", "value": source["upstream"]["nested_archive_sha256"]}])
        properties.extend({"name": "nexus:source:notice:" + notice["path"] + ":sha256", "value": notice["sha256"]}
                          for notice in source["notices"])
    else:
        properties.insert(0, {"name": "nexus:source:commit", "value": source["commit"]})
    result = {"type": "library", "name": component["name"], "version": component["version"],
              "bom-ref": component["name"], "licenses": licenses,
              "hashes": [{"alg": "SHA-256", "content": component["archive"]["sha256"]}],
              "properties": properties}
    if source.get("kind", "git") == "vendor-source-import":
        result["externalReferences"] = [{"type": "distribution", "url": source["upstream"]["source_url"]}]
    return result


def test_summary(path: Path) -> dict:
    file = regular_file(path)
    try:
        root = ET.parse(file).getroot()
    except ET.ParseError as error:
        raise EvidenceError("test report is not valid JUnit XML") from error
    if root.tag not in ("testsuite", "testsuites"):
        raise EvidenceError("JUnit testsuite(s) required")
    cases = root.findall(".//testcase")
    if not cases:
        raise EvidenceError("zero executed tests rejected")
    names = [(case.get("classname", ""), case.get("name", "")) for case in cases]
    if any(not name for _, name in names) or len(names) != len(set(names)):
        raise EvidenceError("test identities missing or duplicated")
    failed = sum(bool(case.findall("failure") or case.findall("error")) for case in cases)
    skipped = sum(bool(case.findall("skipped")) for case in cases)
    for suite in [root, *root.findall(".//testsuite")]:
        for counter in ("failures", "errors", "skipped", "disabled"):
            if counter in suite.attrib:
                try:
                    declared = int(suite.attrib[counter])
                except ValueError as error:
                    raise EvidenceError("invalid JUnit summary counter") from error
                if declared != 0:
                    raise EvidenceError("JUnit summary reports failures, errors, skips or disabled tests")
        if "tests" in suite.attrib:
            try:
                declared_count = int(suite.attrib["tests"])
            except ValueError as error:
                raise EvidenceError("invalid JUnit test count") from error
            if declared_count != len(suite.findall(".//testcase")):
                raise EvidenceError("JUnit summary test count differs from executed cases")
    if failed or skipped:
        raise EvidenceError("failed, errored or skipped tests block build evidence")
    # Require an explicit source/effective-config binding from the test runner.
    # A random passing XML file alone must not promote an unrelated firmware.
    return {**file_identity(file), "format": "junit", "tests": len(cases),
            "failed": failed, "skipped": skipped}


def link_evidence(archive: Path, map_file: Path) -> list[dict]:
    """Require actual archive member references, not merely a LOAD declaration."""
    needles = (str(archive) + "(", archive.name + "(")
    matches = []
    for line_number, line in enumerate(map_file.read_text(errors="strict").splitlines(), 1):
        if any(needle in line for needle in needles):
            matches.append({"line": line_number, "archive_member": line.strip()[:1024]})
            if len(matches) == 16:
                break
    if not matches:
        raise EvidenceError(f"no linked archive member found in map: {archive.name}")
    return matches


def generate(manifest_path: Path, output_path: Path, sbom_path: Path) -> dict:
    manifest = load_json(manifest_path)
    fields(manifest, {"schema_version", "source_root", "product", "configuration",
                      "toolchain", "artifacts", "link_map", "components", "tests"})
    if manifest["schema_version"] != 1:
        raise EvidenceError("unsupported build inventory schema")
    fields(manifest["product"], {"id", "board_profile", "board_revision", "osal"})
    for key, value in manifest["product"].items():
        identifier(value, "product " + key)
    fields(manifest["configuration"], {"effective", "header", "cmake"})
    configuration = {key: file_identity(path) for key, path in manifest["configuration"].items()}
    fields(manifest["toolchain"], {"path", "version_args"})
    compiler = regular_file(manifest["toolchain"]["path"])
    arguments = manifest["toolchain"]["version_args"]
    if not isinstance(arguments, list) or not arguments or any(not isinstance(arg, str) for arg in arguments):
        raise EvidenceError("toolchain version argv required")
    compiler_output = run_adapter(command([str(compiler), *arguments]), 10)
    if not compiler_output.strip():
        raise EvidenceError("toolchain returned no version identity")
    toolchain = {**file_identity(compiler),
                 "version": compiler_output.decode("utf-8", errors="strict").splitlines()[0],
                 "version_output_sha256": hashlib.sha256(compiler_output).hexdigest()}
    source = source_identity(Path(manifest["source_root"]))
    artifacts = []
    roles = set()
    if not isinstance(manifest["artifacts"], list) or not manifest["artifacts"]:
        raise EvidenceError("nonempty artifact list required")
    for artifact in manifest["artifacts"]:
        fields(artifact, {"path", "role"})
        role = identifier(artifact["role"], "artifact role")
        if role in roles:
            raise EvidenceError("artifact roles must be unique")
        roles.add(role)
        artifacts.append({**file_identity(artifact["path"]), "role": role})
    if "elf" not in roles:
        raise EvidenceError("final linked ELF artifact required")
    elf_artifact = next(artifact for artifact in artifacts if artifact["role"] == "elf")
    with Path(elf_artifact["path"]).open("rb") as stream:
        if stream.read(4) != b"\x7fELF":
            raise EvidenceError("final linked artifact is not ELF")
    link_map = regular_file(manifest["link_map"])
    if not isinstance(manifest["components"], list) or not manifest["components"]:
        raise EvidenceError("linked components must be inventoried")
    components = []
    names = set()
    for component in manifest["components"]:
        fields(component, {"name", "version", "license", "license_file", "source_root", "archive"}, {"source_kind"})
        name = identifier(component["name"], "component name")
        if name in names:
            raise EvidenceError("component identities must be unique")
        names.add(name)
        version = identifier(component["version"], "component version")
        license_id = license_expression(component["license"])
        archive = regular_file(component["archive"])
        components.append({"name": name, "version": version, "license": license_id,
                           "license_file": file_identity(component["license_file"]),
                           "source": component_source(component.get("source_kind", "git"),
                                                      Path(component["source_root"]), Path(source["root"])),
                           "archive": file_identity(archive),
                           "link_evidence": link_evidence(archive, link_map)})
        validate_component_origin(components[-1])
    fields(manifest["tests"], {"junit", "source_commit", "config_sha256", "artifact_sha256"})
    test_binding = manifest["tests"]
    sha256_value(test_binding["config_sha256"])
    sha256_value(test_binding["artifact_sha256"])
    elf = next(artifact for artifact in artifacts if artifact["role"] == "elf")
    if (test_binding["source_commit"] != source["commit"] or
            test_binding["config_sha256"] != configuration["effective"]["sha256"] or
            test_binding["artifact_sha256"] != elf["sha256"]):
        raise EvidenceError("test report identity does not match this build")
    tests = {**test_summary(Path(test_binding["junit"])),
             "source_commit": source["commit"], "config_sha256": test_binding["config_sha256"],
             "artifact_sha256": elf["sha256"]}
    observed_archives = sorted(set(re.findall(r"([^\s()]+\.a)\(", link_map.read_text(errors="strict"))))
    declared_archives = {Path(component["archive"]["path"]).name for component in components}
    unresolved_archives = [archive for archive in observed_archives if Path(archive).name not in declared_archives]
    # Coverage is explicit: system/dynamic libraries need a toolchain/runtime
    # inventory before enterprise promotion. A partial SBOM is never called complete.
    coverage = {"scope": "map_linked_static_archives", "observed_archives": observed_archives,
                "unresolved_archives": unresolved_archives, "dynamic_runtime_review": "pending"}
    inventory = {"schema_version": 1, "kind": "build_inventory", "source": source,
                 "product": manifest["product"], "configuration": configuration,
                 "toolchain": toolchain, "artifacts": artifacts,
                 "link_map": file_identity(link_map), "components": components,
                 "tests": tests, "dependency_coverage": coverage,
                 "input_manifest_sha256": digest(manifest_path)}
    sbom = {"bomFormat": "CycloneDX", "specVersion": "1.5", "version": 1,
            "serialNumber": "urn:uuid:" + str(uuid.uuid4()),
            "metadata": {"timestamp": datetime.now(timezone.utc).isoformat(),
                         "component": {"type": "application", "name": manifest["product"]["id"],
                                       "version": source["commit"], "bom-ref": "nexus-product"},
                         "properties": [{"name": "nexus:configuration:sha256", "value": configuration["effective"]["sha256"]},
                                        {"name": "nexus:source:tree", "value": source["tree"]}]},
            "components": [sbom_component(component) for component in components],
            "dependencies": [{"ref": "nexus-product", "dependsOn": sorted(names)}]}
    atomic_json(sbom_path, sbom)
    inventory["sbom"] = file_identity(sbom_path)
    atomic_json(output_path, inventory)
    return inventory


def verify(inventory: dict) -> None:
    fields(inventory, {"schema_version", "kind", "source", "product", "configuration", "toolchain",
                       "artifacts", "link_map", "components", "tests", "dependency_coverage", "input_manifest_sha256", "sbom"})
    if inventory["schema_version"] != 1 or inventory["kind"] != "build_inventory":
        raise EvidenceError("invalid build inventory document")
    sha256_value(inventory["input_manifest_sha256"])
    if not inventory["artifacts"] or not inventory["components"]:
        raise EvidenceError("empty build inventory")
    fields(inventory["configuration"], {"effective", "header", "cmake"})
    fields(inventory["product"], {"id", "board_profile", "board_revision", "osal"})
    for key, value in inventory["product"].items():
        identifier(value, "product " + key)
    for identity in inventory["configuration"].values():
        verify_file_identity(identity)
    roles = set()
    for identity in inventory["artifacts"]:
        verify_file_identity(identity)
        role = identifier(identity.get("role"), "artifact role")
        if role in roles:
            raise EvidenceError("duplicate artifact role in inventory")
        roles.add(role)
    if "elf" not in roles:
        raise EvidenceError("linked ELF missing from inventory")
    elf = next(artifact for artifact in inventory["artifacts"] if artifact["role"] == "elf")
    with Path(elf["path"]).open("rb") as stream:
        if stream.read(4) != b"\x7fELF":
            raise EvidenceError("final linked artifact is not ELF")
    verify_file_identity(inventory["link_map"])
    verify_file_identity(inventory["sbom"])
    toolchain = {key: inventory["toolchain"][key] for key in ("path", "sha256", "size")}
    verify_file_identity(toolchain)
    current = source_identity(Path(inventory["source"]["root"]))
    if current != inventory["source"]:
        raise EvidenceError("source identity changed since inventory creation")
    map_file = Path(inventory["link_map"]["path"])
    names = set()
    for component in inventory["components"]:
        fields(component, {"name", "version", "license", "license_file", "source", "archive", "link_evidence"})
        identifier(component["name"], "component name")
        identifier(component["version"], "component version")
        license_expression(component["license"])
        if component["name"] in names:
            raise EvidenceError("duplicate linked component identity")
        names.add(component["name"])
        verify_file_identity(component["archive"])
        verify_file_identity(component["license_file"])
        if not isinstance(component["source"], dict):
            raise EvidenceError("component source identity must be an object")
        current_component = component_source(component["source"].get("kind", "git"),
                                             Path(component["source"]["root"]), Path(current["root"]))
        if current_component != component["source"]:
            raise EvidenceError("linked dependency source identity changed")
        validate_component_origin(component)
        if link_evidence(Path(component["archive"]["path"]), map_file) != component["link_evidence"]:
            raise EvidenceError("linked dependency map evidence changed")
    test_record = inventory["tests"]
    fields(test_record, {"path", "sha256", "size", "format", "tests", "failed", "skipped",
                        "source_commit", "config_sha256", "artifact_sha256"})
    verify_file_identity({key: test_record[key] for key in ("path", "sha256", "size")})
    observed_tests = test_summary(Path(test_record["path"]))
    if any(observed_tests[key] != test_record[key] for key in ("format", "tests", "failed", "skipped")):
        raise EvidenceError("executed test count changed")
    if (test_record["source_commit"] != inventory["source"]["commit"] or
            test_record["config_sha256"] != inventory["configuration"]["effective"]["sha256"] or
            test_record["artifact_sha256"] != elf["sha256"]):
        raise EvidenceError("inventory test binding differs from source/config/ELF")
    observed_archives = sorted(set(re.findall(r"([^\s()]+\.a)\(", map_file.read_text(errors="strict"))))
    declared = {Path(component["archive"]["path"]).name for component in inventory["components"]}
    expected_coverage = {"scope": "map_linked_static_archives", "observed_archives": observed_archives,
                         "unresolved_archives": [archive for archive in observed_archives if Path(archive).name not in declared],
                         "dynamic_runtime_review": "pending"}
    if inventory["dependency_coverage"] != expected_coverage:
        raise EvidenceError("dependency coverage differs from actual linked map")
    sbom = load_json(inventory["sbom"]["path"])
    if sbom.get("bomFormat") != "CycloneDX" or sbom.get("specVersion") != "1.5":
        raise EvidenceError("CycloneDX SBOM identity invalid")
    main_component = sbom.get("metadata", {}).get("component", {})
    if (main_component.get("name") != inventory["product"]["id"] or
            main_component.get("version") != inventory["source"]["commit"]):
        raise EvidenceError("SBOM product/source identity differs from inventory")
    expected_properties = [{"name": "nexus:configuration:sha256", "value": inventory["configuration"]["effective"]["sha256"]},
                           {"name": "nexus:source:tree", "value": inventory["source"]["tree"]}]
    if (sbom.get("metadata", {}).get("properties") != expected_properties or
            sbom.get("dependencies") != [{"ref": "nexus-product", "dependsOn": sorted(names)}]):
        raise EvidenceError("SBOM configuration/source/dependency binding differs from inventory")
    expected_components = [sbom_component(component) for component in inventory["components"]]
    if sbom.get("components") != expected_components:
        raise EvidenceError("SBOM component hashes/licenses differ from inventory")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="operation", required=True)
    create = sub.add_parser("create")
    create.add_argument("--manifest", type=Path, required=True)
    create.add_argument("--output", type=Path, required=True)
    create.add_argument("--sbom", type=Path, required=True)
    check = sub.add_parser("verify")
    check.add_argument("--inventory", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.operation == "create":
            result = generate(args.manifest, args.output, args.sbom)
            print(f"inventory created: {len(result['components'])} linked components, {result['tests']['tests']} executed tests")
        else:
            verify(load_json(args.inventory))
            print("inventory identity verified")
        return 0
    except (EvidenceError, OSError, KeyError, TypeError, UnicodeError) as error:
        print(f"inventory rejected: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
