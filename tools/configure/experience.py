"""Read-only developer views over the existing configuration authorities.

These views never choose an alternative mode, supply a source dependency graph,
or turn hardware facts into qualification. Generation still consumes typed IR.
"""

import argparse
from dataclasses import asdict
import hashlib
import json
import os
from pathlib import Path
import sys

try:
    from . import selected_contracts
    from .providers import maintained
    from .providers.common import mode_traits, route_dma
    from .resource_graph import json_value, normalize_resource
except ImportError:
    import selected_contracts
    from providers import maintained
    from providers.common import mode_traits, route_dma
    from resource_graph import json_value, normalize_resource


def project(result):
    """Return the JSON projection without mutating the immutable resolution."""
    return result.to_dict() if hasattr(result, "to_dict") else result


def physical_scope(data):
    """Configuration never substitutes for a station's physical evidence."""
    return ("not_applicable" if data["soc_family"] == "native" else
            "not_established_by_configuration")


def limitations(data):
    """Do not display the legacy IR placeholder as a station observation."""
    return [item for item in data.get("limitations", [])
            if item != "Physical qualification not executed"]


def describe(result):
    """Explain decisions and obligations; budgets remain distinct from costs."""
    data = project(result)
    contracts = ({item.id: selected_contracts.describe(item)
                  for item in result.controllers}
                 if hasattr(result, "controllers") else {})
    clock = data["clock"]
    lines = [f"Platform: {data['part']} / {data['board']}",
             f"Family: {data['soc_family']}  Backend: {data['backend']}",
             f"Clock: {data['clock_profile']} ({clock['core_hz']} Hz core)",
             f"Optimization: {data['optimization']}",
             f"Configuration: {data['configuration_sha256']}",
             "Physical verification: " + physical_scope(data),
             "Consult exact-board station records for physical evidence.",
             "", "Controllers:"]
    for item in data["controllers"]:
        lines.append(f"  {item['kind']}.{item['id']}: {item['controller']} / "
                     f"{item['mode']} (binding {item['binding']})")
        contract = contracts.get(item["id"])
        if contract is not None:
            lines.append(f"    Public face: {contract.public_class}; "
                         f"features={','.join(contract.features) or 'none'}")
            lines.append(f"    Transfer: unit={contract.unit}; "
                         f"unit_bytes={contract.unit_bytes}; "
                         f"alignment={contract.buffer_alignment}; "
                         f"maximum={contract.max_transfer_units}")
            lines.append(f"    Continuity: {contract.continuity}; "
                         f"abort_scope={contract.abort_scope}; "
                         f"DMA policy={contract.dma_policy}")
        lines.append("    Pins: " + ", ".join(
            f"{pin['pin']}:{pin['function']}" for pin in item["pins"]))
        if item.get("dma"):
            lines.append("    DMA: " + ", ".join(item["dma"]))
        if "irq_priority" in item:
            lines.append(f"    IRQ priority: {item['irq_priority']}; "
                         f"calls_os: {item.get('calls_os', False)}")
        for field in ("baud", "max_hz", "rx_capacity", "event_capacity",
                      "stream_resolution", "timing_resolution"):
            if field in item:
                lines.append(f"    {field}: " + json.dumps(item[field],
                                                           sort_keys=True))
    if not data["controllers"]:
        lines.append("  none selected")
    lines.extend(("", "Endpoints:"))
    for item in data["devices"]:
        lines.append(f"  {item['id']}: {item['controller']} / {item['driver']}")
    if not data["devices"]:
        lines.append("  none selected")
    lines.extend(("", "Resource owners:"))
    for claim in data["claims"]:
        lines.append(f"  {claim['key']}: {claim['owner']} "
                     f"({claim['mode']}; source {claim['source']})")
    lines.extend(("", "Memory domains:"))
    for region in data["memory"]:
        lines.append(f"  {region['id']}: 0x{region['origin']:08x}, "
                     f"{region['size']} bytes; DMA=" + json.dumps(
                         region.get("dma_engines", region.get("dma")),
                         sort_keys=True))
    lines.extend(("", "Declared budgets (not measured ELF or stack costs):"))
    for name, value in sorted(data["memory_budgets"].items()):
        lines.append(f"  {name}: " + json.dumps(value, sort_keys=True))
    lines.extend(("", "Execution responsibility:",
                  "  The product owns service, waits, queues and shutdown.",
                  "  Cancel or timeout does not return borrowed storage; "
                  "observe SETTLED.", "", "Limitations:"))
    lines.extend("  " + text for text in limitations(data))
    lines.extend(("", "Reviewed input identities:"))
    lines.extend("  " + item["path"] for item in data["inputs"])
    return "\n".join(lines)


