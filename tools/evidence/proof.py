"""Revalidate scope-specific evidence before byte-preserving candidate sealing."""

from __future__ import annotations

from pathlib import Path
import shutil
import re
import tempfile

from tools.evidence.common import (EvidenceError, fields, file_identity, load_json,
    regular_file, sha256_value, stream_digest, verify_file_identity)
from tools.evidence.identity import verify_sdk_package
from tools.measurement.elf import Elf32
from tools.measurement.resources import measure_data


def referenced(value) -> list[dict]:
    if isinstance(value, dict):
        if {"path", "sha256", "size"}.issubset(value):
            return [value]
        return [item for child in value.values() for item in referenced(child)]
    if isinstance(value, list):
        return [item for child in value for item in referenced(child)]
    return []


def proof_document(report, kind, resolver):
    documents = {}
    for identity in referenced(report):
        path = resolver(identity)
        if path.stat().st_size > 4 * 1024 * 1024 or not path.read_bytes().lstrip().startswith(b"{"):
            continue
        value = load_json(path)
        if value.get("kind") == kind:
            documents[identity["sha256"]] = (identity, value)
    if len(documents) != 1:
        raise EvidenceError("qualification requires one actual " + kind + " evidence document")
    return next(iter(documents.values()))


def validate_resources(report, manifest, resolved, resolver):
    _, evidence = proof_document(report, "elf_resource_budget", resolver)
    if (evidence.get("status") != "passed" or evidence.get("physical_status") != "not_executed"
            or evidence["elf"]["sha256"] != manifest["artifacts"]["elf"]["sha256"]
            or evidence["resolved"]["sha256"] != manifest["configuration"]["sha256"]):
        raise EvidenceError("resource qualification differs from candidate identity")
    metrics, violations = measure_data(Elf32(resolver(evidence["elf"]).read_bytes()),
        resolved, load_json(resolver(evidence["budget"])))
    if metrics != evidence.get("metrics") or violations or evidence.get("violations") != []:
        raise EvidenceError("resource evidence differs from actual ELF/budget measurement")
    inputs = evidence.get("configuration_inputs", [])
    actual = {entry["label"]: entry["file"]["sha256"] for entry in inputs}
    expected = {entry["path"]: entry["sha256"] for entry in resolved["inputs"]}
    if len(inputs) != len(actual) or actual != expected:
        raise EvidenceError("resource evidence configuration input closure differs")
    for identity in referenced(evidence):
        resolver(identity)
    return referenced(evidence)


def validate_reproduction(report, manifest, resolved, resolver, *, live=True):
    _, evidence = proof_document(report, "offline_clean_rebuild", resolver)
    if (evidence.get("status") != "passed" or evidence.get("source") != manifest["source"]
            or evidence.get("physical_status") != "not_executed"
            or evidence.get("hermetic_status") != "passed_declared_container_boundary"
            or evidence.get("reproducible_status") != "bit_exact_elf_bin"
            or evidence.get("different_artifacts") != []
            or evidence["environment"]["sha256"] != manifest["environment"]["sha256"]):
        raise EvidenceError("actual independent offline reproduction required")
    spec = load_json(resolver(evidence["spec"]))
    if (spec["source"] != manifest["source"] or evidence["toolchain"] != spec["toolchain"]
            or len(spec["commands"]) != 2):
        raise EvidenceError("reproduction specification/input identity differs")
    environment = load_json(resolver(evidence["environment"]))
    runs = evidence.get("runs")
    if not isinstance(runs, list) or len(runs) != 2:
        raise EvidenceError("two independent clean runs required")
    for key in ("source_directory", "output_directory", "source_mount", "output_mount",
                "package_directory", "package_mount"):
        if (not all(isinstance(run.get(key), str) and run[key] for run in runs)
                or runs[0][key] == runs[1][key]):
            raise EvidenceError("reproduction roots/mounts must be independently distinct")
    if runs[0]["source_snapshot"] != runs[1]["source_snapshot"] or not runs[0]["source_snapshot"]["files"]:
        raise EvidenceError("clean source snapshots differ or are empty")
    from tools.evidence.common import command
    if (runs[0]["source_package_snapshot"] != runs[1]["source_package_snapshot"] or
            evidence["source_sdk"]["sha256"] != spec["source_sdk"]["sha256"]):
        raise EvidenceError("declared SDK prefixes differ between independent builds")
    for run in runs:
        package_directory = Path(run["package_directory"])
        if (Path(run["source_directory"]) != package_directory / "share/nexus/src" or
                run["source_mount"] != run["package_mount"] + "/share/nexus/src"):
            raise EvidenceError("source must be inside the verified complete SDK prefix")
        source_directory = Path(run["source_directory"])
        output_directory = Path(run["output_directory"])
        if source_directory == output_directory or source_directory.is_relative_to(output_directory) or output_directory.is_relative_to(source_directory):
            raise EvidenceError("reproduction source/output trees must be independent")
        if live:
            from tools.evidence.reproduce import tree_identity
            if (tree_identity(source_directory) != run["source_snapshot"] or
                    tree_identity(package_directory) != run["source_package_snapshot"]):
                raise EvidenceError("actual clean source SDK snapshot identity changed")
            from tools.evidence.reproduce import verify_source_sdk
            verify_source_sdk(file_identity(source_directory / ".nexus-source-sdk.json"), manifest["source"])
            for role in ("elf", "bin"):
                if resolver(run["artifacts"][role]) != regular_file(output_directory / spec["artifacts"][role]):
                    raise EvidenceError("reproduction artifact leaves the declared clean output")
        if run.get("status") != "passed" or run.get("clean_output") is not True or len(run["commands"]) != 2:
            raise EvidenceError("complete clean build commands required")
        replacements = {"source": run["source_mount"], "build": run["output_mount"],
            "toolchain": "/toolchain", "assembly": run["source_mount"] + "/" + spec["assembly"]}
        for execution, expected in zip(run["commands"], spec["commands"]):
            argv = execution["argv"]
            if (type(execution.get("exit_code")) is not int or execution["exit_code"] != 0
                    or execution.get("failure") or not isinstance(argv, list)):
                raise EvidenceError("failed or absent offline execution rejected")
            if (not isinstance(run.get("container_user"), str) or
                    not re.fullmatch(r"[0-9]+:[0-9]+", run["container_user"]) or
                    "--user" not in argv or argv[argv.index("--user") + 1] != run["container_user"]):
                raise EvidenceError("recorded bind-mount execution user differs from actual argv")
            if ("--network" not in argv or argv[argv.index("--network") + 1] != "none"
                    or "--read-only" not in argv or "--cap-drop" not in argv
                    or argv[argv.index("--cap-drop") + 1] != "ALL"):
                raise EvidenceError("offline container boundary absent from actual argv")
            mounts = {argv[index + 1] for index, value in enumerate(argv) if value == "--mount"}
            expected_mounts = {
                f"type=bind,src={run['package_directory']},dst={run['package_mount']},readonly",
                f"type=bind,src={run['output_directory']},dst={run['output_mount']}",
                f"type=bind,src={spec['toolchain']['path']},dst=/toolchain,readonly"}
            expanded = command(expected, replacements)
            if mounts != expected_mounts or argv[-len(expanded):] != expanded:
                raise EvidenceError("actual build argv differs from declared inputs")
            if argv[-len(expanded) - 1] != environment["image_config_id"]:
                raise EvidenceError("offline command used a different OCI image")
            resolver(execution["raw"])
        for role in ("elf", "bin"):
            if run["artifacts"][role]["sha256"] != manifest["artifacts"][role]["sha256"]:
                raise EvidenceError("offline artifact differs from same-artifact candidate")
            resolver(run["artifacts"][role])
    for identity in referenced(evidence):
        resolver(identity)
    return referenced(evidence)


