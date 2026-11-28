#!/usr/bin/env python3
"""Check a derived support table against maintained facts and real assemblies.

This report is not a configuration input or qualification record. It uses the
production resolver and selected static bindings, and records exact source definitions
selected by existing software fixtures. It does not infer physical support.
"""

import argparse
from collections import defaultdict
from dataclasses import asdict
import json
from pathlib import Path
import re
import sys
import tomllib

if __name__ == "__main__":
    sys.dont_write_bytecode = True

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.configure import configure, packages  # noqa: E402
from tools.evidence.common import file_identity, load_json, verify_file_identity  # noqa: E402

DOCUMENT = "docs/reference/platforms/capabilities.md"
START = "<!-- nexus-capabilities:start -->"
END = "<!-- nexus-capabilities:end -->"
EVIDENCE_AXES = {
    "build_provenance": "arm_image",
    "source_sdk_consumer_verification": "installed_sdk",
}


def os_selection(typed, data):
    """Project the resolved backend policy; do not infer execution or costs."""
    backend = typed.backend
    policy = data.get("kernel_profile")
    kernel_path = {"freertos": "ext/freertos", "threadx": "ext/threadx"}.get(
        backend)
    dependencies = [dict(item) for item in data["source_dependencies"]
                    if item["path"] == kernel_path]
    if kernel_path is not None and len(dependencies) != 1:
        raise ValueError("Selected OS requires its exact resolved kernel source")
    if backend in {"freertos", "threadx"} and not isinstance(policy, dict):
        raise ValueError("Selected OS requires its resolved kernel policy")
    freertos = backend == "freertos"
    scheduled = backend != "baremetal"
    protected = freertos and policy["memory_protection"]
    split = freertos and policy["security_model"] == "split"
    restrictions = [
        "Configuration selection is not executed SDK, linked-cost or HIL evidence.",
        "Backend task/queue storage is not a universal binary ABI.",
        "Products own workers, stack budgets, fairness and shutdown progress."]
    if scheduled:
        restrictions.append(
            "Task/control/stack storage remains live until successful join.")
    if backend == "native":
        restrictions.append("POSIX host execution establishes no MCU IRQ or timing.")
    elif backend == "baremetal":
        restrictions.append("No scheduler, task or kernel queue is selected.")
    else:
        restrictions.append(
            "Privileged unmasked Thread execution; ISR APIs require the selected ceiling.")
    if backend == "threadx":
        restrictions.append("ThreadX uses a fixed-rate kernel tick.")
    if protected:
        restrictions.append(
            "Restricted tasks own protected metadata, stack and reviewed user regions.")
    if split:
        restrictions.append(
            "Split mode needs a separately linked matching Secure companion "
            "and caller-owned contexts.")
    if freertos:
        restrictions.append(
            "Permanent task storage stays live for its complete nonreturning lifetime.")
    return {
        "backend": backend, "kernel": dependencies, "profile": policy,
        "port": policy["kernel_port"] if policy else None,
        "abi": {"cpu": data["cpu"], "enum_abi": typed.enum_abi,
                "compile_options": list(typed.cpu_profile.compile_options)},
        "tick": ({"source": policy["tick_source"], "hz": policy["tick_hz"],
                  "reload": policy["tick_reload"],
                  "tickless": policy.get("tickless", False)} if policy else None),
        "capabilities": {
            "wait_select": True, "joinable_task": scheduled,
            "raw_queue": scheduled, "closable_queue": scheduled,
            "direct_notification": freertos and policy["task_notifications"],
            "permanent_task": freertos, "memory_protection": protected,
            "split_security": split,
            "trace": freertos and policy["trace"],
            "runtime_stats": freertos and policy["runtime_stats"]},
        "storage": {
            "owner": "caller",
            "minimum_task_stack_words": policy.get("min_stack_words") if policy else None,
            "minimum_task_stack_bytes": policy.get("min_stack_bytes") if policy else None,
            "stack_alignment_bytes": policy.get("stack_alignment_bytes") if policy else None,
            "idle_stack_words": policy.get("idle_stack_words") if policy else None,
            "system_call_stack_words": policy["system_call_stack_words"] if protected else None,
            "secure_idle_stack_bytes": policy["secure_idle_stack_bytes"] if split else None},
        "execution": "not_assessed", "linked_costs": "not_measured",
        "runtime_stack_peak": "not_measured", "restrictions": restrictions}


