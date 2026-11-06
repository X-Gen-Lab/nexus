"""Revalidate scope-specific evidence before byte-preserving candidate sealing."""

from __future__ import annotations

from pathlib import Path
from functools import lru_cache
import importlib.util
import math
import shutil
from subprocess import CompletedProcess
import sys
import tempfile

from tools.evidence.common import (EvidenceError, fields, file_identity, load_json,
    regular_file, sha256_value, stream_digest, verify_file_identity)
from tools.evidence.identity import verify_sdk_package
from tools.measurement.elf import Elf32
from tools.measurement.resources import measure_data, resolved_float_abi


# These are reviewed checker implementation inputs, not import discovery or a
# firmware build graph. Admission observes their current bytes; it does not
# retrospectively assert that an old process recorded its Python runtime.
CHECKER_SOURCES = {
    "scripts/ci/native_contracts.py": (
        "scripts/ci/native_contracts.py", "scripts/ci/io_lifecycle_matrix.py",
        "scripts/ci/tdd_gate.py", "scripts/validation/__init__.py",
        "scripts/validation/junit.py", "tools/__init__.py",
        "tools/dev/change_paths.py", "tools/dev/presets.py",
        "tools/dev/process.py", "tools/dev/qualification.py",
        "tools/dev/timings.py", "tools/evidence/common.py",
        "tools/evidence/identity.py", "tools/testing/run_tool_tests.py"),
    "tools/dev/dev.py": (
        "tools/__init__.py", "tools/dev/dev.py", "tools/dev/process.py",
        "tools/dev/provenance.py", "tools/dev/presets.py",
        "tools/dev/qualification.py", "tools/evidence/common.py",
        "tools/evidence/identity.py"),
    "tools/hil/prepare.py": (
        "tools/__init__.py", "tools/hil/prepare.py", "tools/hil/admit.py",
        "tools/evidence/common.py", "tools/evidence/identity.py",
        "tools/measurement/resources.py", "tools/measurement/elf.py",
        "tools/configure/__init__.py", "tools/configure/memory.py",
        "tools/configure/uart_formats.py", "tools/configure/calls.py",
        "tools/configure/providers/__init__.py",
        "tools/configure/providers/common.py",
        "tools/configure/providers/descriptor.py",
        "tools/maintenance/source_catalogue.py"),
}


def referenced(value) -> list[dict]:
    if isinstance(value, dict):
        if {"path", "sha256", "size"}.issubset(value):
            return [value]
        return [item for child in value.values() for item in referenced(child)]
    if isinstance(value, list):
        return [item for child in value for item in referenced(child)]
    return []


def verify_execution_stream(identity):
    """Preserve an original runner stream, including an empty stderr."""
    fields(identity, {"path", "sha256", "size", "role"})
    if (identity["role"] != "execution_stream" or
            type(identity["size"]) is not int or identity["size"] < 0):
        raise EvidenceError("invalid execution stream identity")
    sha256_value(identity["sha256"])
    path = regular_file(identity["path"], nonempty=False)
    with path.open("rb") as stream:
        actual = stream_digest(stream)
    if path.stat().st_size != identity["size"] or actual != identity["sha256"]:
        raise EvidenceError("execution stream identity changed")
    return path


def verify_admission_tool(identity, *, resolver=verify_file_identity):
    """Check bytes observed at admission, without claiming launch-time capture."""
    fields(identity, {"path", "sha256", "size", "role", "invoked",
                      "tool_kind", "observation"})
    if (identity["role"] != "admission_tool" or
            identity["tool_kind"] not in ("python", "ctest", "cmake", "git", "ninja") or
            identity["observation"] != "scope_admission" or
            not isinstance(identity["invoked"], str) or not identity["invoked"]):
        raise EvidenceError("invalid admission-observed tool identity")
    return resolver({key: identity[key] for key in ("path", "sha256", "size")})


