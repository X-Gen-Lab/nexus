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

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.configure import bindings, configure, documentation  # noqa: E402

DOCUMENT = "docs/delivery/capabilities.md"
START = "<!-- nexus-capabilities:start -->"
END = "<!-- nexus-capabilities:end -->"


def selection(assembly, root=ROOT, *, candidate_path=None):
    """Describe one validated selection; execution has a separate authority.

    Family declarations cannot establish a selected Board/backend/clock tuple.
    Resolve the authored input normally, then project the existing typed public
    contracts. No test receipt, linked image or physical observation is inferred
    from an accepted configuration or a model implementation.
    """
    from tools.configure import selected_contracts
    from tools.configure.resource_graph import json_value
    from tools.evidence import identity

    root = Path(root).resolve()
    source = (identity.observe_git_source(root)
              if candidate_path is not None else None)
    typed = configure.resolve_ir(Path(assembly), root)
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
                                "software_candidate", "physical_hil"),
                               "not_assessed")
    validation["configuration"] = "validated"
    if typed.soc_family == "native":
        validation["physical_hil"] = "not_applicable"
    result = {"schema_version": 1, "kind": "selected_support",
            "selection": {key: data[key] for key in
                          ("board", "soc_family", "part", "package",
                           "backend", "clock_profile", "cpu", "irq",
                           "optimization", "physical_pcb_revision")},
            "configuration_sha256": data["configuration_sha256"],
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
    if candidate_path is not None:
        _associate_candidate(result, data, source, Path(candidate_path))
        if identity.observe_git_source(root) != source:
            from tools.evidence.common import EvidenceError
            raise EvidenceError("source changed during support query")
    return result


def _associate_candidate(result, resolved, source, candidate_path):
    """Only an exact verified source/configuration can support this selection."""
    from tools.evidence import candidate

    reference = candidate.verified_projection(candidate_path)
    source_keys = ("commit", "tree", "dirty", "submodules")
    clean_dependencies = all(line.startswith(" ")
                             for line in source["submodules"])
    source_match = (source["dirty"] is False and clean_dependencies and
                    all(source[key] == reference["source"][key]
                        for key in source_keys))
    configuration_match = (
        reference["configuration_sha256"] == resolved["configuration_sha256"]
        and reference["resolved"] == resolved)
    relation = ("configuration_mismatch" if not configuration_match else
                "exact_source_and_configuration" if source_match else
                "historical_source")
    result["current_source"] = source
    result["evidence"] = {
        "relation": relation, "source_match": source_match,
        "configuration_match": configuration_match,
        "reference": {key: value for key, value in reference.items()
                      if key != "resolved"}}
    if source_match and configuration_match:
        for name in ("host_models", "arm_image", "installed_sdk",
                     "software_candidate"):
            result["validation"][name] = "passed"
    result["limitations"].append(
        "Evidence association verifies an existing candidate; it executes no "
        "new checks and does not assess physical HIL or qualify other selections.")


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
        for part in soc["variants"]:
            configure.validate_soc(soc, part)
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
        for source in sorted((path.parent / "drivers").glob("*.c")):
            for symbol in source_ops(source, root):
                if symbol in exports:
                    raise ValueError(f"Duplicate ops definition: {symbol}")
                exports[symbol] = source.relative_to(root).as_posix()
    # Shared algorithms keep one real readonly table across SoC wrappers.
    # Read actual production definitions, never expand provider-name guesses.
    shared_sources = (set((root / "drivers").rglob("*.c"))
                      | set((root / "io/src").rglob("*.c"))
                      | set((root / "soc/native/drivers").glob("*.c")))
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
        typed = configure.resolve_ir(fixture, root)
        result = typed.to_dict()
        if result["soc_family"] == "native":
            provider = configure.maintained(result["soc_family"])
            if provider.MODEL is not True or result["physical_qualified"] is not False:
                raise ValueError("Native source provenance requires an unqualified MODEL")
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
    # Host models have no physical SoC/Board inventory here. Project only
    # controllers actually selected by the same resolver/face-to-ops path.
    # Empty assemblies therefore cannot manufacture a model capability row.
    for key in sorted(selected):
        family, controller, kind, mode = key
        if family != "native":
            continue
        rows.append({"family": family, "controller": controller,
                     "kind": kind, "mode": mode, "routes": [], "boards": [],
                     "fixtures": sorted(selected[key]["fixtures"]),
                     "exports": sorted(selected[key]["exports"]),
                     "implementation": "MODEL", "physical_qualified": False})
    known = {(row["family"], row["controller"], row["kind"], row["mode"])
             for row in rows}
    if selected.keys() - known:
        raise ValueError("Resolved fixture selects an undeclared mode")
    return rows


def render(rows):
    """Render the sole derived block deterministically without qualification."""
    lines = ["| SoC | Controller | Kind | Mode | Reviewed SoC routes | "
             "Declared reference Boards | Resolver fixtures | "
             "Selected production source |",
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


def main(arguments=None):
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
    parser.add_argument("--root", type=Path, default=ROOT)
    args = parser.parse_args(arguments)
    if args.candidate is not None and args.assembly is None:
        parser.error("--candidate requires --assembly")
    try:
        if args.assembly is not None:
            print(json.dumps(selection(args.assembly, args.root,
                                       candidate_path=args.candidate), indent=2,
                             sort_keys=True))
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