def selection(assembly, root=ROOT, *, test_support=False, evidence=()):
    """Describe one validated selection; execution has a separate authority.

    Family declarations cannot establish a selected Board/backend/clock tuple.
    Resolve the authored input normally, then project the existing typed public
    contracts. No test receipt, linked image or physical observation is inferred
    from an accepted configuration or a model implementation.
    """
    from tools.configure import selected_contracts
    from tools.configure.resource_graph import json_value
    root = Path(root).resolve()
    typed = configure.resolve_ir(Path(assembly), root,
                                 test_support=test_support)
    data = typed.to_dict()
    contracts = selected_contracts.for_configuration(typed)
    policies = selected_contracts.silicon_policies(typed)
    silicon = {owner: (json_value(asdict(policy)) if policy is not None else None)
               for owner, policy in policies.items()}
    by_face = {(item.id, item.public_class): item for item in contracts}
    by_id = {item.id: item for item in typed.controllers}
    selected_faces = set()
    controllers = []
    for item in data["controllers"]:
        record = {key: item[key] for key in
                  ("id", "controller", "kind", "mode", "binding")}
        face = (item["id"], selected_contracts.controller_class(
            by_id[item["id"]]))
        selected_faces.add(face)
        contract = by_face[face]
        record.update(dma=item.get("dma", []), pins=item.get("pins", []),
                      contract=json_value(vars(contract)),
                      silicon_admission=silicon[item["id"]])
        controllers.append(record)
    endpoints = []
    for item in typed.devices:
        face = (item.id, selected_contracts.endpoint_class(item))
        selected_faces.add(face)
        contract = by_face[face]
        endpoints.append({"id": item.id, "controller": item.controller,
                          "contract": json_value(vars(contract))})
    validation = dict.fromkeys(("host_models", "arm_image", "installed_sdk",
                                "software_candidate", "physical_hil", "hil_tooling",
                                "resource_budget", "reproducibility"),
                               "not_assessed")
    validation["configuration"] = "validated"
    if typed.cpu_profile.arch == "native":
        validation["physical_hil"] = "not_applicable"
    result = {"schema_version": 1, "kind": "selected_support",
            "selection": {key: data[key] for key in
                          ("board", "soc_family", "part", "package",
                           "backend", "clock_profile", "cpu", "irq",
                           "optimization", "physical_pcb_revision")},
            "configuration_sha256": data["configuration_sha256"],
            "provider_package": (dict(typed.provider_context.identity)
                if typed.provider_context.identity is not None else
                {"family": typed.soc_family, "scope": "native_model"}),
            "os": os_selection(typed, data),
            "silicon_admission": silicon[None],
            "controllers": controllers, "endpoints": endpoints,
            "companion_interfaces": [json_value(vars(item))
                                     for item in contracts if
                                     (item.id, item.public_class) not in
                                     selected_faces],
            "memory_domains": data["memory"],
            "resource_owners": data["claims"],
            "input_identities": data["inputs"],
            "validation": validation, "physical_qualified": False,
            "limitations": ["Selection does not establish execution or "
                            "hardware qualification.",
                            *[item for item in data.get("limitations", [])
                              if item != "Physical qualification not executed"]]}
    if evidence:
        result["evidence"] = [selection_evidence(path, typed, root,
            test_support=test_support) for path in evidence]
        for item in result["evidence"]:
            axis = EVIDENCE_AXES.get(item["kind"])
            if axis and item["status"] == "identity_matched":
                validation[axis] = "identity_matched"
    return result