def difference_impacts(before, after, changes):
    """Explain changed public contracts without estimating a linked cost."""
    impacts = []

    def append(category, path, old, new, obligation):
        impacts.append({"category": category, "path": path,
                        "before": json_value(old), "after": json_value(new),
                        "obligation": obligation})

    for change in changes:
        path = change["path"]
        field = path.rsplit(".", 1)[-1]
        if (path.startswith("memory_budgets.") or
                field.endswith(("_capacity", "_bytes")) or
                field in {"rx_count", "tx_count", "blocks"}):
            append("storage", path, change["before"], change["after"],
                   "Review caller storage and measure the linked image; "
                   "declared capacity is not a measured saving.")

    if not all(hasattr(item, "controllers") for item in (before, after)):
        return impacts

    def faces(result):
        controllers = {item.id: item for item in result.controllers}
        records = {"controllers." + item.id: selected_contracts.describe(item)
                   for item in result.controllers}
        records.update({"devices." + item.id: selected_contracts.describe(
            controllers[item.controller], item) for item in result.devices})
        return records

    def factory_ids(result):
        groups = {}
        for record in selected_contracts.for_configuration(result):
            groups.setdefault(record.public_class, []).append(record.id)
        return groups

    old_ids, new_ids = factory_ids(before), factory_ids(after)
    for name in sorted(old_ids.keys() | new_ids.keys()):
        old, new = old_ids.get(name, []), new_ids.get(name, [])
        if old != new:
            append("factory_ids", "factory_ids." + name, old, new,
                   "Recompile consumers; generated numeric IDs belong to "
                   "this exact assembly and must not be persisted as ABI.")

    old_faces, new_faces = faces(before), faces(after)
    transfer_fields = ("unit", "unit_bytes", "buffer_alignment",
                       "max_transfer_units", "max_messages",
                       "max_segment_units", "max_segments", "max_total_units",
                       "word_bits")
    execution_fields = ("mode", "model", "features", "abort_scope",
                        "continuity", "timestamp_source", "dma_policy",
                        "cache_line_bytes", "dma_engines")
    for path in sorted(old_faces.keys() | new_faces.keys()):
        old, new = old_faces.get(path), new_faces.get(path)
        old_class = old.public_class if old is not None else None
        new_class = new.public_class if new is not None else None
        if old_class != new_class:
            append("public_interface", path, old_class, new_class,
                   "Use the selected typed face and factory namespace; "
                   "do not cast between nominal interfaces.")
        for category, fields, obligation in (
            ("transfer_contract", transfer_fields,
             "Review payload units, bounds and alignment before reusing "
             "request or buffer storage."),
            ("execution", execution_fields,
             "Review the product executor, service, DMA/cache and stop "
             "sequence; cancel does not release borrowed storage."),
        ):
            left = ({key: getattr(old, key) for key in fields}
                    if old is not None else None)
            right = ({key: getattr(new, key) for key in fields}
                     if new is not None else None)
            if left != right:
                append(category, path, left, right, obligation)
    return impacts


