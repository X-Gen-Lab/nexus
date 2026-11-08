#!/usr/bin/env python3
"""Check a derived support table against maintained facts and real assemblies.

This report is not a configuration input or qualification record. It uses the
production resolver and binding emitter, and records exact source definitions
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
from tools.configure import bindings, configure, documentation  # noqa: E402
from tools.configure.providers import descriptor  # noqa: E402

DOCUMENT = "docs/delivery/capabilities.md"
START = "<!-- nexus-capabilities:start -->"
END = "<!-- nexus-capabilities:end -->"


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
        restrictions.append("ThreadX physical scope is Cortex-M4; no tickless port is maintained.")
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


def selection(assembly, root=ROOT, *, candidate_path=None, results_index=None,
              record_pointer=None, manifest_path=None, qualification_paths=None,
              source_sdk_path=None, sdk_consumers_path=None,
              hil_admission_path=None, hil_raw_path=None,
              hil_measurements_path=None, verify_record=False, legacy_replay=False,
              test_support=False):
    """Describe one validated selection; execution has a separate authority.

    Family declarations cannot establish a selected Board/backend/clock tuple.
    Resolve the authored input normally, then project the existing typed public
    contracts. No test receipt, linked image or physical observation is inferred
    from an accepted configuration or a model implementation.
    """
    from tools.configure import selected_contracts
    from tools.configure.resource_graph import json_value
    from tools.evidence import identity

    if (results_index is None) != (record_pointer is None):
        raise ValueError("results index and record pointer must be supplied together")
    root = Path(root).resolve()
    references = {key: value for key, value in {
        "manifest": manifest_path, "qualifications": qualification_paths,
        "source_sdk": source_sdk_path, "sdk_consumers": sdk_consumers_path,
        "hil_admission": hil_admission_path, "hil_raw": hil_raw_path,
        "hil_measurements": hil_measurements_path}.items() if value is not None}
    associate = (candidate_path is not None or results_index is not None or
                 bool(references))
    if verify_record and results_index is None:
        raise ValueError("record verification requires an explicit index and record")
    source = (identity.observe_git_source(root)
              if associate else None)
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
    from tools.maintenance import support
    if candidate_path is not None:
        support.associate_candidate(result, data, source, Path(candidate_path),
                                    legacy_replay=legacy_replay)
    if results_index is not None:
        _associate_index(result, data, source, Path(results_index), record_pointer)
        links = result["indexed_evidence"]["record"].get("support_evidence")
        if verify_record and links is not None:
            from tools.evidence.common import fields
            fields(links, set(), support.LINKS)
            if references.keys() & links.keys():
                raise ValueError("explicit evidence duplicates a selected record reference")
            references.update(links)
    if references:
        support.associate(result, data, source, references, legacy_replay=legacy_replay)
    else:
        result["next_missing_evidence"] = [scope for scope in support.SCOPES
            if validation.get(support.VALIDATION[scope]) != "passed"]
        if validation["software_candidate"] != "passed":
            result["next_missing_evidence"].append("software_candidate")
        if validation["physical_hil"] != "not_applicable":
            result["next_missing_evidence"].append("physical_hil")
    if associate:
        if identity.observe_git_source(root) != source:
            from tools.evidence.common import EvidenceError
            raise EvidenceError("source changed during support query")
    return result


def _index_record(document, pointer):
    """Select one explicit RFC6901 record without inheriting ancestor claims."""
    from tools.evidence.common import EvidenceError

    if (not isinstance(pointer, str) or len(pointer) > 2048 or
            (pointer and not pointer.startswith("/")) or
            pointer.count("/") > 32):
        raise EvidenceError("bounded JSON record pointer required")
    value = document
    for part in pointer.split("/")[1:] if pointer else ():
        if re.search(r"~(?![01])", part):
            raise EvidenceError("invalid JSON record pointer escape")
        key = part.replace("~1", "/").replace("~0", "~")
        if isinstance(value, dict) and key in value:
            value = value[key]
        elif (isinstance(value, list) and re.fullmatch(r"0|[1-9][0-9]*", key)
              and len(key) <= 8 and int(key) < len(value)):
            value = value[int(key)]
        else:
            raise EvidenceError("JSON record pointer does not select an existing record")
    if not isinstance(value, dict):
        raise EvidenceError("JSON record pointer must select an object")
    return value


def _associate_index(result, resolved, source, path, pointer):
    """Expose an existing result record, never reinterpret it as raw execution."""
    from tools.evidence.common import (EvidenceError, file_identity, load_json,
                                       regular_file, sha256_value)

    if regular_file(path).stat().st_size > 4 * 1024 * 1024:
        raise EvidenceError("JSON document exceeds its bounded input size")
    index = file_identity(path)
    record = _index_record(load_json(path), pointer)
    reference = record.get("source", {})
    if not isinstance(reference, dict):
        raise EvidenceError("indexed source identity must be an object")
    for key in ("commit", "tree"):
        if key in reference and (not isinstance(reference[key], str) or
                re.fullmatch(r"[0-9a-f]{40}", reference[key]) is None):
            raise EvidenceError("indexed source requires exact Git object identities")
    if "dirty" in reference and type(reference["dirty"]) is not bool:
        raise EvidenceError("indexed source dirty state must be boolean")
    if "submodules" in reference and (not isinstance(reference["submodules"], list) or
            any(not isinstance(item, str) for item in reference["submodules"])):
        raise EvidenceError("indexed source dependencies must be explicit Git observations")
    source_keys = ("commit", "tree", "dirty", "submodules")
    complete = all(key in reference for key in source_keys)
    code_match = all(reference.get(key) == source[key] for key in ("commit", "tree"))
    source_match = (complete and source["dirty"] is False and
                    reference["dirty"] is False and
                    all(item.startswith(" ") for item in source["submodules"]) and
                    all(source[key] == reference[key] for key in source_keys))
    projection = record.get("resolved")
    if projection is not None and not isinstance(projection, dict):
        raise EvidenceError("indexed resolved configuration must be an object")
    digest = record.get("configuration_sha256")
    if digest is None and projection is not None:
        digest = projection.get("configuration_sha256")
    if digest is not None:
        sha256_value(digest)
    config_match = (digest == resolved["configuration_sha256"] and
                    (projection is None or projection == resolved))
    config_relation = ("not_recorded" if digest is None else
                       "mismatch" if not config_match else
                       "exact_resolved" if projection is not None else "exact_digest")
    relation = ("configuration_mismatch" if config_relation == "mismatch" else
                "insufficient_identity" if not complete or digest is None else
                "exact_source_and_configuration" if source_match else "historical_source")
    if file_identity(path) != index:
        raise EvidenceError("results index changed during support query")
    result["current_source"] = source
    result["indexed_evidence"] = {
        "index": index, "record_pointer": pointer, "record": record,
        "relation": relation, "source_match": source_match,
        "code_identity_match": code_match, "configuration_match": config_match,
        "configuration_relation": config_relation,
        "verification": "indexed_observation_only"}
    result["limitations"].append(
        "Indexed observations expose their recorded identity and scope only; "
        "raw execution receipts are not verified and validation status is unchanged. "
        "Missing source or configuration identity is never inherited from ancestors.")


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


def collect(root=ROOT):
    """Reject failed resolution or missing selected production definitions."""
    root = root.resolve()
    facts = {}
    routes = {}
    declared = defaultdict(set)
    exports = {}
    for path in sorted((root / "soc").rglob("soc.json")):
        soc = configure.load(path)
        if soc["id"] == "native":
            continue
        integration = descriptor.maintained(soc["id"])
        catalog = (configure.load(path.parent / integration.device_catalog)
                   if integration.device_catalog is not None else None)
        for part in soc["variants"]:
            selected = (configure.catalog_soc_facts(soc, part, catalog)[0]
                        if catalog is not None else soc)
            configure.validate_soc(selected, part)
        facts[soc["id"]] = soc
        route_doc = configure.load(path.parent / "routes.json")
        configure.obj(route_doc, {"schema_version", "routes"}, (), "routes")
        configure.integer(route_doc["schema_version"], 1, 1, "routes schema")
        # Use the resolver's complete reviewed physical/interface validation.
        # Union exact-part results without inventing a documentation mode gate.
        route_index = {}
        provider = configure.maintained(soc["id"])
        for part in soc["variants"]:
            route_index.update(configure.resolve_routes(
                route_doc, soc, part, provider))
        routes[soc["id"]] = route_index
        for source in sorted((path.parent / "drivers").rglob("*.c")):
            for symbol in source_ops(source, root):
                if symbol in exports:
                    raise ValueError(f"Duplicate ops definition: {symbol}")
                exports[symbol] = source.relative_to(root).as_posix()
    # Shared algorithms keep one real readonly table across SoC wrappers.
    # Read actual production definitions, never expand provider-name guesses.
    shared_sources = set((root / "io/src").rglob("*.c"))
    for source in sorted(shared_sources):
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
        if family not in facts or board["soc"] not in facts[family]["variants"]:
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
    contract_paths = (root / "tests/contracts").glob("*_assembly/*.toml")
    sdk_paths = (root / "cmake/package/verification/io_evolution").glob("*.toml")
    fixture_paths = sorted(set(assembly_paths) | set(contract_paths)
                           | set(sdk_paths))
    for fixture in fixture_paths:
        authored = tomllib.loads(fixture.read_text(encoding="utf-8"))
        if authored.get("backend") == "native":
            continue
        typed = configure.resolve_ir(fixture, root)
        result = typed.to_dict()
        source = c_code(bindings.emit(typed)[1])
        for item in result["controllers"]:
            name = bindings.symbol(item["id"])
            face_name = re.escape("s_nx_face_" + name)
            matches = re.findall(
                rf"\bstatic\s+const\s+nx_\w+_t\s+{face_name}"
                r"\s*=\s*\{\s*&(?P<ops>nx_\w+_ops)\s*,", source)
            if len(matches) != 1 or matches[0] not in exports:
                raise ValueError("Selected ops has no production definition: "
                                 + f"{fixture}:{name}")
            key = (result["soc_family"], item["controller"],
                   item["kind"], item["mode"])
            selected[key]["fixtures"].add(fixture.relative_to(root).as_posix())
            selected[key]["exports"].add(exports[matches[0]])
    rows = []
    for family, soc in sorted(facts.items()):
        for controller, facts_item in sorted(soc["controllers"].items()):
            interfaces = [(facts_item["kind"], facts_item["modes"])]
            interfaces += sorted(facts_item.get("interfaces", {}).items())
            for kind, modes in interfaces:
                for mode in sorted(modes):
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
    lines = [("| SoC | Controller | Kind | Mode | Reviewed SoC routes | "
              "Declared reference Boards | Resolver fixtures | "
              "Selected production source |"),
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
    parser.add_argument("--candidate", type=Path,
                        help="verify and associate an existing sealed "
                             "candidate.json with --assembly")
    parser.add_argument("--results-index", type=Path,
                        help="read an existing delivery results index with --assembly")
    parser.add_argument("--record", dest="record_pointer",
                        help="explicit JSON pointer to one indexed observation; "
                             "does not establish execution qualification")
    parser.add_argument("--verify-record", action="store_true",
                        help="verify typed evidence references on the selected leaf")
    parser.add_argument("--legacy-replay", action="store_true",
                        help="read a frozen schema 1 candidate without current qualification")
    parser.add_argument("--manifest", dest="manifest_path", type=Path,
                        help="existing source/configuration/artifact manifest; partial scopes allowed")
    parser.add_argument("--qualification", dest="qualification_paths", action="append", type=Path,
                        help="original qualification report; repeat for separate scopes")
    for option in ("source-sdk", "sdk-consumers", "hil-admission", "hil-raw", "hil-measurements"):
        parser.add_argument("--" + option, dest=option.replace("-", "_") + "_path", type=Path)
    parser.add_argument("--json", action="store_true", help="print the same read-only query as JSON")
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--test-support", action="store_true",
                        help="explicitly query a host test fixture")
    args = parser.parse_args(arguments)
    evidence_options = ("manifest_path", "qualification_paths", "source_sdk_path",
                        "sdk_consumers_path", "hil_admission_path", "hil_raw_path",
                        "hil_measurements_path")
    if support_command and (args.assembly is None or args.write or args.check):
        parser.error("support requires --assembly and is a read-only query")
    if (any(getattr(args, name) is not None for name in evidence_options) or
            args.verify_record) and args.assembly is None:
        parser.error("evidence queries require --assembly")
    if args.verify_record and args.results_index is None:
        parser.error("--verify-record requires --results-index and --record")
    if (args.hil_raw_path is not None or args.hil_measurements_path is not None) and args.hil_admission_path is None:
        parser.error("physical raw evidence requires --hil-admission")
    if args.candidate is not None and args.assembly is None:
        parser.error("--candidate requires --assembly")
    if args.results_index is not None or args.record_pointer is not None:
        if (args.assembly is None or args.results_index is None or
                args.record_pointer is None):
            parser.error("--results-index and --record require each other and --assembly")
    try:
        if args.assembly is not None:
            result = selection(args.assembly, args.root,
                                       candidate_path=args.candidate,
                                       results_index=args.results_index,
                                       record_pointer=args.record_pointer,
                                       verify_record=args.verify_record,
                                       legacy_replay=args.legacy_replay,
                                       test_support=args.test_support,
                                       **{name: getattr(args, name) for name in evidence_options})
            if support_command:
                from tools.maintenance import support
                result["query_environment"] = support.environment()
            if support_command and not args.json:
                print(support.human(result))
            else:
                print(json.dumps(result, indent=2, sort_keys=True))
            return 0
        examples = documentation.check(args.root)
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