def selection_evidence(path, typed, root, *, test_support):
    """Associate existing reports by observed bytes, never reported success."""
    from tools.configure.generation import emission
    from tools.evidence.identity import (
        observe_git_source, resolved_identity, resolved_input_files, verify_sdk_package)
    from tools.measurement import resources
    from tools.measurement.elf import Elf32

    result = {"report": {"path": str(Path(path).absolute())}, "kind": None,
        "status": "not_assessed", "software_qualified": False,
        "identities": {"source": "not_recorded", "configuration": "not_assessed",
            "backend_modes": "not_assessed", "inputs": "not_assessed",
            "artifact": "not_assessed"},
        "reason": "unsupported_report_kind"}
    try:
        report_identity = file_identity(path)
        result["report"] = report_identity
        report = load_json(path)
        kind = report.get("kind")
        if not isinstance(kind, str):
            return result
        result["kind"] = kind
        if kind not in EVIDENCE_AXES and kind != "elf_resource_budget":
            return result
        if type(report.get("schema_version")) is not int or report["schema_version"] != 1:
            raise ValueError("unsupported evidence report schema")
        if kind == "source_sdk_consumer_verification":
            result["consumer"] = _associate_sdk_consumer(
                Path(path).absolute().parent, report, typed, root,
                test_support=test_support)
            if file_identity(path) != report_identity:
                raise ValueError("report changed during identity association")
            result["identities"] = dict.fromkeys(result["identities"], "matched")
            result.update(status="identity_matched", reason="identities_verified")
            return result
        configuration = report.get("resolved", report.get("configuration"))
        resolved_path = verify_file_identity(configuration)
        resolved, resolved_file = resolved_identity(resolved_path,
                                                    allow_reference=test_support)
        if resolved != emission.resolved_projection(typed):
            result["identities"]["configuration"] = "mismatched"
            raise ValueError("report configuration differs from the selected assembly")
        inputs = resolved_input_files(resolved_path, resolved)
        result["identities"].update(configuration="matched", backend_modes="matched")
        if report["kind"] == "elf_resource_budget":
            elf = verify_file_identity(report["elf"])
            budget = verify_file_identity(report["budget"])
            # The selected projection was verified above. Reuse only this
            # observation inside the same read; no report success is cached.
            def read_inputs(path, *, allow_reference):
                if path != resolved_path:
                    raise ValueError("resource report changed its configuration")
                return resolved, resolved_file, inputs
            actual = resources.measure(elf, resolved_path, budget,
                require_cpu=True, allow_reference=test_support, read_inputs=read_inputs)
            if actual != report:
                raise ValueError("resource report differs from its actual inputs or ELF")
            result["identities"].update(inputs="matched", artifact="matched")
            result["reason"] = "source_identity_not_recorded"
        else:
            from tools.dev.provenance import verify_provenance
            verify_provenance(report)
            expected_source = report["source"]
            result["identities"]["source"] = "not_assessed"
            if (root / ".nexus-source-sdk.json").exists():
                package = verify_sdk_package(root)
                actual_source = {"kind": "source_sdk",
                    "source_revision": package["source_revision"],
                    "source_tree": package["source_tree"],
                    "manifest": file_identity(root / ".nexus-source-sdk.json")}
            else:
                actual_source = observe_git_source(root)
            if expected_source != actual_source:
                result["identities"]["source"] = "mismatched"
                raise ValueError("report source identity differs from the current source")
            images = [item for item in report["artifacts"]
                      if Path(item["path"]).suffix == ".elf"]
            if not images:
                raise ValueError("build provenance contains no ARM ELF artifact")
            for image in images:
                elf = Elf32(verify_file_identity(image).read_bytes(),
                    float_abi=resources.resolved_float_abi(resolved, require_cpu=True))
                resources.validate_elf_configuration(elf, resolved)
            result["identities"].update(source="matched", inputs="matched",
                                        artifact="matched")
            result.update(status="identity_matched", reason="identities_verified")
        if file_identity(path) != report_identity:
            raise ValueError("report changed during identity association")
    except (OSError, ValueError, KeyError, TypeError) as error:
        result.update(status="not_assessed", reason=str(error))
    return result