def differences(before, after):
    """Compare effective decisions rather than source-path/hash churn."""
    old, new = project(before), project(after)
    fields = ("part", "board", "soc_family", "backend", "cpu_profile",
              "kernel_profile", "clock_profile", "clock", "clock_plan",
              "optimization", "controllers", "devices", "components",
              "claims", "memory", "flash", "layout", "memory_budgets",
              "factory", "runtime_power", "i2s2", "trigger_plan",
              "resource_plan", "pwm_shared_owners", "dma_topology")
    changes = []
    missing = object()

    def named(values):
        return (isinstance(values, list) and
                all(isinstance(item, dict) and isinstance(item.get("id"), str)
                    for item in values) and
                len({item["id"] for item in values}) == len(values))

    def compare(path, left, right):
        if type(left) is type(right) and left == right:
            return
        if isinstance(left, dict) and isinstance(right, dict):
            for name in sorted(left.keys() | right.keys()):
                compare(path + "." + name, left.get(name, missing),
                        right.get(name, missing))
        elif named(left) and named(right):
            # Identity makes a storage/mode change readable; order still
            # matters to emitted enum values and initialization sequencing.
            compare(path + ".$order", [item["id"] for item in left],
                    [item["id"] for item in right])
            compare(path, {item["id"]: item for item in left},
                    {item["id"]: item for item in right})
        else:
            changes.append({"path": path,
                            "before_present": left is not missing,
                            "after_present": right is not missing,
                            "before": None if left is missing else left,
                            "after": None if right is missing else right})

    for field in fields:
        compare(field, old.get(field, missing), new.get(field, missing))
    return {"schema_version": 1,
            "before_configuration": old["configuration_sha256"],
            "after_configuration": new["configuration_sha256"],
            "measurement": "declared configuration, not measured",
            "changes": changes,
            "impacts": difference_impacts(before, after, changes)}


def board_catalogue(gate, root, *, board=None, board_package=None):
    """Read reviewed routes without inventing a backend, clock or assembly."""
    root = root.resolve()
    gate.sdk.verify(root)
    if board is not None:
        if not gate.IDENTIFIER.fullmatch(board):
            gate.fail("Board id cannot be a path; use explicit board_package")
        directory = root / "boards" / board
    else:
        directory = Path(board_package)
    directory = gate.board_directory(directory)
    snapshots = {}
    read = lambda path: gate.load(path, snapshots)
    board_data = gate.obj(read(directory / "board.json"),
        {"schema_version", "id", "soc", "soc_family", "source_revision",
         "physical_pcb_revision", "clocks", "bindings", "reserved_resources",
         "unknowns", "provenance"}, {"inputs"}, "Board")
    gate.integer(board_data["schema_version"], 1, 1, "Board schema")
    gate.identity(board_data["id"], "Board id")
    gate.validate_board_context(board_data)
    family = gate.identity(board_data["soc_family"], "SoC family")
    provider = gate.maintained(family)
    part = gate.identity(board_data["soc"], "exact part")
    soc_dir = root / gate.descriptor.maintained(family).source_directory
    if (soc_dir.is_symlink() or
            not soc_dir.resolve().is_relative_to(root / "soc")):
        gate.fail("SoC package escapes owned source")
    soc = read(gate.contained_file(soc_dir, "soc.json"))
    gate.validate_soc(soc, part)
    if soc["id"] != family:
        gate.fail("Board SoC family differs from the package identity")
    routes_doc = gate.obj(read(gate.contained_file(soc_dir, "routes.json")),
                          {"schema_version", "routes"}, (), "routes")
    gate.integer(routes_doc["schema_version"], 1, 1, "routes schema")
    routes = gate.resolve_routes(routes_doc, soc, part, provider)
    bindings = gate.resolve_bindings(board_data, routes, part)
    reservations = gate.reserved_claims(soc, board_data, provider)
    return {"board": board_data["id"], "part": part, "soc_family": family,
            "board_bindings": bindings, "routes": routes,
            "claims": reservations,
            "controller_facts": soc["controllers"],
            "controllers": [], "limitations": board_data["unknowns"],
            "physical_pcb_revision": board_data["physical_pcb_revision"],
            "board_clocks": board_data["clocks"],
            "physical_qualified": False,
            "fact_sources": [str(path) for path in snapshots]}