class ScopeInputs:
    """Resolve a producer's named files through retained byte identities."""

    def __init__(self, report, resolver, proof_inputs):
        self.resolver = resolver
        self.portable = proof_inputs is not None
        self.identities = {}
        self.tools = {}
        pending = referenced(report) + list(proof_inputs or [])
        seen = set()
        while pending:
            identity = pending.pop()
            if identity.get("role") == "admission_tool":
                key = (identity["tool_kind"], identity["invoked"])
                previous_tool = self.tools.setdefault(key, identity)
                if previous_tool != identity:
                    raise EvidenceError("ambiguous admission-observed tool")
            previous = self.identities.setdefault(identity["path"], identity)
            if any(previous[key] != identity[key] for key in ("path", "sha256", "size")):
                raise EvidenceError("ambiguous scope input identity")
            key = (identity["path"], identity["sha256"], identity.get("role"))
            if key in seen:
                continue
            seen.add(key)
            path = self.resolve(identity)
            if (identity.get("role") in ("sdk_file", "execution_stream", "admission_tool") or
                    path.stat().st_size > 4 * 1024 * 1024 or
                    not path.read_bytes().lstrip().startswith(b"{")):
                continue
            pending.extend(referenced(load_json(path)))

    def resolve(self, identity):
        if identity.get("role") == "admission_tool":
            return verify_admission_tool(identity, resolver=self.resolver)
        if self.resolver is verify_file_identity:
            if identity.get("role") == "execution_stream":
                return verify_execution_stream(identity)
            if identity.get("role") == "sdk_file":
                return verify_sdk_file(identity)
        return self.resolver(identity)

    def records(self):
        return [item for item in self.identities.values()
                if item.get("role") != "admission_tool"] + list(self.tools.values())

    def tool(self, invoked, kind):
        """Resolve a prepared tool role now; portable replay uses retained bytes."""
        key = (kind, invoked)
        if self.portable:
            identity = self.tools.get(key)
            if identity is None:
                raise EvidenceError("missing admission-observed " + kind + " tool")
            self.resolve(identity)
            return
        expected = (Path(sys.executable).resolve() if kind == "python" else
                    Path(shutil.which(kind) or "").resolve())
        actual = shutil.which(invoked)
        if actual is None or Path(actual).resolve() != expected:
            label = "Python" if kind == "python" else kind
            raise EvidenceError("trusted " + label + " execution role required")
        identity = {**file_identity(expected), "role": "admission_tool",
                    "invoked": invoked, "tool_kind": kind,
                    "observation": "scope_admission"}
        previous = self.tools.setdefault(key, identity)
        if previous != identity:
            raise EvidenceError("admission-observed tool changed")

    def path(self, original, *, stream=False):
        original = str(Path(original))
        identity = self.identities.get(original)
        if identity is None:
            if self.portable:
                raise EvidenceError("missing retained scope input: " + original)
            if stream:
                file = regular_file(original, nonempty=False)
                with file.open("rb") as handle:
                    sha = stream_digest(handle)
                identity = {"path": str(file), "sha256": sha,
                            "size": file.stat().st_size,
                            "role": "execution_stream"}
            else:
                identity = file_identity(original)
            self.identities[original] = identity
        return self.resolve(identity)

    def json(self, original):
        return load_json(self.path(original))

    def command(self, log, *, argv=None, stage=None, allow_failure=False,
                tool_kind=None):
        """Check the maintained process runner's original completed attempt."""
        log = Path(log)
        view = self.json(log.with_suffix(".command.json"))
        attempt = Path(view.get("attempt_receipt", ""))
        if (attempt.name != "receipt.json" or attempt.parent.parent !=
                log.parent / (log.stem + ".attempts")):
            raise EvidenceError("original unique command attempt required")
        receipt = self.json(attempt)
        if receipt != {key: value for key, value in view.items()
                       if key != "attempt_receipt"}:
            raise EvidenceError("command view differs from original attempt")
        if (type(receipt.get("schema")) is not int or receipt["schema"] != 1 or
                receipt.get("outcome") != "completed" or
                type(receipt.get("exit_code")) is not int or
                (not allow_failure and receipt["exit_code"] != 0) or
                type(receipt.get("child_returncode")) is not int or
                receipt["child_returncode"] != receipt["exit_code"] or
                receipt.get("withdrawal_complete") is not True or
                receipt.get("withdrawal_stable") is not True or
                receipt.get("withdrawn_processes") != []):
            raise EvidenceError("completed owned command required")
        if argv is not None and receipt.get("command") != argv:
            raise EvidenceError("command differs from scope producer plan")
        if tool_kind is not None:
            self.tool(receipt["command"][0], tool_kind)
        if stage is not None and receipt.get("stage") != stage:
            raise EvidenceError("command differs from scope producer stage")
        start, finish = receipt.get("started_ns"), receipt.get("finished_ns")
        duration = receipt.get("duration_seconds")
        if (type(start) is not int or start < 0 or type(finish) is not int or
                finish < start or type(duration) not in (int, float) or
                not math.isfinite(duration) or duration < 0):
            raise EvidenceError("invalid completed execution interval")
        output = []
        for name in ("stdout", "stderr"):
            identity = receipt.get(name)
            fields(identity, {"path", "bytes", "sha256"})
            if (identity["path"] != str(attempt.parent / (name + ".txt")) or
                    type(identity["bytes"]) is not int or identity["bytes"] < 0):
                raise EvidenceError("original execution stream required")
            stream = self.path(identity["path"], stream=True)
            with stream.open("rb") as handle:
                sha = stream_digest(handle)
            if stream.stat().st_size != identity["bytes"] or sha != identity["sha256"]:
                raise EvidenceError("execution stream differs from receipt")
            output.append(stream.read_text(errors="replace"))
        if self.path(log, stream=True).read_text() != "".join(output):
            raise EvidenceError("command log differs from original streams")
        return receipt


def owned_check(report, manifest, inputs, checker, *, arguments=None):
    """Require the qualification's command to execute the selected checker."""
    script = str(Path(manifest["source"]["root"]) / checker)
    selected = []
    for check in report.get("executed_checks", []):
        argv = check.get("argv", [])
        if (len(argv) >= 3 and argv[1] == "-B" and argv[2] == script and
                (arguments is None or argv[3:] == arguments)):
            if (check.get("status") != "passed" or
                    type(check.get("exit_code")) is not int or
                    check["exit_code"] != 0 or check.get("failure") or
                    check.get("raw", {}).get("size", 0) <= 0):
                raise EvidenceError("completed nonempty checker execution required")
            for name in CHECKER_SOURCES.get(checker, (checker,)):
                inputs.path(Path(manifest["source"]["root"]) / name)
            inputs.tool(argv[0], "python")
            inputs.command(check["raw"]["path"], argv=argv)
            selected.append(check)
    if len(selected) != 1:
        raise EvidenceError("one actual maintained scope checker required")
    return selected[0]


def proof_document(report, kind, resolver):
    documents = {}
    for identity in referenced(report):
        if identity.get("role") in ("execution_stream", "sdk_file"):
            continue
        path = resolver(identity)
        if path.stat().st_size > 4 * 1024 * 1024 or not path.read_bytes().lstrip().startswith(b"{"):
            continue
        value = load_json(path)
        if value.get("kind") == kind:
            documents[identity["sha256"]] = (identity, value)
    if len(documents) != 1:
        raise EvidenceError("qualification requires one actual " + kind + " evidence document")
    return next(iter(documents.values()))