def _successful_build(directory, report, consumer, build):
    """Bind the consumer's build receipt to its owned output directory."""
    from tools.evidence.common import EvidenceError, regular_file, stream_digest

    candidates = [entry for entry in report["commands"]
                  if entry["consumer_index"] == consumer["consumer_index"]
                  and entry["argv"][:3] == ["cmake", "--build", str(build)]]
    if len(candidates) != 1:
        raise EvidenceError("consumer has no unique build receipt")
    entry, = candidates
    relative = Path(entry["receipt"])
    if relative.is_absolute() or ".." in relative.parts:
        raise EvidenceError("consumer receipt is outside the observation")
    path = directory / relative
    before = file_identity(path)
    receipt = load_json(path)
    if (entry["exit_code"] != 0 or entry["withdrawal_complete"] is not True
            or receipt["stage"] != "source-sdk-consumer"
            or receipt["command"] != entry["argv"]
            or receipt["outcome"] != "completed"
            or receipt["withdrawal_complete"] is not True
            or type(receipt["exit_code"]) is not int
            or receipt["exit_code"] != 0
            or type(receipt["child_returncode"]) is not int
            or receipt["child_returncode"] != 0):
        raise EvidenceError("consumer build did not complete successfully")
    for key, output in (("stdout", "stdout.txt"), ("stderr", "stderr.txt")):
        raw = regular_file(path.with_name(output), nonempty=False)
        observed = receipt[key]
        with raw.open("rb") as stream:
            actual_sha256 = stream_digest(stream)
        if (observed["path"] != str(raw)
                or observed["bytes"] != raw.stat().st_size
                or observed["sha256"] != actual_sha256):
            raise EvidenceError("consumer build output has changed")
    if file_identity(path) != before:
        raise EvidenceError("consumer build receipt changed")


def _arm_images(report):
    for consumer in report["consumers"]:
        if consumer["status"] != "passed":
            continue
        if "service_image" in consumer:
            image = consumer["service_image"]
            inputs = image["inputs"]
            if image["runtime_executed"] is False and image["resources"]:
                yield consumer, inputs["resolved"], inputs["elf"], image["resources"]
        elif "selected_image" in consumer:
            image = consumer["selected_image"]
            if "artifacts" in image:
                yield consumer, image["resolved"], image["artifacts"]["elf"], image["resources"]