def binding_facts(gate, result, root):
    """Read the exact SoC bytes bound by IR without changing its identity."""
    data = project(result)
    relative = (gate.descriptor.maintained(data["soc_family"]).source_directory
                + "/soc.json")
    expected = next((item["sha256"] for item in data["inputs"]
                     if item["path"] == relative), None)
    path = gate.contained_file(root.resolve(), relative)
    snapshot = {}
    facts = gate.load(path, snapshot)
    actual = hashlib.sha256(snapshot[path]).hexdigest()
    if expected is None or actual != expected:
        gate.fail("Binding query SoC facts differ from the resolved input: "
                  + str(path))
    return {**data, "controller_facts": facts["controllers"]}


def bindings_view(result, *, kind=None):
    """Show routing candidates and current ownership, not mode admission."""
    data = project(result)
    provider = maintained(data["soc_family"])
    rows = []
    for name, binding in sorted(data["board_bindings"].items()):
        route = data["routes"][binding["route"]]
        if kind is not None and route["kind"] != kind:
            continue
        keys = ({"controller:" + route["controller"]}
                if route["kind"] != "gpio" else set())
        keys.update("pin:" + pin["pin"] for pin in route["pins"])
        claims = data.get("resource_plan", {}).get("claims", data["claims"])

        def occupied_resources(resources):
            return [claim for claim in claims if any(
                claim["key"] == key or claim["key"].startswith(key + "/") or
                key.startswith(claim["key"] + "/") for key in resources)]

        occupied = occupied_resources(keys)
        selected = [item["id"] for item in data["controllers"]
                    if item["binding"] == name]
        modes = {}
        for mode in route["modes"]:
            shape = mode_traits(route["kind"], mode)
            hardware = data.get("controller_facts", {}).get(
                route["controller"], {})
            dma = route_dma(route, mode) if shape.dma else []
            resources = (hardware.get("resources", []) +
                         route.get("resources", []) +
                         route.get("mode_resources", {}).get(mode, []))
            resource_keys = set(keys)
            resource_keys.update(normalize_resource(item) for item in resources)
            if shape.irq:
                resource_keys.update(normalize_resource("irq:" + item)
                                     for item in hardware.get("irq", []))
            for lane in dma:
                resource_keys.add(normalize_resource("dma:" + lane))
                resource_keys.add(normalize_resource(provider.dma_irq(lane)))
            modes[mode] = {
                "implementation": provider.mode_implementation(
                    route["kind"], mode),
                "model": provider.MODEL,
                "execution": asdict(shape), "dma": dma,
                "resources": resources, "resource_keys": sorted(resource_keys),
                "occupied_resources": occupied_resources(resource_keys),
                "admission": "configuration-required",
                "resource_scope": "reviewed mode facts; selected options "
                "and complete reservation checks still required"}
        rows.append({"binding": name, "kind": route["kind"],
                     "controller": route["controller"], "route": route["id"],
                     "modes": route["modes"], "pins": route["pins"],
                     "selected_by": selected, "occupied_resources": occupied,
                     "mode_contracts": modes,
                     "admission": "configuration-required"})
    return {"schema_version": 1, "board": data["board"], "part": data["part"],
            "physical_verification": physical_scope(data), "bindings": rows,
            "selection": {"family": data["soc_family"],
                          "backend": data.get("backend"),
                          "clock_profile": data.get("clock_profile"),
                          "physical_pcb_revision": data.get(
                              "physical_pcb_revision")},
            "fact_sources": data.get("fact_sources", [
                item["path"] for item in data.get("inputs", [])]),
            "limitations": limitations(data)}


