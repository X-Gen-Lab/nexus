#!/usr/bin/env python3
"""Check a derived support table against maintained facts and real assemblies.

This report is not a configuration input or qualification record. It uses the
production resolver and binding emitter, and records exact source definitions
selected by existing software fixtures. It does not infer physical support.
"""

import argparse
from collections import defaultdict
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.configure import bindings, configure  # noqa: E402

DOCUMENT = "docs/delivery/capabilities.md"
START = "<!-- nexus-capabilities:start -->"
END = "<!-- nexus-capabilities:end -->"


def c_code(source):
    """Remove comments and literals before examining exact C declarations."""
    return re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                  " ", source, flags=re.DOTALL)


def ops_definitions(source):
    """Read initialized exported ops objects, never mentions or declarations."""
    return set(re.findall(r"\bconst\s+nx_\w+_ops_t\s+(nx_\w+)\s*=\s*\{",
                          c_code(source)))


def collect(root=ROOT):
    """Reject failed resolution or missing selected production definitions."""
    root = root.resolve()
    facts = {}
    routes = {}
    declared = defaultdict(set)
    exports = {}
    for path in sorted((root / "soc").glob("*/soc.json")):
        soc = configure.load(path)
        if soc["id"] == "native":
            continue
        for part in soc["variants"]:
            configure.validate_soc(soc, part)
        facts[soc["id"]] = soc
        route_doc = configure.load(path.parent / "routes.json")
        configure.obj(route_doc, {"schema_version", "routes"}, (), "routes")
        configure.integer(route_doc["schema_version"], 1, 1, "routes schema")
        route_index = {}
        for route in route_doc["routes"]:
            identity = route["id"]
            if identity in route_index:
                raise ValueError(f"Duplicate route: {identity}")
            controller = soc["controllers"].get(route["controller"])
            if (controller is None or controller["kind"] != route["kind"]
                    or not set(route["modes"]).issubset(controller["modes"])
                    or not set(route["variants"]).issubset(soc["variants"])):
                raise ValueError(f"Route contradicts SoC facts: {identity}")
            route_index[identity] = route
        routes[soc["id"]] = route_index
        for source in sorted((path.parent / "drivers").glob("*.c")):
            for symbol in ops_definitions(source.read_text(encoding="utf-8")):
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
                declared[(family, binding["controller"], mode)].add(board["id"])
    selected = defaultdict(lambda: {"fixtures": set(), "exports": set()})
    assembly_paths = (root / "tools/configure/assemblies").glob("*.toml")
    contract_paths = (root / "tests/contracts").glob("*_assembly/*.toml")
    fixture_paths = sorted(set(assembly_paths) | set(contract_paths))
    for fixture in fixture_paths:
        result = configure.resolve(fixture, root)
        if result["soc_family"] == "native":
            continue
        source = c_code(bindings.emit(result)[1])
        for item in result["controllers"]:
            name = bindings.symbol(item["id"])
            face_name = re.escape("s_nx_face_" + name)
            matches = re.findall(
                rf"\bstatic\s+const\s+nx_\w+_port_t\s+{face_name}"
                r"\s*=\s*\{\s*&(?P<ops>nx_\w+_ops)\s*,", source)
            if len(matches) != 1 or matches[0] not in exports:
                raise ValueError("Selected ops has no production definition: "
                                 + f"{fixture}:{name}")
            key = result["soc_family"], item["controller"], item["mode"]
            selected[key]["fixtures"].add(fixture.relative_to(root).as_posix())
            selected[key]["exports"].add(exports[matches[0]])
    rows = []
    for family, soc in sorted(facts.items()):
        for controller, facts_item in sorted(soc["controllers"].items()):
            for mode in sorted(facts_item["modes"]):
                key = family, controller, mode
                reviewed = [route["id"] for route in routes[family].values()
                            if route["controller"] == controller
                            and mode in route["modes"]]
                row = {"family": family, "controller": controller,
                       "kind": facts_item["kind"], "mode": mode,
                       "routes": sorted(reviewed),
                       "boards": sorted(declared[key]),
                       "fixtures": sorted(selected[key]["fixtures"]),
                       "exports": sorted(selected[key]["exports"])}
                rows.append(row)
    known = {(row["family"], row["controller"], row["mode"]) for row in rows}
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
    parser.add_argument("--root", type=Path, default=ROOT)
    args = parser.parse_args(arguments)
    try:
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
    except (OSError, ValueError) as error:
        print(f"Capability consistency rejected: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