def validate_host(report, manifest, resolver, proof_inputs):
    """Replay the complete maintained Native case plan without executing it."""
    from scripts.ci.native_contracts import discovered_cases
    from scripts.ci.tdd_gate import validate_google_report
    from scripts.validation.junit import validate_junit

    identity, summary = proof_document(
        report, "development-contract-execution", resolver)
    inputs = ScopeInputs(report, resolver, proof_inputs)
    preset = summary.get("preset")
    if preset not in ("native-debug", "native-release", "native-asan", "native-tsan"):
        raise EvidenceError("unknown Native producer preset")
    owned_check(report, manifest, inputs, "scripts/ci/native_contracts.py",
                arguments=["--preset", preset])
    fields(summary, {"schema", "kind", "preset", "passed", "suites", "ctest", "scope"})
    build = Path(identity["path"]).parent.parent
    if Path(identity["path"]).name != "execution.json":
        raise EvidenceError("actual Native execution summary required")
    plan = inputs.json(build / "google-contracts.json")
    fields(plan, {"schema", "executables"})
    names = plan["executables"]
    if (type(plan["schema"]) is not int or plan["schema"] != 1 or
            not isinstance(names, list) or not names or
            any(not isinstance(name, str) or not name for name in names) or
            len(set(names)) != len(names)):
        raise EvidenceError("complete unique Google executable plan required")
    binaries = [Path(name) if Path(name).is_absolute() else build / name
                for name in names]
    if (len({path.name for path in binaries}) != len(binaries) or
            any(not path.is_relative_to(build) or ".." in path.parts
                for path in binaries)):
        raise EvidenceError("Google executable plan leaves its build")
    for binary in binaries:
        inputs.path(binary)
    output = build / "tdd"
    plan_log = output / "ctest-plan.txt"
    ctest = inputs.command(plan_log, tool_kind="ctest")
    tool = ctest["command"][0]
    if ctest["command"] != [tool, "--test-dir", str(build), "--show-only=json-v1"]:
        raise EvidenceError("unfiltered complete CTest discovery required")
    import json
    ctest_plan = json.loads(inputs.path(plan_log).read_text())
    tests = ctest_plan.get("tests", [])
    planned = [item.get("name") for item in tests]
    if (not planned or any(not isinstance(name, str) or not name for name in planned)
            or len(planned) != len(set(planned))):
        raise EvidenceError("complete nonzero CTest plan required")
    for test in tests:
        argv = test.get("command", [])
        if (not argv or not isinstance(argv[0], str) or
                not Path(argv[0]).is_relative_to(build) or
                ".." in Path(argv[0]).parts):
            raise EvidenceError("CTest runtime command leaves its build")
        inputs.path(argv[0])
    for binary in binaries:
        registrations = [item["command"] for item in tests
                         if item["command"][0] == str(binary)]
        if registrations != [[str(binary)]]:
            raise EvidenceError("Google executable must run once without filters")
    ctest_path = build / "ctest-results.xml"
    inputs.command(output / "ctest-execution.txt", argv=[
        tool, "--preset", preset, "--output-on-failure", "--no-tests=error",
        "--parallel", "1", "--output-junit", str(ctest_path)])
    actual = validate_junit(inputs.path(ctest_path))
    if (actual["skipped"] or {case["name"] for case in actual["cases"]} !=
            set(planned) or actual != summary["ctest"]):
        raise EvidenceError("CTest execution differs from complete producer plan")
    results = []
    for index, binary in enumerate(binaries):
        log = output / ("discovery-" + str(index) + ".txt")
        inputs.command(log, argv=[str(binary), "--gtest_filter=*", "--gtest_list_tests"])
        discovered = discovered_cases(inputs.path(log).read_text())
        evidence = validate_google_report(inputs.path(
            output / "google" / (binary.name + ".xml")),
            expected_cases=len(discovered))
        if {(case["classname"], case["name"]) for case in evidence["cases"]} != discovered:
            raise EvidenceError("Google execution differs from original case discovery")
        results.append({"binary": binary.name, **evidence})
    if (type(summary["schema"]) is not int or summary["schema"] != 1 or
            summary["suites"] != results or type(summary["passed"]) is not int or
            summary["passed"] != sum(row["passed"] for row in results)):
        raise EvidenceError("Native summary differs from actual nonzero execution")
    from tools.evidence.native_proof import validate_native_snapshot
    validate_native_snapshot(inputs, manifest["source"]["root"], build)
    return inputs.records()


def validate_arm_link(report, manifest, resolved, resolver, proof_inputs):
    """Bind the same linked bytes to an actual maintained build receipt."""
    from tools.measurement.elf import binary_from_elf, image_window
    from tools.measurement.resources import validate_elf_configuration

    _, evidence = proof_document(report, "build_provenance", resolver)
    inputs = ScopeInputs(report, resolver, proof_inputs)
    if (evidence.get("action") != "build" or
            evidence.get("source") != manifest["source"] or
            evidence.get("configuration") != manifest["configuration"] or
            evidence.get("status") != "identities_recorded" or
            evidence.get("software_qualified") is not False):
        raise EvidenceError("same-source configured ARM build provenance required")
    observed = {item["sha256"] for item in evidence.get("artifacts", [])}
    if not {item["sha256"] for item in manifest["artifacts"].values()} <= observed:
        raise EvidenceError("ARM link provenance differs from same-artifact candidate")
    actual = evidence.get("actual_execution", {})
    receipt_identity = actual.get("receipt", {})
    receipt_path = Path(receipt_identity.get("path", ""))
    receipt = inputs.json(receipt_path)
    argv = receipt.get("command", [])
    if (len(argv) < 4 or Path(argv[0]).name not in ("cmake", "cmake.exe") or
            argv[1:3] != ["--build", "--preset"] or
            argv[3].startswith("native-") or
            argv[4:] not in ([], ["--parallel", "1"], ["--parallel=1"])):
        raise EvidenceError("maintained complete ARM build checker required")
    owned_check(report, manifest, inputs, "tools/dev/dev.py", arguments=[
        "build", "--preset", argv[3], *argv[4:]])
    command_log = Path(manifest["source"]["root"]) / "build/quality/developer-command.txt"
    completed = inputs.command(command_log, argv=argv, tool_kind="cmake")
    if receipt_path != Path(inputs.json(command_log.with_suffix(
            ".command.json"))["attempt_receipt"]):
        raise EvidenceError("ARM build differs from the original producer attempt")
    if (completed != {key: value for key, value in receipt.items()
                      if key != "attempt_receipt"} or
            evidence.get("commands") != [argv] or
            actual.get("duration_seconds") != receipt.get("duration_seconds") or
            actual.get("stdout_stderr") != [receipt.get("stdout"), receipt.get("stderr")]):
        raise EvidenceError("ARM build provenance differs from actual execution")
    elf = Elf32(inputs.resolve(manifest["artifacts"]["elf"]).read_bytes(),
                float_abi=resolved_float_abi(resolved))
    validate_elf_configuration(elf, resolved)
    window = image_window(resolved)
    if inputs.resolve(manifest["artifacts"]["bin"]).read_bytes() != binary_from_elf(
            elf, window["origin"], window["origin"] + window["size"]):
        raise EvidenceError("ARM link BIN differs from actual ELF")
    return inputs.records()