def render_bindings(view):
    lines = [f"Board: {view['board']} ({view['part']})",
             "Physical verification: " + view["physical_verification"],
             "Consult exact-board station records for physical evidence.",
             "Routes are candidates; complete configuration is required."]
    for row in view["bindings"]:
        lines.append(f"{row['binding']}: {row['kind']} {row['controller']} "
                     f"modes={','.join(row['modes'])} "
                     f"[{row['admission']}]")
        if row["selected_by"]:
            lines.append("  Selected by: " + ", ".join(row["selected_by"]))
        for item in row["occupied_resources"]:
            lines.append(f"  Occupied {item['key']}: {item['owner']} "
                         f"(source {item['source']})")
        for mode, contract in row["mode_contracts"].items():
            lines.append("  " + mode + " resources: " +
                         ", ".join(contract["resource_keys"]))
            for item in contract["occupied_resources"]:
                lines.append(f"    {item['key']}: {item['owner']} "
                             f"(source {item['source']})")
    return "\n".join(lines)


def init_contents(gate, args):
    """Render explicit small profiles; normal resolution admits the result."""
    quoted = lambda value: json.dumps(value, ensure_ascii=True)
    if (args.board is None) == (args.board_package is None):
        gate.fail("Select exactly one --board or --board-package")
    if args.board_package is not None:
        package = gate.board_directory(args.board_package)
        relative = os.path.relpath(package, args.output.parent.resolve())
        board_line = "board_package = " + quoted(Path(relative).as_posix())
    else:
        board_line = "board = " + quoted(args.board)
    lines = ["schema = 2", board_line,
             "backend = " + quoted(args.backend),
             "clock = " + quoted(args.clock),
             "", "[memory]", "main_stack_bytes = 2048", "libc_heap_bytes = 0"]
    if args.profile != "empty":
        if args.binding is None:
            gate.fail("A nonempty profile requires an explicit --binding")
        gate.c_identity(args.instance)
        mode = "output" if args.profile == "gpio" else "irq"
        lines += ["", f"[{args.profile}.{args.instance}]",
                  "binding = " + quoted(args.binding),
                  "mode = " + quoted(mode)]
        if args.profile == "uart":
            lines += ["baud = 115200", f"[{args.profile}.{args.instance}.rx]",
                      'format = "events"', "capacity = 64",
                      f"[{args.profile}.{args.instance}.irq]", "priority = 5",
                      "calls_os = false"]
    elif args.binding is not None:
        gate.fail("Empty profile has no binding")
    return "\n".join(lines) + "\n"


