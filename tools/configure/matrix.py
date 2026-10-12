"""Check maintained authoring/factory contracts and derive reference support.

This reports software selections and reviewed routes, never physical execution
or a source-target graph. Assemblies enter through one explicit catalogue.
"""
import argparse
import json
from pathlib import Path
import re

try:
    from . import authored, configure, factory
    from .contracts import COMPONENTS
    from .providers import descriptor, maintained
    from .providers.common import mode_traits
except ImportError:
    import authored
    import configure
    import factory
    from contracts import COMPONENTS
    from providers import descriptor, maintained
    from providers.common import mode_traits


CATALOGUE = "tools/configure/assemblies/catalogue.json"


def check_contracts(root=configure.ROOT):
    """Reject missing authoring, factory, provider and CMake selection entries."""
    kinds = frozenset(authored.KINDS)
    if kinds != frozenset(factory.KINDS) or kinds != configure.KINDS:
        raise ValueError("Controller kinds differ across authoring/resolver/factory")
    properties = authored.editor_schema()["properties"]
    if not kinds.issubset(properties):
        raise ValueError("Editor schema is missing authored controller kinds")
    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    selected = frozenset(re.findall(r'component STREQUAL "([a-z0-9-]+)"', cmake))
    if selected != COMPONENTS:
        raise ValueError("Maintained components differ from CMake target selection")
    for family in descriptor.families():
        provider = maintained(family.family)
        soc = configure.load(root / "soc" / family.family / "soc.json")
        for controller in soc["controllers"].values():
            kind = controller["kind"]
            if kind not in kinds:
                raise ValueError("Provider kind has no authored/factory contract: " + kind)
            for mode in controller["modes"]:
                provider.mode_implementation(kind, mode)
                mode_traits(kind, mode)
    return {"kinds": sorted(kinds), "components": sorted(COMPONENTS),
            "families": [item.family for item in descriptor.families()]}


def resolve_catalogue(root=configure.ROOT):
    """Derive actual selected references; never infer broader hardware support."""
    root = Path(root)
    contract = check_contracts(root)
    catalogue = json.loads((root / CATALOGUE).read_text(encoding="utf-8"))
    if (not isinstance(catalogue, dict) or set(catalogue) != {"schema_version", "assemblies"}
            or type(catalogue["schema_version"]) is not int or catalogue["schema_version"] != 1):
        raise ValueError("Invalid maintained assembly catalogue")
    entries = catalogue["assemblies"]
    if not isinstance(entries, list) or not entries:
        raise ValueError("Maintained assembly catalogue must be a nonempty array")
    if any(not isinstance(name, str) or not re.fullmatch(r"[a-z0-9-]+\.toml", name)
           for name in entries) or len(set(entries)) != len(entries):
        raise ValueError("Invalid or duplicate maintained assembly catalogue entry")
    directory = root / "tools/configure/assemblies"
    # This checks the reference set, not C sources. A new reference requires an
    # explicit catalogue change so reviewers see its maintained scope.
    actual = {path.name for path in directory.glob("*.toml")}
    if set(entries) != actual:
        raise ValueError("Assembly catalogue differs from maintained TOML references: " +
                         str(sorted(set(entries) ^ actual)))
    references = []
    for name in entries:
        path = directory / name
        result = configure.resolve_ir(path, root)
        references.append({
            "assembly": path.relative_to(root).as_posix(),
            "board": result.board, "family": result.soc_family,
            "part": result["part"], "backend": result.backend,
            "optimization": result["optimization"],
            "configuration_sha256": result["configuration_sha256"],
            "selected": [{"id": item.id, "kind": item.kind,
                          "controller": item.controller, "mode": item.mode}
                         for item in result.controllers],
            "scope": "native_model" if result.soc_family == "native" else "public_reference",
            "physical_status": "not_executed",
            "pcb_revision": result["physical_pcb_revision"],
            "limitations": list(result["limitations"]),
        })
    return {"schema_version": 1, "identity_kind": "derived-software-support",
            "contracts": contract, "references": references,
            "physical_status": "not_executed"}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=configure.ROOT)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args(argv)
    try:
        report = resolve_catalogue(arguments.source_root)
        if arguments.output is not None:
            arguments.output.parent.mkdir(parents=True, exist_ok=True)
            arguments.output.write_text(json.dumps(
                report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        if arguments.check:
            print(f"Maintained contracts and {len(report['references'])} references passed")
        elif arguments.output is None:
            print(json.dumps(report, indent=2, sort_keys=True))
        return 0
    except (OSError, ValueError, TypeError, KeyError) as error:
        print(f"Support matrix rejected: {error}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