def validate_hil_tooling(report, manifest, resolved, resolver, proof_inputs):
    """Recompute tooling preparation; physical execution stays unperformed."""
    from tools.hil.prepare import preparation_plan

    identity, evidence = proof_document(report, "hil_preparation", resolver)
    inputs = ScopeInputs(report, resolver, proof_inputs)
    if evidence.get("resolved") != manifest["configuration"]:
        raise EvidenceError("HIL tooling resolved identity differs from candidate")
    fixture = load_json(inputs.resolve(evidence["fixture"]))
    expected = preparation_plan(fixture, resolved)
    expected.update(fixture=evidence["fixture"], resolved=evidence["resolved"])
    if evidence != expected:
        raise EvidenceError("HIL tooling preparation differs from maintained checker")
    owned_check(report, manifest, inputs, "tools/hil/prepare.py", arguments=[
        "--fixture", evidence["fixture"]["path"], "--resolved",
        evidence["resolved"]["path"], "--report", identity["path"]])
    return inputs.records()


def validate_resources(report, manifest, resolved, resolver):
    _, evidence = proof_document(report, "elf_resource_budget", resolver)
    if (evidence.get("status") != "passed" or evidence.get("physical_status") != "not_executed"
            or evidence["elf"]["sha256"] != manifest["artifacts"]["elf"]["sha256"]
            or evidence["resolved"]["sha256"] != manifest["configuration"]["sha256"]):
        raise EvidenceError("resource qualification differs from candidate identity")
    metrics, violations = measure_data(Elf32(resolver(evidence["elf"]).read_bytes(),
        float_abi=resolved_float_abi(resolved)), resolved,
        load_json(resolver(evidence["budget"])))
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


def validate_reproduction(report, manifest, resolved, resolver, *, live=True,
                          legacy_replay=False):
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
    external = spec.get("schema_version") == 2
    if external:
        from tools.evidence import consumer
        consumer.contract(spec["consumer"])
        if evidence.get("consumer") != spec.get("consumer"):
            raise EvidenceError("external consumer identity differs from declared source")
        if live:
            consumer.verify(spec["consumer"])
        for key in ("consumer_directory", "consumer_mount"):
            runs = evidence.get("runs", [])
            if (len(runs) != 2 or not all(isinstance(run.get(key), str) and
                    run[key] for run in runs) or runs[0][key] == runs[1][key]):
                raise EvidenceError("external consumer roots/mounts must be independently distinct")
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
    from tools.evidence.container import docker_prefix
    from tools.evidence.reproduce import container_argv
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
        if external:
            from tools.evidence.inputs import records_identity, tree_identity
            consumer_directory = Path(run["consumer_directory"])
            for other in (package_directory, output_directory):
                if (consumer_directory == other or consumer_directory.is_relative_to(other)
                        or other.is_relative_to(consumer_directory)):
                    raise EvidenceError("external consumer must be independent of SDK and output")
            files = run.get("consumer_files")
            if (not isinstance(files, dict) or "CMakeLists.txt" not in files or
                    spec["assembly"] not in files):
                raise EvidenceError("complete external consumer source closure required")
            records = []
            for name, identity in files.items():
                consumer.relative_path(name, "consumer source file")
                path = resolver(identity)
                if live and path != regular_file(consumer_directory / name, nonempty=False):
                    raise EvidenceError("external consumer source leaves declared snapshot")
                records.append({"path": name, "size": identity["size"],
                                "sha256": identity["sha256"]})
            expected = spec["consumer"]["snapshot"]
            if (records_identity(records) != expected or
                    run.get("consumer_snapshot") != expected or
                    (live and tree_identity(consumer_directory) != expected)):
                raise EvidenceError("external consumer snapshot differs from declared bytes")
            if (run.get("resolved", {}).get("sha256") != manifest["configuration"]["sha256"] or
                    (live and resolver(run["resolved"]) != regular_file(
                        output_directory / consumer.relative_path(spec["resolved"], "resolved configuration")))):
                raise EvidenceError("external consumer resolved configuration differs from candidate")
        for execution, expected in zip(run["commands"], spec["commands"]):
            argv = execution["argv"]
            if (type(execution.get("exit_code")) is not int or execution["exit_code"] != 0
                    or execution.get("failure") or not isinstance(argv, list)):
                raise EvidenceError("failed or absent offline execution rejected")
            prefix_length = len(docker_prefix())
            if len(argv) < prefix_length + 4:
                raise EvidenceError("complete managed Docker prefix and container name required")
            # The caller may select a Docker executable, but every other prefix
            # token and daemon endpoint is fixed. Exact reconstruction rejects
            # alternate volume syntax, extra options and duplicate flags too.
            docker = argv[prefix_length - 2]
            container_name = argv[prefix_length + 3]
            allowed = [container_argv(spec, run, expected,
                environment["image_config_id"], container_name=container_name,
                docker=docker)]
            if (legacy_replay and spec.get("schema_version") == 1 and
                    "storage_preflight" not in evidence):
                allowed.append(container_argv(spec, run, expected,
                    environment["image_config_id"], container_name=container_name,
                    docker=docker, legacy=True))
            if argv not in allowed:
                raise EvidenceError("actual Docker argv differs from the complete offline boundary")
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