def run(gate, argv=None):
    """Keep the existing command entry while exposing explicit read views."""
    arguments = list(sys.argv[1:] if argv is None else argv)
    commands = {"generate", "check", "explain", "list-bindings", "init",
                "diff", "provider-check"}
    command = (arguments.pop(0) if arguments and arguments[0] in commands
               else "generate")
    parser = argparse.ArgumentParser(description=gate.__doc__)
    parser.add_argument("--source-root", type=Path, default=gate.ROOT)
    parser.add_argument("--diagnostics-json", action="store_true",
                        help="write configuration rejections as JSON on stderr")
    parser.add_argument("--assembly", type=Path,
                        required=command not in {"init", "list-bindings"})
    parser.add_argument("--output", type=Path,
                        required=command in {"generate", "init"})
    if command in {"explain", "list-bindings", "diff", "provider-check"}:
        parser.add_argument("--json", action="store_true")
    if command in {"init", "list-bindings"}:
        parser.add_argument("--board")
        parser.add_argument("--board-package", type=Path)
    if command == "list-bindings":
        parser.add_argument("--kind", choices=sorted(gate.KINDS))
    if command == "init":
        parser.add_argument(
            "--backend", required=True,
            choices=("native", "baremetal", "freertos", "threadx"))
        parser.add_argument("--clock", required=True)
        parser.add_argument("--profile", choices=("empty", "gpio", "uart"),
                            default="empty")
        parser.add_argument("--binding")
        parser.add_argument("--instance", default="device0")
    if command == "diff":
        parser.add_argument("--against", required=True, type=Path)
    if command == "provider-check":
        parser.add_argument("--expect-family")
        parser.add_argument("--expect-part")
    args = parser.parse_args(arguments)
    context = args.assembly

    def resolve(path):
        nonlocal context
        context = path
        return gate.resolve_ir(path, args.source_root)

    try:
        if command == "init":
            context = args.output
            if args.output.exists() or args.output.is_symlink():
                gate.fail("init refuses to overwrite an existing "
                          "authored configuration")
            if args.output.suffix != ".toml":
                gate.fail("init output must be authored TOML (.toml)")
            contents = init_contents(gate, args)
            args.output.parent.mkdir(parents=True, exist_ok=True)
            # Own the path only after exclusive create succeeds. A failed
            # write or validation then removes only our incomplete file.
            stream = args.output.open("x", encoding="utf-8")
            try:
                with stream:
                    stream.write(contents)
                result = resolve(args.output)
            except BaseException:
                args.output.unlink(missing_ok=True)
                raise
        elif command == "generate":
            result = gate.configure(args.assembly, args.output,
                                    args.source_root)
        elif command == "list-bindings":
            if sum(item is not None for item in (
                    args.assembly, args.board, args.board_package)) != 1:
                gate.fail("Select exactly one --assembly, --board "
                          "or --board-package")
            if args.assembly is None:
                directory = (args.board_package if args.board_package else
                             args.source_root / "boards" / args.board)
                context = directory / "board.json"
            result = (resolve(args.assembly) if args.assembly is not None else
                      board_catalogue(gate, args.source_root, board=args.board,
                                      board_package=args.board_package))
            if args.assembly is not None:
                result = binding_facts(gate, result, args.source_root)
        else:
            result = resolve(args.assembly)
        if command == "provider-check":
            try:
                from . import onboarding
            except ImportError:
                import onboarding
            report = onboarding.check(
                result, expected_family=args.expect_family,
                expected_part=args.expect_part)
            if args.json:
                print(json.dumps(report, indent=2, sort_keys=True))
            else:
                print("Provider configuration checked: " + report["family"] +
                      "/" + report["part"] + "/" + report["board"])
                print("Checks: " + ", ".join(report["checks"]))
                print("Physical verification: " +
                      report["physical_verification"])
                print(report["scope"])
        elif command == "explain":
            print(json.dumps(project(result), indent=2, sort_keys=True)
                  if args.json else describe(result))
        elif command == "list-bindings":
            view = bindings_view(result, kind=args.kind)
            print(json.dumps(view, indent=2, sort_keys=True)
                  if args.json else render_bindings(view))
        elif command == "diff":
            delta = differences(result, resolve(args.against))
            if args.json:
                print(json.dumps(delta, indent=2, sort_keys=True))
            else:
                print("Configuration changes (declared, not measured):")
                for item in delta["changes"]:
                    old = (json.dumps(item["before"], sort_keys=True)
                           if item["before_present"] else "<absent>")
                    new = (json.dumps(item["after"], sort_keys=True)
                           if item["after_present"] else "<absent>")
                    print(item["path"] + ": " + old + " -> " + new)
                if not delta["changes"]:
                    print("  no effective changes")
                for impact in delta["impacts"]:
                    print("  " + impact["category"] + " " + impact["path"] +
                          ": " + impact["obligation"])
        else:
            print(f"Resolved {result['part']}/{result['board']}/"
                  f"{result['backend']}: "
                  f"{result['configuration_sha256']}")
        return 0
    except (gate.ConfigurationError, OSError, ValueError,
            TypeError, KeyError) as error:
        if args.diagnostics_json:
            diagnostic = (error.diagnostic(context) if isinstance(
                error, gate.ConfigurationError) else
                {"code": "input_error", "message": str(error),
                 "source": str(context) if context is not None else None})
            print(json.dumps(diagnostic, sort_keys=True), file=sys.stderr)
        else:
            source = f" [{context}]" if context is not None else ""
            print(f"Nexus configuration rejected{source}: {error}",
                  file=sys.stderr)
        return 1