def _associate_sdk_consumer(directory, report, typed, root, *, test_support):
    """Verify only the matching image, without replaying an SDK test plan."""
    from tools.configure.generation.emission import resolved_projection
    from tools.evidence import identity
    from tools.evidence.common import EvidenceError, regular_file
    from tools.measurement import resources

    if (report["status"] != "passed"
            or report["scope"] not in ("full-consumers", "selected-consumers")
            or typed.cpu_profile.arch == "native"):
        raise EvidenceError("requires a successful installed ARM consumer")
    prefix = Path(report["prefix"])
    if not prefix.is_absolute() or prefix.resolve() != prefix:
        raise EvidenceError("noncanonical SDK prefix")
    sdk = prefix / "share/nexus/src"
    manifest_path = sdk / ".nexus-source-sdk.json"
    if sdk != root:
        source = identity.observe_git_source(root)
        if (source["dirty"] or (source["commit"], source["tree"]) !=
                (report["source_revision"], report["source_tree"])):
            raise EvidenceError("SDK consumer source differs from the selected source")

    expected = resolved_projection(typed)
    for consumer, configuration, image, resource in _arm_images(report):
        observed = Path(configuration["path"])
        # A deleted unrelated consumer must not prevent querying another one.
        if not observed.is_file() or load_json(observed) != expected:
            continue
        resolved = verify_file_identity(configuration)
        actual, _ = identity.resolved_identity(resolved, allow_reference=test_support)
        if actual != expected:
            raise EvidenceError("SDK consumer configuration changed")
        files = identity.resolved_input_files(resolved, actual)
        if not any(entry["label"] == "dependencies/platform-compatibility.json"
                   and Path(entry["file"]["path"]).parents[1] == sdk
                   for entry in files):
            raise EvidenceError("consumer configuration came from another source")
        elf = verify_file_identity(image)
        build = elf.parent.parent
        if (not resolved.is_relative_to(build) or not build.is_relative_to(directory)
                or regular_file(build / "resolved-config.txt").read_text() !=
                str(resolved.parent)
                or regular_file(build / "resolved-sdk.txt").read_text() != str(sdk)):
            raise EvidenceError("consumer build does not bind this SDK and assembly")
        manifest_before = file_identity(manifest_path)
        manifest = identity.verify_sdk_package(sdk)
        if (manifest["publishable"] is not True or manifest["source_dirty"] is not False
                or (manifest["source_revision"], manifest["source_tree"]) !=
                (report["source_revision"], report["source_tree"])):
            raise EvidenceError("consumer source SDK identity differs")
        resource_path = verify_file_identity(resource)
        observed_budget = load_json(resource_path)
        budget = verify_file_identity(observed_budget["budget"])

        def read_inputs(path, *, allow_reference):
            if path != resolved:
                raise EvidenceError("consumer resource report changed its configuration")
            return actual, configuration, files

        measured = resources.measure(elf, resolved, budget, require_cpu=True,
            allow_reference=test_support, read_inputs=read_inputs)
        if measured != observed_budget or measured["status"] != "passed":
            raise EvidenceError("consumer resource report differs from the actual ELF")
        _successful_build(directory, report, consumer, build)
        if (file_identity(manifest_path) != manifest_before
                or file_identity(resolved) != configuration
                or file_identity(elf) != image
                or file_identity(resource_path) != resource
                or identity.resolved_input_files(resolved, actual) != files):
            raise EvidenceError("consumer inputs changed during identity association")
        return {"name": consumer["name"], "elf": image,
                "configuration": configuration, "sdk_manifest": manifest_before}
    raise EvidenceError("report has no installed ARM image for this exact assembly")


def c_code(source):
    """Remove comments and literals before examining exact C declarations."""
    return re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                  " ", source, flags=re.DOTALL)


def ops_definitions(source):
    """Read initialized exported ops objects, never mentions or declarations."""
    return set(re.findall(r"\bconst\s+nx_\w+_ops_t\s+(nx_\w+)\s*=\s*\{",
                          c_code(source)))


def source_ops(path, root=ROOT):
    """Reuse the ELF observer's exact source specialization contract.

    This source projection is not execution evidence. Conditional, ambiguous,
    duplicate or opaque template selections fail closed in the shared helper;
    the wrapper remains the actual translation unit for method provenance.
    """
    from tools.measurement.retention import provider_initializers
    source = re.sub(r"/\*.*?\*/|//[^\n]*", " ",
                    path.read_text(encoding="utf-8"), flags=re.DOTALL)
    return set().union(*(ops_definitions(initializer)
                         for initializer, _ in provider_initializers(
                             path, source, root)))


def _checkout(root):
    """Global maintenance needs authored fixtures, not a selected SDK payload."""
    root = Path(root).resolve()
    required = ('docs', 'tests/integration', 'cmake/package/verification',
                'tools/configure/assemblies')
    if ((root / '.nexus-source-sdk.json').exists()
            or any(not (root / name).is_dir() for name in required)):
        raise ValueError('Global capability maintenance requires a complete source checkout; '
                         'use --assembly for selected SDK support')
    return root