def validate_sdk(report, manifest, resolver, sdk_packages=None, *, legacy_replay=False):
    identity, sdk = proof_document(report, "nexus-installed-source-sdk", resolver)
    if (sdk.get("publishable") is not True or sdk.get("source_dirty") is not False
            or sdk.get("source_revision") != manifest["source"]["commit"]
            or sdk.get("source_tree") != manifest["source"]["tree"]):
        raise EvidenceError("publishable source SDK must bind the qualified source commit/tree")
    if legacy_replay:
        from tools.evidence.legacy_sdk import validate_declared_sdk_closure
        matched = [item for item in sdk_packages or []
                   if item.get("manifest_sha256") == identity["sha256"]]
        if len(matched) != 1:
            raise EvidenceError("complete original legacy SDK closure required")
        return validate_declared_sdk_closure(identity, sdk, matched[0], resolver=resolver)
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


@lru_cache(maxsize=1)
def consumer_checker():
    """Load the maintained local checker, never a script from sealed evidence."""
    path = Path(__file__).resolve().parents[2] / "cmake/package/verify_consumers.py"
    specification = importlib.util.spec_from_file_location("scope_consumer_checker", path)
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    return module


def validate_consumer_inputs(inputs, sdk_identity, sdk_manifest, kind,
                             specification, prefix, source, build, configured,
                             revision, *, fixture):
    """Recreate maintained templates and inspect actual public build inputs."""
    from tools.configure.metadata import POLICY
    from tools.evidence.identity import canonical_digest, resolved_identity

    checker = consumer_checker()
    sdk = prefix / "share/nexus/src"
    names = {POLICY}
    if kind == "selected":
        names.update((specification[1], "cmake/package/verification/io_evolution/consumers/" +
                      specification[2] + ".c"))
    if kind == "services":
        scenario = "cmake/package/verification/" + specification.scenario + "/"
        names.update(name for name in sdk_manifest["files_sha256"] if name.startswith(scenario))
    if not names <= sdk_manifest["files_sha256"].keys():
        raise EvidenceError("consumer canonical SDK template inputs missing")
    with tempfile.TemporaryDirectory(prefix="nexus-consumer-template-") as directory:
        temporary = Path(directory)
        package = temporary / "sdk"
        template_sdk = package / "share/nexus/src"
        for name in names:
            original = sdk / name
            actual = inputs.path(original)
            with actual.open("rb") as stream:
                sha = stream_digest(stream)
            if sha != sdk_manifest["files_sha256"][name]:
                raise EvidenceError("consumer canonical SDK source changed")
            target = template_sdk / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(actual, target)
        output = temporary / "consumer"
        output.mkdir()
        # Preparation only writes templates. Its Git request is compared with
        # the original receipt elsewhere; no archived command is executed.
        ignore_command = lambda argv: None
        if kind == "base":
            name, arm, chip = specification
            expected_source, expected_build, expected = checker.prepare_consumer(
                package, output, name, ignore_command, fixture=fixture, arm=arm,
                chip=chip, expected_revision=revision)
        elif kind == "modern":
            expected_source, expected_build, expected = checker.prepare_modern_consumer(
                package, output, *specification, ignore_command, expected_revision=revision)
        elif kind == "selected":
            expected_source, expected_build, expected = checker.prepare_selected_consumer(
                package, output, *specification, ignore_command, expected_revision=revision)
        else:
            expected_source, expected_build, expected = checker.prepare_service_consumer(
                package, output, specification, ignore_command, fixture=fixture,
                expected_revision=revision)
        substitutions = ((str(expected_source), str(source)),
                         (str(expected_build), str(build)), (str(package), str(prefix)))
        def original_text(value):
            for old, new in substitutions:
                value = value.replace(old, new)
            return value
        if configured != [original_text(value) for value in expected]:
            raise EvidenceError("consumer command differs from its canonical SDK template")
        authored = []
        for path in expected_source.rglob("*"):
            if not path.is_file():
                continue
            original = source / path.relative_to(expected_source)
            expected_bytes = original_text(path.read_text()).encode()
            if inputs.path(original).read_bytes() != expected_bytes:
                raise EvidenceError("consumer source differs from its canonical SDK template")
            if path.suffix in (".c", ".cpp") and "provider" not in path.relative_to(
                    expected_source).parts:
                authored.append(original)
    if inputs.path(build / "resolved-sdk.txt").read_text() != str(sdk):
        raise EvidenceError("consumer did not resolve the declared source SDK")
    config = build / "nexus-sdk/generated"
    bundle = (".nexus-config-bundle", "bindings.c", "configuration_code_paths.json",
              "generator-identity.json", "input_paths.json", "memory.ld",
              "nexus_bindings.h", "nexus_config.h", "nexus_factory.h",
              "nexus_factory_strong.h", "resolved.json", "resource-budget.json",
              "selection.cmake")
    for name in bundle:
        inputs.path(config / name)
    selected, _ = resolved_identity(inputs.path(config / "resolved.json"))
    if inputs.path(config / ".nexus-config-bundle").read_text() != (
            selected["configuration_sha256"] + "\n"):
        raise EvidenceError("consumer generated bundle identity differs")
    for records, navigation in ((selected["inputs"], inputs.json(config / "input_paths.json")),
            (inputs.json(config / "generator-identity.json")["files"],
             inputs.json(config / "configuration_code_paths.json"))):
        if len(records) != len(navigation) or {row["path"] for row in records} != set(navigation):
            raise EvidenceError("consumer generated input closure differs")
        for row in records:
            with inputs.path(navigation[row["path"]]).open("rb") as stream:
                if stream_digest(stream) != row["sha256"]:
                    raise EvidenceError("consumer generated input changed")
    generator = inputs.json(config / "generator-identity.json")
    if generator.get("sha256") != canonical_digest(generator["files"]):
        raise EvidenceError("consumer generator input identity differs")
    rows = __import__("json").loads(inputs.path(build / "compile_commands.json").read_text())
    if kind == "base":
        public_names = {source / "main.c", source / "main.cpp"}
    elif kind == "services":
        scenario = specification.scenario
        if scenario == "external_provider":
            public_names = {source / "consumer.c"}
        elif scenario == "mcuboot_boot":
            public_names = {source / "private_headers.c"}
        else:
            split = scenario in ("secure_services", "loaned_rx", "mcuboot_control")
            public_names = {source / ("application.c" if split else "main.c"),
                            source / "private_headers.c"}
    else:
        public_names = {path for path in authored if path.name != "composition.c"}
    for path in public_names:
        public = [row for row in rows if row.get("file") == str(path)]
        if len(public) != 1 or checker.private_compile_inputs(public[0], sdk,
                (source / "provider/soc", source / "provider/configure")):
            raise EvidenceError("consumer public compile closure differs from actual SDK use")
    bindings = [row for row in rows if row.get("file") == str(config / "bindings.c")]
    if len(bindings) != 1:
        raise EvidenceError("consumer generated bindings lack their unique compile row")


