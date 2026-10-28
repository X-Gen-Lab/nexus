"""Check maintained authoring/factory contracts and derive reference support.

This reports software selections and reviewed routes, never physical execution
or a source-target graph. Assemblies enter through one explicit catalogue.
"""
import argparse
import json
from pathlib import Path
import sys
import re
import subprocess
import tempfile

# Direct CLI execution binds only this checkout or relocated SDK.
if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.configure.providers.descriptor import source_directory
from tools.configure import authored, configure, factory, selected_contracts
from tools.configure.contracts import COMPONENTS
from tools.configure.providers import descriptor, maintained
from tools.configure.providers.common import mode_traits


CATALOGUE = "tools/configure/assemblies/catalogue.json"


def support_catalogue(root):
    """Reuse complete existing validation, including the real provider gate.

    Review sources identify the maintained input facts, not complete per-mode
    errata audits. A missing route remains missing. Profile/Board reservations
    are still checked by the normal resolver when an actual instance is chosen.
    """
    rows = []
    for integration in descriptor.families():
        family = integration.family
        if family == "native":
            continue
        provider = maintained(family)
        soc = configure.load(root / source_directory(family) / "soc.json")
        routes = configure.load(root / source_directory(family) / "routes.json")
        for part in sorted(soc["variants"]):
            configure.validate_soc(soc, part)
            reviewed = configure.resolve_routes(routes, soc, part, provider)
            for controller, facts in sorted(soc["controllers"].items()):
                interfaces = [(facts["kind"], facts["modes"])]
                interfaces += sorted(facts.get("interfaces", {}).items())
                for kind, modes in interfaces:
                    for mode in sorted(modes):
                        implementation = provider.mode_implementation(
                            kind, mode)
                        mode_traits(kind, mode)
                        matches = tuple(sorted(route["id"] for route in
                            reviewed.values() if part in route["variants"] and
                            route["controller"] == controller and
                            route["kind"] == kind and mode in route["modes"]))
                        rows.append(selected_contracts.SupportContract(
                            family, part, controller, kind, mode,
                            implementation,
                            matches, integration.fact_files))
    return tuple(rows)



def component_choices(root):
    """Execute the sole target map instead of interpreting CMake syntax."""
    with tempfile.TemporaryDirectory(
            prefix="nexus-component-choices-") as value:
        directory = Path(value)
        output = directory / "choices"
        script = directory / "query.cmake"
        script.write_text(
            'include("${NEXUS_COMPONENT_MODULE}")\n'
            'nexus_component_choices(choices)\n'
            'file(WRITE "${NEXUS_COMPONENT_OUTPUT}" "${choices}")\n',
            encoding="utf-8")
        try:
            result = subprocess.run(
                ["cmake", "-DNEXUS_COMPONENT_MODULE=" + str(
                    root / "cmake/platform/Components.cmake"),
                 "-DNEXUS_COMPONENT_OUTPUT=" + str(output),
                 "-P", str(script)],
                capture_output=True, text=True, timeout=20, check=False)
        except (OSError, subprocess.SubprocessError) as error:
            raise ValueError("Cannot query CMake component choices: " +
                             str(error)) from error
        if result.returncode:
            raise ValueError("CMake component choices rejected: " +
                             result.stderr)
        choices = output.read_text(encoding="utf-8")
    return frozenset(choices.split(";")) if choices else frozenset()


def check_contracts(root=configure.ROOT):
    """Reject missing authoring, factory, provider and CMake selection entries."""
    kinds = frozenset(authored.KINDS)
    if kinds != frozenset(factory.KINDS) or kinds != configure.KINDS:
        raise ValueError("Controller kinds differ across authoring/resolver/factory")
    properties = authored.editor_schema()["properties"]
    if not kinds.issubset(properties):
        raise ValueError("Editor schema is missing authored controller kinds")
    selected = component_choices(root)
    if selected != COMPONENTS:
        raise ValueError("Maintained components differ from CMake target selection")
    for family in descriptor.families():
        provider = maintained(family.family)
        soc = configure.load(root / source_directory(family.family) / "soc.json")
        for part in soc["variants"]:
            configure.validate_soc(soc, part)
        for controller in soc["controllers"].values():
            interfaces = [(controller["kind"], controller["modes"])]
            interfaces += sorted(controller.get("interfaces", {}).items())
            for kind, modes in interfaces:
                if kind not in kinds:
                    raise ValueError("Provider kind has no authored/factory contract: " + kind)
                for mode in modes:
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
            "part": result.part, "backend": result.backend,
            "optimization": result.optimization,
            "configuration_sha256": result.configuration_sha256,
            "selected": [{"id": item.id, "kind": item.kind,
                          "controller": item.controller, "mode": item.mode}
                         for item in result.controllers],
            "scope": "native_model" if result.soc_family == "native" else "public_reference",
            "physical_status": "not_executed",
            "pcb_revision": result.physical_pcb_revision,
            "limitations": list(result.limitations),
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