def collect(root=ROOT):
    """Reject failed resolution or missing selected production definitions."""
    root = Path(root).resolve()
    from tools.configure import matrix
    from tools.configure.providers import for_selection
    facts = {}
    routes = {}
    declared = defaultdict(set)
    exports = {}
    production_sources = set((root / "io/src").rglob("*.c"))
    for view in matrix.package_views(root):
        family = view.package.family
        facts[family] = {part.part: part.facts.device for part in view.parts}
        route_index = {}
        for part in view.parts:
            route_index.update(part.routes)
        routes[family] = route_index
        production_sources.update(
            source for directory in view.package.export_roots
            for source in directory.rglob("*.c") if view.package.exports(source))
    # Several packages can export one shared provider source. Scan each file once.
    for source in sorted(production_sources):
        for symbol in source_ops(source, root):
            if symbol in exports:
                raise ValueError(f"Duplicate ops definition: {symbol}")
            exports[symbol] = source.relative_to(root).as_posix()
    if not facts:
        raise ValueError("No maintained MCU SoC facts found")
    for board_path in sorted((root / "boards").glob("*/board.json")):
        board = configure.load(board_path)
        family = board["soc_family"]
        if family == "native":
            continue
        if family not in facts or board["soc"] not in facts[family]:
            raise ValueError(f"Board has no maintained part: {board_path}")
        for binding in board["bindings"]:
            route = routes[family].get(binding["route"])
            if (route is None or route["controller"] != binding["controller"]
                    or board["soc"] not in route["variants"]):
                raise ValueError("Board binding contradicts route: "
                                 + binding["id"])
            for mode in route["modes"]:
                declared[(family, binding["controller"], route["kind"], mode)].add(board["id"])
    selected = defaultdict(lambda: {"fixtures": set(), "exports": set()})
    assembly_paths = (root / "tools/configure/assemblies").glob("*.toml")
    contract_paths = []
    for path in (root / "tests/integration").rglob("*.toml"):
        # Authored inputs carry schema even outside the assembly directories.
        # Existing assembly directories retain strict validation if it is absent.
        if (path.parent.name.endswith("_assembly") or
                "schema" in tomllib.loads(path.read_text(encoding="utf-8"))):
            contract_paths.append(path)
    sdk_paths = (root / "cmake/package/verification/io_evolution").glob("*.toml")
    fixture_paths = sorted(set(assembly_paths) | set(contract_paths)
                           | set(sdk_paths))
    for fixture in fixture_paths:
        authored = tomllib.loads(fixture.read_text(encoding="utf-8"))
        if authored.get("backend") == "native":
            continue
        package_path = authored.get("soc_package")
        if isinstance(package_path, str):
            manifest = fixture.parent / package_path / "package.json"
            if manifest.is_file() and packages.load(manifest).scope != "production":
                continue
        typed = configure.resolve_ir(fixture, root)
        if (typed.provider_context.package is not None and
                typed.provider_context.package.scope != "production"):
            raise ValueError("Production capability collection requires a production package")
        provider = for_selection(typed)
        for item in typed.controllers:
            operations = provider.operations(item)
            if (not isinstance(operations, str)
                    or not re.fullmatch(r'nx_\w+_ops', operations)
                    or operations not in exports):
                raise ValueError("Selected ops has no production definition: "
                                 + f"{fixture}:{item.id}")
            key = (typed.soc_family, item.controller, item.kind, item.mode)
            selected[key]["fixtures"].add(fixture.relative_to(root).as_posix())
            selected[key]["exports"].add(exports[operations])
    rows = []
    for family, parts in sorted(facts.items()):
        personalities = set()
        for view in parts.values():
            for controller, hardware in view.controllers.items():
                policy = view.modes[controller]
                interfaces = [(hardware.kind, policy.modes)]
                interfaces += list(policy.interfaces.items())
                personalities.update((controller, kind, mode)
                                     for kind, modes in interfaces
                                     for mode in modes)
        for controller, kind, mode in sorted(personalities):
            key = family, controller, kind, mode
            reviewed = [route["id"] for route in routes[family].values()
                        if route["controller"] == controller and
                        route["kind"] == kind and mode in route["modes"]]
            row = {"family": family, "controller": controller,
                   "kind": kind, "mode": mode,
                   "routes": sorted(reviewed),
                   "boards": sorted(declared[key]),
                   "fixtures": sorted(selected[key]["fixtures"]),
                   "exports": sorted(selected[key]["exports"])}
            rows.append(row)
    known = {(row["family"], row["controller"], row["kind"], row["mode"])
             for row in rows}
    if selected.keys() - known:
        raise ValueError("Resolved fixture selects an undeclared mode")
    return rows