def validate_consumer_observation(inputs, sdk, source, build, kind,
                                  specification, observation, rows, output):
    """Replay exact maintained scenarios with original verified outputs."""
    from tools.evidence.identity import canonical_digest
    from tools.release.transport import selected_domain

    checker = consumer_checker()
    native = kind == "selected"
    if native:
        if selected_domain(specification) != "native-time":
            raise EvidenceError("Native observation requires its canonical domain")
    elif (kind != "services" or
            checker.service_profile(specification).scenario not in {
                "image_window", "mcuboot_control"} or not specification.arm):
        raise EvidenceError("provider-free observation requires its canonical domain")
    image = build / "bin" / ("installed_selected" if native
                             else "installed_services.elf")
    config = build / "nexus-sdk/generated"
    originals = {
        "elf": image, "resolved": config / "resolved.json",
        "bindings": config / "bindings.c",
        "compile_commands": build / "compile_commands.json",
        "sdk_manifest": sdk / ".nexus-source-sdk.json",
        "private_probe": source / "private_headers.c",
    }
    canonical = set()
    if native:
        originals.update({"canonical_source": sdk /
            "cmake/package/verification/io_evolution/consumers" /
            (specification[2] + ".c"),
            "assembly": sdk / specification[1],
            "public_source": source / "main.c"})
    else:
        authored = checker.service_sources(sdk, specification)
        canonical.update(authored.values())
        canonical.add(sdk / "cmake/package/verification" /
                      specification.scenario / specification.assembly)
        originals.update({"assembly": source / "assembly.toml",
            **{"consumer/" + name: source / name for name in authored}})
    with tempfile.TemporaryDirectory(prefix="nexus-consumer-observation-") as directory:
        temporary = Path(directory)
        mappings = ((str(sdk), str(temporary / "sdk")),
                    (str(source), str(temporary / "source")),
                    (str(build), str(temporary / "build")))
        def relocated(value):
            for original, target in mappings:
                value = value.replace(original, target)
            return value
        metadata = {build / "resolved-sdk.txt", build / "resolved-config.txt",
                    build / "compile_commands.json"}
        resource = build / "bin/installed_services.resources.json"
        if not native:
            metadata.add(resource)
        for original in {*originals.values(), *metadata, *canonical}:
            target = Path(relocated(str(original)))
            target.parent.mkdir(parents=True, exist_ok=True)
            actual = inputs.path(original)
            if original in metadata:
                target.write_text(relocated(actual.read_text()))
            else:
                shutil.copyfile(actual, target)
        def replay(argv):
            symbol = ["nm" if native else "arm-none-eabi-nm", "-S",
                      "--defined-only", "--format=posix", str(image)]
            allowed = [[str(image)], symbol] if native else [symbol]
            expected = next((original for original in allowed if
                argv == [relocated(value) for value in original]), None)
            matching = [row for row in rows if row.get("argv") == expected]
            if expected is None or len(matching) != 1:
                raise EvidenceError("unique actual consumer observation command required")
            log = output / matching[0].get("log", "")
            if log.parent != output or ".." in log.parts:
                raise EvidenceError("consumer command leaves its original execution")
            receipt = inputs.command(log, argv=expected,
                                     stage="source-sdk-consumer")
            streams = [inputs.path(receipt[name]["path"], stream=True).read_text(
                errors="replace") for name in ("stdout", "stderr")]
            return CompletedProcess(expected, receipt["exit_code"], *streams)
        try:
            arguments = (Path(relocated(str(sdk))), Path(relocated(str(source))),
                         Path(relocated(str(build))))
            expected = (checker.check_selected_image(*arguments, replay,
                owner_specification=specification) if native else
                checker.check_service_image(*arguments, specification, replay))
        except ValueError as error:
            raise EvidenceError("consumer checker rejected original inputs: " +
                                str(error)) from error
        # Relocation changes metadata paths only. Preserve the identities of
        # the original bytes, including the unmodified compile database.
        identities = {key: {name: inputs.identities[str(original)][name]
                           for name in ("path", "sha256", "size")}
                      for key, original in originals.items()}
        if native:
            expected.update(identities, name=specification[0],
                            assembly=str(sdk / specification[1]))
        else:
            expected["inputs"] = identities
            expected["resources"] = {name: inputs.identities[str(resource)][name]
                                     for name in ("path", "sha256", "size")}
        if canonical_digest(observation) != canonical_digest(expected):
            raise EvidenceError("consumer observation differs from checker replay")