def sdk_file_identity(path):
    """SDK source files may be empty; raw execution evidence may never be empty."""
    path = regular_file(path, nonempty=False)
    with path.open("rb") as stream:
        sha = stream_digest(stream)
    return {"path": str(path), "sha256": sha, "size": path.stat().st_size, "role": "sdk_file"}


def verify_sdk_file(identity):
    fields(identity, {"path", "sha256", "size", "role"})
    if identity["role"] != "sdk_file" or type(identity["size"]) is not int or identity["size"] < 0:
        raise EvidenceError("invalid SDK source file identity")
    sha256_value(identity["sha256"])
    path = regular_file(identity["path"], nonempty=False)
    if sdk_file_identity(path) != identity:
        raise EvidenceError("SDK source payload changed")
    return path


def validate_sdk(report, manifest, resolver, sdk_packages=None):
    identity, sdk = proof_document(report, "nexus-installed-source-sdk", resolver)
    if (sdk.get("publishable") is not True or sdk.get("source_dirty") is not False
            or sdk.get("source_revision") != manifest["source"]["commit"]
            or sdk.get("source_tree") != manifest["source"]["tree"]):
        raise EvidenceError("publishable source SDK must bind the qualified source commit/tree")
    if sdk_packages is None:
        root = resolver(identity).parent
        verify_sdk_package(root)
        closure = {"manifest_sha256": identity["sha256"],
            "files": {name: sdk_file_identity(root / name) for name in sdk["files_sha256"]},
            "package_files": {name: sdk_file_identity(root.parents[2] / name)
                              for name in sdk["package_files_sha256"]}}
    else:
        matched = [item for item in sdk_packages if item["manifest_sha256"] == identity["sha256"]]
        if len(matched) != 1:
            raise EvidenceError("complete sealed source SDK closure required")
        closure = matched[0]
        with tempfile.TemporaryDirectory(prefix="nexus-verify-sdk-") as directory:
            prefix = Path(directory)
            root = prefix / "share/nexus/src"
            root.mkdir(parents=True)
            shutil.copyfile(resolver(identity), root / ".nexus-source-sdk.json")
            for key, target_root in (("files", root), ("package_files", prefix)):
                if set(closure[key]) != set(sdk["files_sha256" if key == "files" else "package_files_sha256"]):
                    raise EvidenceError("sealed SDK source/export file map is incomplete")
                for name, record in closure[key].items():
                    relative = Path(name)
                    if relative.is_absolute() or ".." in relative.parts:
                        raise EvidenceError("SDK payload path escapes its package")
                    target = target_root / relative
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copyfile(resolver(record), target)
            verify_sdk_package(root)
    return closure


def validate_scope_proof(report, manifest, resolved, *, resolver=verify_file_identity,
                         sdk_packages=None):
    scope = report["scope"]
    if scope == "resource_budget":
        return validate_resources(report, manifest, resolved, resolver), None
    if scope == "reproducibility":
        return validate_reproduction(report, manifest, resolved, resolver, live=sdk_packages is None), None
    if scope == "source_sdk":
        closure = validate_sdk(report, manifest, resolver, sdk_packages)
        return [*closure["files"].values(), *closure["package_files"].values()], closure
    return [], None