def render(rows):
    """Render the sole derived block deterministically without qualification."""
    lines = [("| SoC | 控制器 | 类别 | 模式 | 已审查 SoC 路线 | "
              "已声明参考板卡 | 解析器夹具 | 所选生产源码 |"),
             "| --- | --- | --- | --- | --- | --- | --- | --- |"]
    for row in rows:
        cells = [row[key] for key in ("family", "controller", "kind", "mode")]
        if row.get("implementation") == "MODEL":
            if row.get("physical_qualified") is not False:
                raise ValueError("Model rows cannot assert physical qualification")
            cells[0] += " (MODEL; physical=false)"
        for key in ("routes", "boards", "fixtures", "exports"):
            cells.append(", ".join(f"`{value}`" for value in row[key]) or "—")
        lines.append("| " + " | ".join(cells) + " |")
    return "\n".join(lines) + "\n"


def check_block(document, expected):
    """Reject duplicate/missing markers and any stale generated byte."""
    if document.count(START) != 1 or document.count(END) != 1:
        raise ValueError("Capability document requires exactly one marker pair")
    begin = document.index(START) + len(START)
    end = document.index(END)
    if end < begin or document[begin:end] != "\n" + expected:
        raise ValueError("Stale capability document; use --write to update")


def main(arguments=None, *, support_command=False):
    parser = argparse.ArgumentParser(description=__doc__)
    action = parser.add_mutually_exclusive_group()
    action.add_argument("--check", action="store_true")
    action.add_argument("--write", action="store_true")
    action.add_argument("--assembly", type=Path,
                        help="print exact selected support JSON, not execution "
                             "qualification")
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--evidence", type=Path, action="append", default=[],
                        help="associate an existing resource/provenance report by identity")
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--test-support", action="store_true",
                        help="explicitly query a host test fixture")
    args = parser.parse_args(arguments)
    if support_command and (args.assembly is None or args.write or args.check):
        parser.error("support requires --assembly and is a read-only query")
    if args.evidence and args.assembly is None:
        parser.error("--evidence requires an exact --assembly")
    try:
        if args.assembly is not None:
            result = selection(args.assembly, args.root,
                               test_support=args.test_support, evidence=args.evidence)
            print(json.dumps(result, indent=2, sort_keys=True))
            return 0
        root = _checkout(args.root)
        from tools.configure import documentation
        examples = documentation.check(root)
        expected = render(collect(args.root))
        path = args.root / DOCUMENT
        document = path.read_text(encoding="utf-8")
        if args.write:
            if document.count(START) != 1 or document.count(END) != 1:
                raise ValueError("Capability document needs one marker pair")
            begin = document.index(START) + len(START)
            end = document.index(END)
            if end < begin:
                raise ValueError("Capability document marker order is invalid")
            document = document[:begin] + "\n" + expected + document[end:]
            path.write_text(document, encoding="utf-8")
        check_block(document, expected)
        print("Capability document matches facts and selected definitions; "
              "physical HIL is not assessed")
        print(f"Resolved {sum(row['kind'] != 'fragment' for row in examples)} "
              "TOML examples; "
              f"{sum(row['kind'] == 'fragment' for row in examples)} explicit fragments")
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f"Capability consistency rejected: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