def validate_sdk_consumers(report, manifest, resolver=verify_file_identity,
                           proof_inputs=None, *, consumer_index=None,
                           require_owned=True):
    """Require execution of the full shared consumer policy, beyond packaging."""
    from tools.release.transport import consumer_layout, consumer_plan
    from tools.evidence.identity import resolved_identity

    identity, evidence = proof_document(
        report, "source_sdk_consumer_verification", resolver)
    plan = consumer_plan()
    consumers = evidence.get("consumers", [])
    expected_names = [
        specification.name if kind == "services" else
        specification[0] + " " + specification[1] if kind == "modern" else
        ("selected " if kind == "selected" else "") + specification[0]
        for kind, specification in plan]
    if (consumer_index is not None and
            (type(consumer_index) is not int or not 1 <= consumer_index <= len(plan))):
        raise EvidenceError("selected consumer index is outside the maintained plan")
    indices = list(range(1, len(plan) + 1)) if consumer_index is None else [consumer_index]
    if (evidence.get("physical_status") != "not_executed" or
            evidence.get("source_revision") != manifest["source"]["commit"] or
            evidence.get("source_tree") != manifest["source"]["tree"] or
            len(consumers) != len(plan) or
            any((consumers[index - 1].get("consumer_index"),
                 consumers[index - 1].get("name"),
                 consumers[index - 1].get("status")) !=
                (index, expected_names[index - 1], "passed") for index in indices) or
            (consumer_index is None and (evidence.get("status") != "passed" or
                evidence.get("failures") or evidence.get("first_failure") or
                evidence.get("error")))):
        raise EvidenceError("complete maintained consumer plan must actually pass")
    inputs = ScopeInputs(report, resolver, proof_inputs)
    if require_owned:
        owned_check(report, manifest, inputs, "cmake/package/verify_consumers.py")
    output = Path(identity["path"]).parent
    prefix = Path(evidence["prefix"])
    sdk = prefix / "share/nexus/src"
    sdk_identity, sdk_manifest = proof_document(report, "nexus-installed-source-sdk", resolver)
    if sdk_identity["path"] != str(sdk / ".nexus-source-sdk.json"):
        raise EvidenceError("consumer execution selected another source SDK")
    commands = evidence.get("commands", [])
    groups = {index: [] for index in range(len(plan) + 1)}
    for record in commands:
        index, phase = record.get("consumer_index"), record.get("phase_index")
        if type(index) is not int or index not in groups:
            raise EvidenceError("unknown consumer command index")
        if index not in [0, *indices]:
            continue
        if (type(index) is not int or index not in groups or type(phase) is not int
                or phase != len(groups[index]) or record.get("exit_code") != 0
                or record.get("outcome") != "completed"
                or record.get("withdrawal_complete") is not True):
            raise EvidenceError("consumer command plan is incomplete or failed")
        groups[index].append(record)
        log = output / record.get("log", "")
        if log.parent != output or ".." in log.parts:
            raise EvidenceError("consumer receipt leaves the original execution")
        argv = record["argv"]
        tool_kind = (argv[0] if argv[0] in ("cmake", "git") else
                     "python" if len(argv) >= 3 and argv[1] == "-B" else None)
        receipt = inputs.command(log, argv=argv, stage="source-sdk-consumer",
                                 tool_kind=tool_kind)
        for key in ("started_ns", "finished_ns", "duration_seconds", "outcome",
                    "child_returncode", "withdrawal_complete"):
            if record.get(key) != receipt.get(key):
                raise EvidenceError("consumer report differs from original command receipt")
        if output / record["receipt"] != Path(
                inputs.json(log.with_suffix(".command.json"))["attempt_receipt"]):
            raise EvidenceError("consumer original receipt association differs")
    initial = groups[0]
    if (len(initial) != 1 or initial[0]["argv"][1:] != [
            "-B", str(sdk / "cmake/package/package_source_sdk.py"), "--verify", str(sdk)]):
        raise EvidenceError("actual source SDK verification command required")
    layouts = consumer_layout()
    for index, ((kind, specification), (source, build, images)) in enumerate(
            zip(plan, layouts), 1):
        if index not in indices:
            continue
        rows = groups[index]
        source, build = output / source, output / build
        argvs = [row["argv"] for row in rows]
        if not argvs or argvs[0] != ["git", "init", "--quiet", str(source)]:
            raise EvidenceError("complete independent consumer source preparation required")
        configured = [argv for argv in argvs if argv[:3] == ["cmake", "-S", str(source)]]
        targets = [Path(image).stem for image in images]
        fixture = any("ALLOW_SOURCE_SDK_FIXTURE" in arg for argv in configured for arg in argv)
        bounded_fixture = (consumer_index is not None and not require_owned and
            sdk_manifest.get("publishable") is False and sdk_manifest.get("source_dirty") is True)
        if (len(configured) != 1 or configured[0][3:5] != ["-B", str(build)] or
                configured[0].count("-DNEXUS_EXPECTED_SOURCE_REVISION=" +
                                    manifest["source"]["commit"]) != 1 or
                configured[0].count("-DNexus_DIR=" + str(prefix / "lib/cmake/Nexus")) != 1
                or (fixture and not bounded_fixture) or
                argvs.count(["cmake", "--build", str(build), "--parallel", "1",
                             "--target", *targets]) != 1):
            raise EvidenceError("consumer configure/build differs from complete maintained plan")
        validate_consumer_inputs(inputs, sdk_identity, sdk_manifest, kind, specification,
            prefix, source, build, configured[0], manifest["source"]["commit"], fixture=fixture)
        observation = None
        if kind == "selected":
            observation = consumers[index - 1].get("selected_image", {})
            if (observation.get("name") != specification[0] or
                    observation.get("assembly") != str(sdk / specification[1]) or
                    configured[0].count("-DNEXUS_ASSEMBLY_FILE=" +
                                        str(sdk / specification[1])) != 1):
                raise EvidenceError("selected consumer differs from its maintained scenario")
            canonical = sdk / "cmake/package/verification/io_evolution/consumers" / (
                specification[2] + ".c")
            if inputs.path(source / "main.c").read_bytes() != inputs.path(canonical).read_bytes():
                raise EvidenceError("selected consumer source differs from its maintained smoke")
        if kind == "services":
            observation = consumers[index - 1].get("service_image", {})
            if (observation.get("name") != specification.name or
                    observation.get("domain") != specification.domain or
                    observation.get("status") != "passed" or
                    observation.get("physical_status") != "not_executed" or
                    observation.get("inputs", {}).get("sdk_manifest") != sdk_identity):
                raise EvidenceError("service consumer differs from its maintained scenario")
            if specification.scenario == "external_provider":
                recorded = observation["inputs"]
                public = recorded.get("source/consumer.c", {})
                canonical = recorded.get("canonical/consumer.c", {})
                package = recorded.get("source/provider/nexus-soc.json", {})
                if (observation.get("kind") != "external-provider-host-model" or
                        observation.get("runtime_executed") is not True or
                        observation.get("elf") != recorded.get("elf") or
                        observation.get("public_source") != public or
                        observation.get("canonical_source") != canonical or
                        observation.get("package_manifest") != package or
                        public.get("path") != str(source / "consumer.c") or
                        canonical.get("path") != str(sdk /
                            "cmake/package/verification/external_provider/consumer.c") or
                        public.get("sha256") != canonical.get("sha256") or
                        package.get("path") != str(source / "provider/nexus-soc.json")):
                    raise EvidenceError("external provider observation differs from its actual model")
        for image in images:
            path = output / image
            actual_image = inputs.path(path)
            if path.suffix == ".elf":
                resource_path = path.with_suffix(".resources.json")
                resource = inputs.json(resource_path)
                selected, _ = resolved_identity(inputs.resolve(resource["resolved"]))
                resource_identity = inputs.identities[str(resource_path)]
                validate_resources({"evidence": [resource_identity]}, {
                    "artifacts": {"elf": inputs.identities[str(path)]},
                    "configuration": resource["resolved"]}, selected, inputs.resolve)
                if observation is not None:
                    observed_elf = (observation.get("artifacts", {}).get("elf")
                        if kind == "selected" else observation.get("inputs", {}).get("elf"))
                    if (observed_elf != inputs.identities[str(path)] or
                            observation.get("resources") != resource_identity):
                        raise EvidenceError("consumer observation differs from its actual ELF")
                    if kind == "services" and specification.scenario in {
                            "image_window", "mcuboot_control"}:
                        validate_consumer_observation(inputs, sdk, source, build,
                            kind, specification, observation, rows, output)
                        continue
                    if not isinstance(observation.get("retention"), dict):
                        raise EvidenceError("actual consumer provider retention required")
                    retained = load_json(inputs.resolve(observation["retention"]))
                    if (retained.get("status") != "passed" or
                            retained.get("elf") != observed_elf or
                            retained.get("resolved") != resource["resolved"]):
                        raise EvidenceError("consumer retention differs from its actual ELF")
                    retention_argv = [argv for argv in argvs if len(argv) >= 3 and
                        argv[1:3] == ["-B", str(sdk / "tools/measurement/retention.py")]]
                    if (len(retention_argv) != 1 or
                            retention_argv[0][3:11] != ["--elf", str(path),
                                "--bindings", retained["bindings"]["path"],
                                "--resolved", resource["resolved"]["path"],
                                "--output", observation["retention"]["path"]]):
                        raise EvidenceError("actual consumer retention checker required")
            else:
                header = actual_image.read_bytes()[:20]
                if (len(header) != 20 or header[:6] != b"\x7fELF\x02\x01" or
                        header[18:20] != b"\x3e\x00" or argvs.count([str(path)]) != 1):
                    raise EvidenceError("native consumer was built without actual execution")
                if kind == "selected":
                    validate_consumer_observation(inputs, sdk, source, build,
                        kind, specification, observation, rows, output)
                elif observation is not None and (
                        observation.get("inputs", {}).get("elf") !=
                        inputs.identities[str(path)] or
                        observation.get("runtime_executed") is not True):
                    raise EvidenceError("native consumer observation differs from actual execution")
    expected_selected = [row["selected_image"] for row in consumers
                         if "selected_image" in row]
    expected_services = [row["service_image"] for row in consumers
                         if "service_image" in row]
    if consumer_index is None and (
            len(expected_selected) != sum(kind == "selected" for kind, _ in plan) or
            len(expected_services) != sum(kind == "services" for kind, _ in plan) or
            evidence.get("selected_images") != expected_selected or
            evidence.get("service_images") != expected_services):
        raise EvidenceError("complete selected and service consumer observations required")
    return inputs.records()


def validate_scope_proof(report, manifest, resolved, *, resolver=verify_file_identity,
                         sdk_packages=None, proof_inputs=None, legacy_replay=False):
    scope = report["scope"]
    if scope == "resource_budget":
        return validate_resources(report, manifest, resolved, resolver), None
    if scope == "reproducibility":
        return validate_reproduction(report, manifest, resolved, resolver,
            live=sdk_packages is None, legacy_replay=legacy_replay), None
    if scope == "source_sdk":
        extra = ([] if legacy_replay else validate_sdk_consumers(
            report, manifest, resolver, proof_inputs))
        closure = validate_sdk(report, manifest, resolver, sdk_packages,
                               legacy_replay=legacy_replay)
        return [*extra, *closure["files"].values(), *closure["package_files"].values()], closure
    validators = {"host_contracts": lambda: validate_host(
        report, manifest, resolver, proof_inputs),
        "arm_link": lambda: validate_arm_link(
            report, manifest, resolved, resolver, proof_inputs),
        "hil_tooling": lambda: validate_hil_tooling(
            report, manifest, resolved, resolver, proof_inputs)}
    if scope not in validators:
        raise EvidenceError("unsupported qualification scope")
    return ([], None) if legacy_replay else (validators[scope](), None)
