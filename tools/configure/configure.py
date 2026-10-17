#!/usr/bin/env python3
"""Resolve reviewed hardware facts and emit one atomic configuration bundle.

CMake remains the authority for source targets. This tool never discovers source
code, creates workers, selects application policy, or invents a hardware route.
"""
import argparse
from contextlib import contextmanager
from functools import lru_cache
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tomllib

try:
    from . import cpu as cpu_profiles
    from . import bundle, dependencies, sdk, uart_formats
    from . import resource_graph, trigger_graph, dma_topology
    from .contracts import COMPONENTS
    from . import kernel, memory as memory_regions, metadata, factory, can, threadx, usb_device, ethernet, timer_advanced, secondary_controllers, fact_references, clocks, physical_facts
    from .ir import thaw
    from .providers import descriptor, maintained
    from .providers.common import (ConfigurationError, cpu_abi, fail, integer,
                                   mode_traits, route_dma, validate_clock_profiles,
                                   resolve_interrupts, sequence,
                                   validate_selection)
except ImportError:
    import cpu as cpu_profiles
    import bundle
    import uart_formats
    import dependencies
    import sdk
    import resource_graph
    import trigger_graph
    import dma_topology
    from contracts import COMPONENTS
    import kernel
    import can
    import threadx
    import usb_device
    import ethernet
    import timer_advanced
    import secondary_controllers
    import fact_references
    import clocks
    import physical_facts
    import metadata
    import factory
    import memory as memory_regions
    from ir import thaw
    from providers import descriptor, maintained
    from providers.common import (ConfigurationError, cpu_abi, fail, integer,
                                   mode_traits, route_dma, validate_clock_profiles,
                                  resolve_interrupts, sequence,
                                  validate_selection)

ROOT = Path(__file__).resolve().parents[2]
UINT32_MAX = (1 << 32) - 1
IDENTIFIER = re.compile(r"[A-Za-z][A-Za-z0-9_-]{0,63}")
PIN = physical_facts.GPIO_PIN
KINDS = {"gpio", "uart", "spi", "i2c", "flash", "watchdog", "exti",
         "pwm", "adc", "can", "usb_device", "eth", "eth_frames",
         "timer_capture", "advanced_pwm", "pwm_group", "encoder", "pwm_adc", "can_fd", "adc_group"}
C_KEYWORDS = set("auto break case char const continue default do double else enum extern float for goto if inline int long register restrict return short signed sizeof static struct switch typedef union unsigned void volatile while _Alignas _Alignof _Atomic _Bool _Complex _Generic _Imaginary _Noreturn _Static_assert _Thread_local main".split())


@contextmanager
def error_context(context):
    """Attach the authored instance while retaining the precise field failure."""
    try:
        yield
    except ConfigurationError as error:
        raise ConfigurationError(f"{context}: {error}") from error


def c_identity(value):
    identifier = c_id(value)
    if identifier.upper() == "COUNT":
        fail(f"Reserved factory identifier: {value}")
    if identifier in C_KEYWORDS:
        fail(f"Reserved C identifier: {value}")
    return identifier


def dependency_identity(root, relative, expected=None):
    """Validate checkout commits, or the verified installed SDK identities."""
    manifest = root / ".nexus-source-sdk.json"
    if manifest.is_file():
        return sdk_dependency_identity(sdk.verify(root), relative, expected)
    else:
        checkout = root / relative
        if not (checkout / ".git").exists():
            fail(f"Initialize the locked SDK dependency: {relative}")
        environment = dict(os.environ)
        local = subprocess.run(["git", "rev-parse", "--local-env-vars"],
                               cwd=root, capture_output=True, text=True)
        if local.returncode:
            fail(f"Cannot verify SDK dependency: {relative}")
        # Hooks select their parent repository through these variables. Each
        # explicit -C checkout must instead observe its own Git identity.
        for name in local.stdout.splitlines():
            environment.pop(name, None)
        def git(directory, *arguments):
            command = subprocess.run(["git", "-C", str(directory), *arguments],
                                     capture_output=True, text=True,
                                     env=environment)
            if command.returncode:
                fail(f"Cannot verify SDK dependency: {relative}")
            return command.stdout.strip()
        commit = git(checkout, "rev-parse", "HEAD")
        entry = git(root, "ls-tree", "HEAD", "--", relative).split()
        if len(entry) != 4 or entry[:2] != ["160000", "commit"] or entry[2] != commit:
            fail(f"SDK dependency differs from the locked Gitlink: {relative}")
        if git(checkout, "status", "--porcelain", "--untracked-files=normal"):
            fail(f"SDK dependency has unrecorded changes: {relative}")
    if expected is not None and commit != expected:
        fail(f"SDK source lock contradicts actual dependency: {relative}")
    return {"path": relative, "commit": commit}


def sdk_dependency_identity(verified, relative, expected=None):
    """Consume only the caller's complete verification from this resolution."""
    record = next((item for item in verified["dependencies"]
                   if item["path"] == relative), None)
    if record is None:
        fail(f"Installed SDK has no source identity: {relative}")
    commit = record["commit"]
    if expected is not None and commit != expected:
        fail(f"SDK source lock contradicts actual dependency: {relative}")
    return {"path": relative, "commit": commit}


@lru_cache(maxsize=1)
def vendor_source_verifier():
    """Reuse the owned import verifier; installed SDKs ship the same script."""
    script = ROOT / "scripts/ci/verify_vendor_source.py"
    spec = importlib.util.spec_from_file_location("nexus_vendor_source", script)
    if spec is None or spec.loader is None:
        fail("Imported SDK verifier is unavailable")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.verify


def imported_dependency_identity(root, relative, read):
    """Bind the verified import, without inventing an upstream Git commit."""
    lock_path = contained_file(root, relative)
    lock = read(lock_path)
    if lock_path.name != "source.lock.json":
        fail("Imported SDK requires its reviewed source.lock.json")
    for key in ("vendor", "package", "version"):
        text(lock.get(key), "Imported SDK " + key)
    try:
        verified = vendor_source_verifier()(lock_path.parent)
    except (OSError, ValueError, TypeError) as error:
        fail(str(error))
    return {"kind": "vendor-source-import",
            "path": lock_path.parent.relative_to(root).as_posix(),
            "lock_sha256": file_digest(lock_path),
            **verified}


def validate_resource_fact(value, provider, context):
    """Validate the full physical fact before discarding DMA selectors."""
    value = text(value, context)
    if value.startswith("dma:"):
        provider.validate_dma_resource(value)
    return normalize_resource(value)


def pairs(items):
    result = {}
    for key, value in items:
        if key in result:
            raise ConfigurationError(f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def load(path, snapshots=None):
    if path.is_symlink() or not path.is_file():
        raise ConfigurationError(f"Input must be a regular file: {path}")
    contents = path.read_bytes()
    if snapshots is not None:
        snapshots[path] = contents
    try:
        return json.loads(contents.decode("utf-8"), object_pairs_hook=pairs,
                          parse_constant=lambda value: fail(f"Invalid number {value}"))
    except (ValueError, UnicodeDecodeError) as error:
        raise ConfigurationError(f"{path}: {error}") from error


def obj(value, required, optional=(), context="object"):
    if not isinstance(value, dict):
        fail(f"{context}: expected object")
    missing = set(required) - value.keys()
    unknown = value.keys() - set(required) - set(optional)
    if missing or unknown:
        fail(f"{context}: missing {sorted(missing)}, unknown {sorted(unknown)}")
    return value


def text(value, context, empty=False):
    if not isinstance(value, str) or (not empty and not value):
        fail(f"{context}: expected nonempty string")
    if "\0" in value:
        fail(f"{context}: NUL is forbidden")
    return value


def identity(value, context):
    text(value, context)
    if not IDENTIFIER.fullmatch(value):
        fail(f"{context}: invalid identifier {value!r}")
    return value


def unique(items, context):
    if len(set(items)) != len(items):
        fail(f"{context}: duplicate values")


def address_range(origin, size, context):
    integer(origin, context=f"{context}.origin")
    integer(size, 1, context=f"{context}.size")
    if origin + size > UINT32_MAX + 1:
        fail(f"{context}: address overflow")
    return origin, origin + size


def file_digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def contained_file(package, name):
    text(name, "declared input")
    rel = Path(name)
    if rel.is_absolute() or ".." in rel.parts or rel.as_posix() != name:
        fail(f"Package input escapes containment: {name}")
    path = package / rel
    if any((package / Path(*rel.parts[:n])).is_symlink()
           for n in range(1, len(rel.parts) + 1)):
        fail(f"Package input follows symlink: {name}")
    if not path.is_file() or not path.resolve().is_relative_to(package):
        fail(f"Missing package input: {name}")
    return path


def validate_soc(soc, part):
    obj(soc, {"schema_version", "id", "variants", "cpu", "irq", "clock_profiles",
              "controllers"}, {"provenance", "source_lock", "reserved_resources",
                               "can_domain", "references", "adc_group_review"},
        "SoC")
    integer(soc["schema_version"], 1, 1, "SoC schema")
    identity(soc["id"], "SoC id")
    if not isinstance(soc["variants"], dict) or part not in soc["variants"]:
        fail(f"Unmaintained exact part: {part}")
    provider = maintained(soc["id"])
    reference = getattr(provider, "FACT_REFERENCES", None)
    if reference is not None and soc.get("references") != reference:
        fail("SoC references differ from the maintained source index")
    # Review every physical package, including unselected variants referenced by
    # route facts. A controller pad is not evidence that a package bonds it out.
    for exact, package in soc["variants"].items():
        identity(exact, "variant")
        if not isinstance(package, dict):
            fail("Physical variant must be an object")
        if provider.MODEL and "pins" not in package:
            continue
        pins = package.get("pins")
        physical_facts.package_pins(pins, package.get("pad_conditions", {}),
                                    getattr(provider, "CONDITIONAL_PACKAGE_PADS", ()))
    variant = obj(soc["variants"][part], {"package", "flash", "memory"},
                  {"pins", "pad_conditions"}, "variant")
    text(variant["package"], "package")
    flash = obj(variant["flash"], {"origin", "size", "erase_blocks"}, (),
                "physical Flash")
    address_range(flash["origin"], flash["size"], "Flash")
    blocks = sequence(flash["erase_blocks"], "Flash erase blocks")
    if not blocks or sum(integer(size, 1, context="erase block")
                         for size in blocks) != flash["size"]:
        fail("Flash erase geometry does not cover exact physical Flash")
    memory = sequence(variant["memory"], "memory domains")
    if not memory:
        fail("At least one memory domain is required")
    unique([region.get("id") for region in memory], "memory IDs")
    ranges = [(flash["origin"], flash["origin"] + flash["size"], "Flash")]
    for region in memory:
        obj(region, {"id", "origin", "size", "linker"},
            {"dma", "dma_engines"}, "memory")
        identity(region["id"], "memory id")
        physical_facts.memory_access(region,
                                     getattr(provider, "DMA_MEMORY_ENGINES", ()))
        if type(region["linker"]) is not bool:
            fail("Memory linker facts must be boolean")
        start, end = address_range(region["origin"], region["size"], region["id"])
        if any(start < old_end and old_start < end for old_start, old_end, _ in ranges):
            fail(f"Overlapping physical memory domain: {region['id']}")
        ranges.append((start, end, region["id"]))
    if sum(region["linker"] for region in memory) != 1:
        fail("Exactly one maintained default RAM linker domain is required")
    cpu = obj(soc["cpu"], cpu_profiles.CPU_FIELDS,
              cpu_profiles.CPU_OPTIONAL_FIELDS,
              "CPU facts")
    if cpu_abi(cpu) != provider.CPU_ABI:
        fail(f"Unsupported CPU/FPU ABI: {cpu}")
    if (type(cpu["dwt_cyccnt"]) is not bool or
            cpu["dwt_cyccnt"] != provider.DWT_CYCCNT):
        fail("DWT cycle counter differs from maintained CPU facts")
    if {key: cpu.get(key) for key in provider.CPU_FEATURES} != provider.CPU_FEATURES:
        fail("Optional CPU capabilities differ from maintained SoC facts")
    irq = obj(soc["irq"], {"priority_bits", "external_count"}, (), "IRQ facts")
    bits = integer(irq["priority_bits"], 1, 8, "IRQ priority bits")
    if bits != provider.IRQ_PRIORITY_BITS:
        fail("IRQ priority bits differ from maintained SoC facts")
    count = integer(irq["external_count"], 0, 496, "External IRQ count")
    if count != provider.EXTERNAL_IRQ_COUNT:
        fail("External IRQ count differs from maintained SoC facts")
    cpu_profiles.resolve(cpu, irq, "native" if provider.MODEL else "baremetal",
                         enum_abi=provider.ENUM_ABI)
    if not isinstance(soc["controllers"], dict):
        fail("SoC controllers must be an object")
    for name, controller in soc["controllers"].items():
        identity(name, "controller")
        obj(controller, {"kind", "modes", "irq", "pins"},
            {"resources", "max_hz", "irq_numbers", "channel_count", "clock_hz",
             "fifo_words", "endpoint_numbers", "interfaces", "message_ram_words"}, name)
        if controller["kind"] not in KINDS:
            fail(f"Unmaintained controller kind: {controller['kind']}")
        for field in ("modes", "irq", "pins"):
            values = sequence(controller[field], f"{name}.{field}")
            unique(values, f"{name}.{field}")
            for value in values:
                text(value, f"{name}.{field}")
        if not controller["modes"]:
            fail(f"Controller {name} has no maintained mode")
        for mode in controller["modes"]:
            provider.mode_implementation(controller["kind"], mode)
        # Primary kind names the existing default public interface. Reviewed
        # personalities share this physical controller and its resource claim.
        interfaces = obj(controller.get("interfaces", {}), (), KINDS,
                         name + ".interfaces")
        if controller["kind"] in interfaces:
            fail("Controller interfaces cannot repeat its primary kind")
        reviewed = getattr(provider, "CONTROLLER_INTERFACES", {}).get(name, {})
        if set(interfaces) != set(reviewed):
            fail("Controller interfaces differ from reviewed provider facts")
        for kind, modes in interfaces.items():
            modes = sequence(modes, name + ".interfaces." + kind)
            unique(modes, "controller interface modes")
            if not modes or tuple(modes) != reviewed[kind]:
                fail("Controller interface modes differ from reviewed facts")
            for mode in modes:
                provider.mode_implementation(kind, text(mode, "interface mode"))
        for resource in sequence(controller.get("resources", []),
                                 f"{name}.resources"):
            validate_resource_fact(resource, provider, "SoC resource")
    can.validate_soc(soc, provider)
    usb_device.validate_soc(soc, provider)
    ethernet.validate_soc(soc, provider)
    timer_advanced.validate_soc(soc, provider)
    secondary_controllers.validate_soc(soc, provider)
    try:
        from . import coupled_io
    except ImportError:
        import coupled_io
    coupled_io.validate_soc(soc, provider)
    if not isinstance(soc["clock_profiles"], dict) or not soc["clock_profiles"]:
        fail("Clock profiles are required")
    for name, profile in soc["clock_profiles"].items():
        identity(name, "clock profile")
        clocks.normalize(profile)
    validate_clock_profiles(soc, provider)
    return variant


def load_assembly(path, root=ROOT, snapshots=None):
    """Normalize one explicit authored input; no validation or format fallback."""
    path = path.resolve()
    snapshots = {} if snapshots is None else snapshots
    if path.suffix == ".toml":
        try:
            from . import authored
        except ImportError:
            import authored
        return authored.read(path, snapshots, root.resolve(), fail, obj, integer, text)
    if path.suffix == ".json":
        return load(path, snapshots)
    fail("Assembly must use authored TOML (.toml)")


def resolve_routes(routes_doc, soc, part, provider):
    """Validate reviewed route facts before selecting instances."""
    routes = {}
    for route in sequence(routes_doc["routes"], "routes"):
        obj(route, {"id", "variants", "controller", "kind", "pins", "modes",
                    "source"}, {"resources", "dma", "mode_dma", "mode_resources"}, "route")
        identity(route["id"], "route id")
        if route["id"] in routes:
            fail(f"Duplicate route id: {route['id']}")
        for field in ("variants", "modes"):
            values = sequence(route[field], f"route {field}")
            unique(values, f"route {field}")
            for value in values:
                text(value, f"route {field}")
        if not route["variants"] or not route["modes"]:
            fail("Route must have exact variants and maintained modes")
        if any(value not in soc["variants"] for value in route["variants"]):
            fail(f"Route {route['id']} has unknown exact variant")
        controller = soc["controllers"].get(route["controller"])
        modes = (None if controller is None else
                 controller["modes"] if controller["kind"] == route["kind"] else
                 controller.get("interfaces", {}).get(route["kind"]))
        if modes is None:
            fail(f"Route {route['id']} does not match a maintained controller")
        if not set(route["modes"]).issubset(modes):
            fail(f"Route {route['id']} advertises unsupported modes")
        for dma in sequence(route.get("dma", []), "DMA route"):
            provider.dma_irq(text(dma, "DMA route resource"))
        mode_dma = obj(route.get("mode_dma", {}), set(), set(route["modes"]),
                       "mode-specific DMA resources")
        for mode, resources in mode_dma.items():
            if not mode_traits(route["kind"], mode).dma:
                fail("Mode-specific DMA resources require a DMA execution mode")
            resources = sequence(resources, "mode-specific DMA resources")
            if not resources:
                fail("Mode-specific DMA resources must not be empty")
            unique(resources, "mode-specific DMA resources")
            for dma in resources:
                provider.dma_irq(text(dma, "mode-specific DMA resource"))
        for resource in sequence(route.get("resources", []), "route resources"):
            validate_resource_fact(resource, provider, "route resource")
        mode_resources = obj(route.get("mode_resources", {}), set(), set(route["modes"]),
                             "mode-specific resources")
        for mode, resources in mode_resources.items():
            if mode not in route["modes"]:
                fail("Mode-specific resource has no maintained route mode")
            for resource in sequence(resources, "mode-specific resources"):
                validate_resource_fact(resource, provider,
                                       "mode-specific resource")
        pins = sequence(route["pins"], "route pins")
        unique([pin.get("pin") for pin in pins], "route pins")
        for pin in pins:
            obj(pin, {"pin", "function", "af"}, (), "route pin")
            if not isinstance(pin["pin"], str) or not PIN.fullmatch(pin["pin"]):
                fail(f"Invalid pin: {pin['pin']}")
            if pin["pin"] not in controller["pins"]:
                fail(f"Route pin not permitted by controller: {pin['pin']}")
            for exact in route["variants"]:
                available = soc["variants"][exact].get("pins")
                if available is not None and pin["pin"] not in available:
                    fail(f"Pin {pin['pin']} does not exist on {exact}")
            integer(pin["af"], 0, 15, "pin AF")
            text(pin["function"], "pin function")
        text(route["source"], "route source")
        routes[route["id"]] = route
    return routes


def resolve_bindings(board, routes, part):
    """Validate Board electrical bindings and materialize safe masks."""
    bindings = {}
    for binding in sequence(board["bindings"], "Board bindings"):
        obj(binding, {"id", "controller", "route"},
            {"initial", "mask", "supply_mv"}, "binding")
        identity(binding["id"], "binding id")
        if binding["id"] in bindings:
            fail(f"Duplicate Board binding: {binding['id']}")
        route = routes.get(binding["route"])
        if not route or part not in route["variants"] or route["controller"] != binding["controller"]:
            fail(f"Board binding {binding['id']} has no reviewed exact-part route")
        if "supply_mv" in binding:
            if route["kind"] not in {"flash", "adc", "pwm_adc", "adc_group"}:
                fail("Supply voltage belongs to a Flash or ADC binding")
            integer(binding["supply_mv"], 1, 5000, "Supply millivolts")
        if "initial" in binding:
            integer(binding["initial"], 0, UINT32_MAX, "GPIO initial")
        actual_mask = sum(1 << int(PIN.fullmatch(pin["pin"])[2])
                          for pin in route["pins"])
        if "mask" in binding:
            integer(binding["mask"], 1, 65535, "GPIO mask")
            if binding["mask"] != actual_mask:
                fail(f"GPIO binding {binding['id']} mask differs from its reviewed pins")
        if (route["kind"] == "gpio" or "mask" in binding) and (
                binding.get("initial", 0) & ~actual_mask):
            fail(f"GPIO initial sets unauthorized bits: {binding['id']}")
        if route["kind"] == "gpio":
            binding = {**binding, "mask": actual_mask}
        bindings[binding["id"]] = binding
    return bindings


def bind_input_identities(inputs, root, snapshots, input_paths):
    """Seal authored/fact input bytes and their navigation atomically."""
    records = []
    for path in sorted(set(inputs), key=lambda value: value.as_posix()):
        label = (path.relative_to(root).as_posix() if path.is_relative_to(root)
                 else "external/" + path.name)
        contents = snapshots.get(path, path.read_bytes())
        expected = hashlib.sha256(contents).hexdigest()
        if file_digest(path) != expected:
            fail(f"Input changed during resolution: {label}")
        records.append({"path": label, "sha256": expected})
        if input_paths is not None:
            input_paths[label] = str(path)
    if len({record["path"] for record in records}) != len(records):
        fail("Declared input identity labels collide")
    return records


def resolve_components(assembly):
    """Validate component intent; CMake chooses its source targets."""
    components = sequence(assembly.get("components", []), "components")
    for component in components:
        text(component, "component")
    unique(components, "components")
    for component in components:
        if component not in COMPONENTS:
            fail(f"Unknown maintained component: {component}")
    if any(device["driver"] == "bmp280" for device in assembly["devices"]):
        if "bmp280-spi" not in components:
            fail("BMP280 SPI device requires the maintained bmp280-spi component")
    return components


def resolve(assembly_path, root=ROOT, *, input_paths=None):
    """Validate all input facts before producing the sole resolved decision."""
    root = root.resolve()
    verified_sdk = sdk.verify(root)

    def source_dependency(relative, expected=None):
        return (sdk_dependency_identity(verified_sdk, relative, expected)
                if verified_sdk is not None else
                dependency_identity(root, relative, expected))
    if assembly_path.is_symlink():
        fail("Assembly must not be a symlink")
    snapshots = {}
    read = lambda path: load(path, snapshots)
    assembly_path = assembly_path.resolve()
    policy_path = contained_file(root, metadata.POLICY)
    platform_metadata = metadata.policy_contract(
        read(policy_path), expected_factory_epoch=factory.FACTORY_ABI_EPOCH)
    assembly_input = load_assembly(assembly_path, root, snapshots)
    assembly = obj(assembly_input,
                   {"schema_version", "board_package", "backend", "clock_profile",
                    "controllers", "devices", "memory_budgets", "layout"},
                   {"abi", "optimization", "components", "os", "factory", "can_domain"}, "assembly")
    integer(assembly["schema_version"], 1, 1, "assembly schema")
    factory_options = obj(assembly.get("factory", {}), (),
                          {"capabilities"}, "factory policy")
    if ("capabilities" in factory_options and
            type(factory_options["capabilities"]) is not bool):
        fail("Factory capabilities must be boolean")
    board_name = text(assembly["board_package"], "Board package")
    board_dir = (assembly_path.parent / board_name).resolve()
    if Path(assembly_path.parent / board_name).is_symlink():
        fail("Board package must not be a symlink")
    board_path = board_dir / "board.json"
    board = obj(read(board_path),
                {"schema_version", "id", "soc", "soc_family", "source_revision",
                 "physical_pcb_revision", "clocks", "bindings",
                 "reserved_resources", "unknowns", "provenance"},
                {"inputs"}, "Board")
    integer(board["schema_version"], 1, 1, "Board schema")
    identity(board["id"], "Board id")
    family = identity(board["soc_family"], "SoC family")
    provider = maintained(family)
    part = identity(board["soc"], "exact part")
    soc_dir = root / "soc" / family
    if soc_dir.is_symlink() or not soc_dir.resolve().is_relative_to(root / "soc"):
        fail("SoC package escapes owned source")
    soc_path, routes_path = soc_dir / "soc.json", soc_dir / "routes.json"
    soc, routes_doc = read(soc_path), read(routes_path)
    variant = validate_soc(soc, part)
    reference_path = None
    references = None
    if "references" in soc:
        reference_path = contained_file(soc_dir, soc["references"])
        references = fact_references.validate(read(reference_path),
                                             soc["variants"])
    if soc["id"] != family:
        fail("Board SoC family differs from the package identity")
    obj(routes_doc, {"schema_version", "routes"}, (), "routes")
    integer(routes_doc["schema_version"], 1, 1, "routes schema")
    routes = resolve_routes(routes_doc, soc, part, provider)
    text(board["source_revision"], "Board source revision")
    if board["physical_pcb_revision"] is not None:
        text(board["physical_pcb_revision"], "physical PCB revision")
    clocks.validate_board(board["clocks"])
    for value in sequence(board["unknowns"], "unknowns"):
        text(value, "unknown condition")
    for value in sequence(board["provenance"], "provenance"):
        if isinstance(value, str):
            text(value, "provenance")
        else:
            obj(value, {"scope"}, {"url", "repository", "tree", "led_header_blob",
                "led_source_blob", "clock_source_blob", "uart_source_blob"}, "provenance")
            if not ("url" in value or "repository" in value):
                fail("Provenance requires URL or repository")
            for key, field in value.items():
                text(field, "provenance " + key)
                if key == "tree" or key.endswith("_blob"):
                    if not re.fullmatch(r"[0-9a-f]{40}", field):
                        fail("Provenance Git identity must be exact SHA-1")
    backend = assembly["backend"]
    if backend not in {"baremetal", "freertos", "native", "threadx"}:
        fail(f"Unsupported OS backend: {backend}")
    host = soc["cpu"]["arch"] == "native"
    if (host and backend not in {"native", "threadx"}) or (
            not host and backend == "native"):
        fail("Native is a host model; its backend cannot qualify MCU execution")
    if backend == "threadx":
        threadx.validate_platform(board["id"], family)
    resolve_interrupts(soc["cpu"], soc["irq"], backend, provider)
    cpu_profile = cpu_profiles.resolve(soc["cpu"], soc["irq"], backend,
                                       enum_abi=provider.ENUM_ABI)
    clock = soc["clock_profiles"].get(assembly["clock_profile"])
    if clock is None:
        fail(f"Unknown clock profile: {assembly['clock_profile']}")
    clock_plan = clocks.plan(assembly["clock_profile"], clock)
    clock = clocks.normalize(clock)
    clocks.match_board(clock, board["clocks"])
    if "os" in assembly and not isinstance(assembly["os"], dict):
        fail("OS policy: expected object")
    kernel_profile = kernel.resolve(assembly.get("os"), cpu_profile,
                                    clock["core_hz"])
    if (backend == "freertos" and kernel_profile.lowpower_port == "timebase" and
            family not in {"stm32f407", "gd32f470"}):
        fail("Timebase lowpower requires the maintained STM32F407/GD32F470 SoC alarm")
    cpu_profile = kernel.bind(cpu_profile, kernel_profile)
    irq_profile = cpu_profile.to_dict()["irq"]
    if "abi" in assembly and assembly["abi"] != cpu_abi(soc["cpu"]):
        fail("Assembly ABI contradicts the selected SoC")
    optimization = assembly.get("optimization", "Os")
    if optimization not in {"O0", "Og", "O2", "Os", "O3", "O2-lto", "Os-lto", "O3-lto"}:
        fail(f"Unsupported optimization profile: {optimization}")
    budgets = obj(assembly["memory_budgets"], {"main_stack_bytes"},
                  {"flash_load_bytes", "static_ram_bytes", "libc_heap_bytes", "regions"},
                  "memory budgets")
    for name, value in budgets.items():
        if name == "regions":
            continue
        integer(value, 0 if name == "libc_heap_bytes" else 1, context=name)
    if budgets["main_stack_bytes"] % 8:
        fail("Main stack must have 8-byte alignment")
    ram = next(region for region in variant["memory"] if region["linker"])
    if budgets["main_stack_bytes"] + budgets.get("libc_heap_bytes", 0) >= ram["size"]:
        fail("Reserved stack/heap exhaust default RAM")
    if budgets.get("static_ram_bytes", ram["size"]) > ram["size"]:
        fail("Static RAM budget exceeds physical linker domain")
    if budgets.get("flash_load_bytes", variant["flash"]["size"]) > variant["flash"]["size"]:
        fail("Flash budget exceeds exact physical density")
    selected_memory = memory_regions.select(
        variant["memory"], budgets, family, part, obj=obj, integer=integer, fail=fail)
    bindings = resolve_bindings(board, routes, part)
    claims = {}
    claim_list = []

    def claim(key, owner, mode, source):
        text(key, "resource key")
        if key.startswith("dma:"):
            provider.validate_dma_resource(key)
        key = normalize_resource(key)
        previous = claims.get(key)
        record = {"key": key, "owner": owner, "mode": mode, "source": source}
        if previous is not None:
            if previous == record:
                return
            fail(f"Resource conflict {key}: {previous['owner']} versus {owner}")
        claims[key] = record
        claim_list.append(record)

    for resource in soc.get("reserved_resources", []):
        claim(resource, "soc-timebase", "reserved", family)
    for reserved in sequence(board["reserved_resources"], "reserved resources"):
        if isinstance(reserved, str):
            key = "pin:" + reserved if PIN.fullmatch(reserved) else reserved
            owner = "board-reserved"
        else:
            obj(reserved, {"key", "owner"}, (), "reserved resource")
            key, owner = reserved["key"], reserved["owner"]
        # A Board may restate a silicon reservation; it cannot release it.
        if key.startswith("dma:"):
            provider.validate_dma_resource(key)
        previous = claims.get(normalize_resource(text(key, "resource key")))
        if previous is not None and previous["owner"] == "soc-timebase":
            continue
        claim(key, owner, "reserved", board["id"])
    if backend == "freertos":
        for irq in ("SysTick", "PendSV", "SVC"):
            claim("irq:" + irq, "freertos", "kernel",
                  "FreeRTOS " + irq_profile["kernel_port"])
    elif backend == "threadx" and not host:
        for irq in ("PendSV", *(["SysTick"] if
                                kernel_profile.tick_source == "systick" else [])):
            claim("irq:" + irq, "threadx", "kernel",
                  "ThreadX " + irq_profile["kernel_port"])
    selected = []
    adc_group_review = None
    if any(item.get("authored_kind") == "adc_group"
           for item in assembly["controllers"]):
        if family != "stm32g474":
            fail("ADC group has no maintained provider")
        adc_group_path = contained_file(root, soc.get("adc_group_review", ""))
        adc_group_review = read(adc_group_path)
        adc_group_digest = hashlib.sha256(snapshots[adc_group_path]).hexdigest()
    instance_ids = []
    irq_owners = {}
    for selection in sequence(assembly["controllers"], "selected controllers"):
        location = (selection.get("id", "<unknown>") if isinstance(selection, dict)
                    else "<invalid>")
        binding = selection.get("binding") if isinstance(selection, dict) else None
        mode = selection.get("mode") if isinstance(selection, dict) else None
        context = f"controller {location} binding {binding} mode {mode}"
        with error_context(context):
            obj(selection, {"id", "binding", "mode"},
                {"irq_priority", "rx_capacity", "baud", "max_hz", "event_capacity",
                 "period_ticks", "duty_ticks", "channels", "sample_cycles",
                 "timeout_ms", "calls_os", "rx_profile", "sample_times",
                 "reference_mv", "tick_hz", "edge", "authored_kind", "operation",
                 "bitrate", "timing", "drain_timeout_us", "poll_limit",
                 "fifo_order", "rx_fifo_words", "vbus_sensing", "clock_timeout_us",
                 "tx_count", "rx_count", "rx_storage_bytes", "address", "init_timeout_us",
                 "filter", "exclusive", "deadtime_ticks", "break_active_high", "idle_high",
                 "idle_complement_high", "frame", "stream", "irq_probe",
                 "counting", "filter_a", "filter_b", "invert_a", "invert_b",
                 "trigger_ticks", "data_bitrate", "filters", "silicon_profile",
                 "trigger", "period_us", "watchdog", "fifo"}, "controller selection")
            name = identity(selection["id"], "instance id")
            c_identity(name)
            instance_ids.append(name)
            binding = bindings.get(selection["binding"])
            if binding is None:
                fail(f"Unknown Board binding: {selection['binding']}")
            route = routes[binding["route"]]
            hardware = soc["controllers"][route["controller"]]
            authored_kind = selection.pop("authored_kind", route["kind"])
            if authored_kind != route["kind"]:
                fail("Authored kind differs from Board binding kind")
            mode = text(selection["mode"], "controller mode")
            if mode not in route["modes"]:
                fail(f"Unsupported mode {mode} for {selection['binding']}; "
                     f"allowed: {', '.join(route['modes'])}")
            validate_selection(selection, route, provider, ram["size"])
            if mode in uart_formats.FIFO_MODES and assembly["clock_profile"] != (
                    "hsi64-fixed-cache-off"
                    if family == "stm32h743" else "msis4"):
                fail("FIFO UART requires its reviewed fixed clock profile")
            if mode == uart_formats.RTSCTS_MODE and assembly["backend"] != "baremetal":
                fail("RTSCTS FIFO UART requires its reviewed baremetal owner")
            for numeric in {"rx_capacity", "baud", "max_hz", "event_capacity",
                            "period_ticks", "sample_cycles", "timeout_ms"} & selection.keys():
                integer(selection[numeric], 1, context=numeric)
            if "duty_ticks" in selection:
                integer(selection["duty_ticks"], 0, context="duty ticks")
                if selection["duty_ticks"] > selection.get("period_ticks", 0):
                    fail("PWM duty exceeds period")
            if "irq_priority" in selection:
                integer(selection["irq_priority"], 0,
                        irq_profile["maximum_priority"], "IRQ priority")
            if route["kind"] == "spi" and mode == "short-poll" and ({"irq_priority", "calls_os"} & selection.keys()):
                fail("Polling SPI has no IRQ options")
            if route["kind"] == "spi" and mode_traits("spi", mode).dma and "irq_priority" not in selection:
                fail("DMA SPI requires an explicit IRQ priority")
            if "calls_os" in selection and type(selection["calls_os"]) is not bool:
                fail("calls_os must be boolean")
            if selection.get("calls_os", False) and (
                    kernel_profile is None or host or
                    kernel_profile.kernel_policy == "none" or
                    selection.get("irq_priority", 0) < irq_profile["syscall_priority"]):
                fail("OS-calling ISR violates the selected kernel syscall ceiling")
            if selection.get("max_hz", 0) > hardware.get("max_hz", UINT32_MAX):
                fail("Controller speed exceeds maintained limit")
            traits = mode_traits(route["kind"], mode)
            if traits.dma and not route_dma(route, mode):
                fail("DMA mode has no reviewed DMA route")
            if traits.dma:
                provider.validate_dma(route, mode, normalize_resource)
            if route["kind"] != "gpio":
                claim("controller:" + route["controller"], name, mode, route["id"])
            for pin in route["pins"]:
                claim("pin:" + pin["pin"], name, mode, route["id"])
            if traits.irq:
                for irq in hardware["irq"]:
                    key = normalize_resource("irq:" + irq)
                    priority = selection.get("irq_priority")
                    previous = irq_owners.get(key)
                    shared = provider.shares_irq(route["kind"], key)
                    if previous and shared and previous[0] == "exti":
                        if previous[1] != priority:
                            fail(f"Shared EXTI vector {key} requires identical priorities")
                        record = next(record for record in claim_list if record["key"] == key)
                        record["owner"] += ";" + name
                        record["mode"] = "static-exti-dispatch"
                    else:
                        claim(key, name, mode, route["id"])
                        irq_owners[key] = (route["kind"], priority)

            for resource in hardware.get("resources", []) + route.get("resources", []) + route.get("mode_resources", {}).get(mode, []):
                claim(resource, name, mode, route["id"])
            if route["kind"] == "adc_group" and selection["trigger"] == "tim6-trgo":
                claim("controller:TIM6", name, mode, route["id"])
            if traits.dma:
                for dma in route_dma(route, mode):
                    claim("dma:" + dma, name, mode, route["id"])
                    claim(provider.dma_irq(dma), name, mode, route["id"])
            resolved_timing = None
            if route["kind"] == "i2c":
                try:
                    from . import i2c_timing_selection
                except ImportError:
                    import i2c_timing_selection
                resolved_timing = i2c_timing_selection.resolve(
                    selection, clock, provider, clock_plan=clock_plan)
            try:
                from . import stream_profiles
            except ImportError:
                import stream_profiles
            resolved_stream = stream_profiles.resolve(
                selection, route["kind"], provider)
            selected.append({**selection, "controller": route["controller"],
                             "kind": route["kind"], "route": route["id"],
                             "pins": route["pins"], "initial": binding.get("initial", 0),
                             "mask": binding.get("mask", 0),
                             **({"dma": route_dma(route, mode)} if traits.dma else {}),
                             **({"timing_resolution": resolved_timing}
                                if resolved_timing is not None else {}),
                             **({"stream_resolution": resolved_stream}
                                if resolved_stream is not None else {}),
                             **({"can_irqs": hardware["irq"]} if route["kind"] == "can" else {})})
            if route["kind"] == "can_fd":
                selected[-1]["message_ram_words"] = hardware["message_ram_words"]
            if route["kind"] == "adc_group":
                try:
                    from .providers.stm32g474 import adc_group
                except ImportError:
                    from providers.stm32g474 import adc_group
                selected[-1]["adc_group_resolution"] = adc_group.resolve(
                    selection, adc_group_review, adc_group_digest,
                    part=part, clock=clock)
    if (family == "stm32h743" and provider.cache_mask(assembly["clock_profile"]) != 0
            and any(item["kind"] == "flash" for item in selected)):
        fail("H743 physical Flash requires the reviewed cache-off timing owner")
    ethernet.resolve_counter_domains(selected)
    unique(instance_ids, "controller instance IDs")
    unique([c_id(name).upper() for name in instance_ids], "generated C identifiers")
    can_domain = can.resolve_domain(assembly.get("can_domain"), selected,
                                    soc, provider, claim, claims)
    device_ids = []
    addresses = set()
    chip_selects = set()
    for device in sequence(assembly["devices"], "devices"):
        location = (device.get("id", "<unknown>") if isinstance(device, dict)
                    else "<invalid>")
        context = f"endpoint {location}"
        with error_context(context):
            obj(device, {"id", "controller", "driver"}, {"cs_binding", "address",
                "mode", "max_hz", "authored_kind", "model_bytes", "number",
                "direction", "transfer", "max_packet", "fifo_words"}, "device")
            device_ids.append(identity(device["id"], "device ID"))
            c_identity(device["id"])
            controller = next((item for item in selected if item["id"] == device["controller"]), None)
            if controller is None or controller["kind"] not in {"spi", "i2c", "usb_device"}:
                fail("Device references no selected SPI/I2C/USB controller")
            authored_kind = device.pop("authored_kind", controller["kind"])
            if authored_kind != controller["kind"]:
                fail("Authored device kind differs from controller binding kind")
            if controller["kind"] == "usb_device":
                validated = usb_device.endpoint(device)
                device.clear()
                device.update(validated)
                continue
            model_fields = {"model_bytes"} if provider.MODEL else set()
            if "model_bytes" in device:
                if not provider.MODEL:
                    fail("model_bytes is only valid for an explicit Native device model")
                integer(device["model_bytes"], 1, 256, "Native device model_bytes")
            if controller["kind"] == "spi":
                obj(device, {"id", "controller", "driver"},
                    {"cs_binding", "mode", "max_hz"} | model_fields, "SPI device")
            else:
                obj(device, {"id", "controller", "driver", "address"}, model_fields,
                    "I2C device")
                if provider.MODEL and "model_bytes" not in device:
                    fail("Native I2C device requires explicit model_bytes")
            if device["driver"] not in {"spi-endpoint", "i2c-endpoint", "bmp280",
                                        "spi-transaction", "i2c-long-endpoint",
                                        "spi-dma-transaction"}:
                fail(f"Unknown maintained device driver: {device['driver']}")
            if device["driver"] == "spi-transaction":
                if controller["kind"] != "spi":
                    fail("SPI transaction requires its SPI controller")
                if family not in {"native", "stm32f407", "gd32f470"}:
                    fail("SPI transaction provider is not maintained")
                if controller["mode"] != "short-poll":
                    fail("SPI transaction requires the polling provider mode")
            if device["driver"] == "i2c-long-endpoint":
                if (controller["kind"] != "i2c" or family not in
                        {"stm32g474", "stm32h743", "stm32u575"} or
                        controller["mode"] != "poll-messages"):
                    fail("Long I2C requires a maintained polling V2 provider")
            if device["driver"] == "spi-dma-transaction":
                allowed = {"stm32f407": ("SPI1", "dma"),
                           "gd32f470": ("SPI4", "dma-full-duplex"),
                           "native": (controller["controller"], "async-model")}
                if (controller["kind"] != "spi" or family not in allowed or
                        (controller["controller"], controller["mode"]) !=
                        allowed[family]):
                    fail("SPI DMA transaction requires a reviewed DMA mode")
            if family == "native" and controller["mode"] == "async-model":
                if device["driver"] != "spi-dma-transaction" or "model_bytes" in device:
                    fail("Native async SPI requires enhanced echo transactions without model_bytes")
            if device["driver"] == "bmp280" and controller["kind"] != "spi":
                fail("BMP280 currently requires its maintained SPI transport")
            if device["driver"] == "spi-endpoint" and controller["kind"] != "spi":
                fail("SPI endpoint driver is not an I2C transport")
            if device["driver"] == "i2c-endpoint" and controller["kind"] != "i2c":
                fail("I2C endpoint driver is not a SPI transport")
            if "mode" in device:
                integer(device["mode"], 0, 3, "SPI endpoint mode")
            if "max_hz" in device:
                integer(device["max_hz"], 1, context="device speed")
                if device["max_hz"] > controller.get("max_hz", 1000000):
                    fail("Device speed exceeds its selected controller budget")
            if controller["kind"] == "spi":
                speed = device.get("max_hz", controller.get("max_hz", 1000000))
                limit = soc["controllers"][controller["controller"]].get("max_hz")
                if (family == "stm32h743" and clock_plan is not None and
                        speed * 256 < clock_plan["domains"]["spi4"]["maximum_hz"]):
                    fail("SPI speed cannot be realized by the reviewed upper clock bound and divider")
                if not provider.MODEL and (limit is None or speed < (limit + 127) // 128):
                    fail("SPI endpoint speed cannot be realized by the reviewed clock divider")
                builtin_cs = provider.builtin_cs(controller)
                if "cs_binding" not in device and not builtin_cs:
                    fail("SPI child requires a CS binding")
                if "cs_binding" in device:
                    binding = bindings.get(device["cs_binding"])
                    if not binding or routes[binding["route"]]["kind"] != "gpio":
                        fail("SPI child CS lacks reviewed GPIO wiring")
                    cs_pins = routes[binding["route"]]["pins"]
                    if len(cs_pins) != 1 or binding.get("initial") != 1 << int(cs_pins[0]["pin"][2:]):
                        fail("Active-low SPI CS must be one reviewed pin initially high")
                    for pin in cs_pins:
                        if pin["pin"] in chip_selects:
                            fail("Duplicate SPI chip select")
                        chip_selects.add(pin["pin"])
                        claim("pin:" + pin["pin"], device["id"], "cs", binding["route"])
                elif not provider.MODEL:
                    for pin in controller["pins"]:
                        if pin["function"] == "cs":
                            if pin["pin"] in chip_selects:
                                fail("Duplicate SPI chip select")
                            chip_selects.add(pin["pin"])
            else:
                integer(device["address"], 8, 119, "7-bit I2C address")
                key = (controller["id"], device["address"])
                if key in addresses:
                    fail("Duplicate I2C address on one bus")
                addresses.add(key)
    unique(device_ids + instance_ids, "assembly instance IDs")
    unique([c_id(name).upper() for name in device_ids + instance_ids], "generated C identifiers")
    usb_device.complete_plan(selected, assembly["devices"], claim)
    for controller in selected:
        children = [item for item in assembly["devices"] if item["controller"] == controller["id"]]
        if (family == "native" and controller["kind"] == "spi" and
                controller["mode"] == "async-model" and not children):
            fail("Native async SPI requires at least one explicit enhanced endpoint")
        if any(item["driver"] == "spi-dma-transaction" for item in children):
            if any(item["driver"] != "spi-dma-transaction" for item in children):
                fail("SPI DMA sequence requires all endpoints; mixed faces forbidden")
            controller["interface_kind"] = "spi_dma_transaction"
        if controller["kind"] == "i2c" and not children:
            fail("I2C requires at least one explicit address endpoint")
        if controller["kind"] == "spi" and provider.SPI_REQUIRES_ENDPOINT and not children:
            fail("SPI requires at least one reviewed CS endpoint")
    layout = variant["flash"].copy()
    inputs = [assembly_path, board_path, soc_path, routes_path, policy_path]
    if adc_group_review is not None:
        inputs.append(adc_group_path)
    if reference_path is not None:
        inputs.append(reference_path)
    for name in sequence(board.get("inputs", []), "declared Board inputs"):
        inputs.append(contained_file(board_dir, name))
    if assembly["layout"] is not None:
        layout_path = (assembly_path.parent / text(assembly["layout"], "layout path")).resolve()
        layout_doc = obj(read(layout_path), {"schema_version", "regions"}, (), "layout")
        integer(layout_doc["schema_version"], 1, 1, "layout schema")
        boundaries = {variant["flash"]["origin"]}
        cursor = variant["flash"]["origin"]
        for size in variant["flash"]["erase_blocks"]:
            cursor += size
            boundaries.add(cursor)
        spans = []
        region_ids = []
        images = []
        for region in sequence(layout_doc["regions"], "layout regions"):
            obj(region, {"id", "origin", "size", "kind"}, (), "layout region")
            identity(region["id"], "region ID")
            region_ids.append(region["id"])
            start, end = address_range(region["origin"], region["size"], "region")
            if start not in boundaries or end not in boundaries:
                fail("Flash region is not on exact erase boundaries")
            if any(start < b and a < end for a, b in spans):
                fail("Flash layout overlap")
            spans.append((start, end))
            if region["kind"] not in {"image", "data"}:
                fail("Unknown Flash region kind")
            if region["kind"] == "image":
                images.append(region)
        if len(images) != 1 or images[0]["origin"] != variant["flash"]["origin"]:
            fail("Only one reset-address image is maintained; offsets/bootloaders are unsupported")
        unique(region_ids, "Flash region IDs")
        layout = {**variant["flash"], **images[0]}
        inputs.append(layout_path)
    source_dependencies = []
    if "source_lock" in soc:
        lock = soc["source_lock"]
        if isinstance(lock, str):
            if lock != getattr(provider, "SOURCE_IMPORT_LOCK", None):
                fail("SoC source import lock differs from maintained provider")
            source_dependencies.append(
                imported_dependency_identity(root, lock, read))
            inputs.append(contained_file(root, lock))
            # A reviewed archive may consume separately pinned CMSIS inputs.
            # Its original archive identity cannot stand in for those bytes.
            for _, path in descriptor.maintained(family).git_dependencies:
                source_dependencies.append(source_dependency(path))
        else:
            dependencies = descriptor.maintained(family).git_dependencies
            if not dependencies:
                fail("SoC has no maintained Git source dependency contract")
            obj(lock, {key for key, _ in dependencies}, (), "SoC source lock")
            if any(not isinstance(value, str) or not re.fullmatch(r"[0-9a-f]{40}", value)
                   for value in lock.values()):
                fail("SDK Git dependencies require exact commit identities")
            for key, path in dependencies:
                source_dependencies.append(source_dependency(path, lock[key]))
    if backend == "freertos":
        source_dependencies.append(source_dependency("ext/freertos"))
    elif backend == "threadx":
        source_dependencies.append(source_dependency("ext/threadx"))
    manifest = root / ".nexus-source-sdk.json"
    if manifest.is_file():
        inputs.append(manifest)
    records = bind_input_identities(inputs, root, snapshots, input_paths)
    components = resolve_components(assembly)
    result = {"schema_version": 1, "platform_metadata": platform_metadata,
              "board": board["id"], "soc_family": family,
              "part": part, "package": variant["package"], "backend": backend,
              "cpu": soc["cpu"], "irq": irq_profile,
              "cpu_profile": cpu_profile.to_dict(),
              "kernel_profile": (kernel_profile.to_dict()
                                 if kernel_profile is not None else None),
              "clock_profile": assembly["clock_profile"],
              "clock": clock, "memory": selected_memory, "flash": variant["flash"],
              "layout": layout, "controllers": selected, "devices": assembly["devices"],
              "can_domain": can_domain,
              "can_controllers": list(provider.CAN_CONTROLLERS) if can_domain else [],
              "components": components, "claims": claim_list, "memory_budgets": budgets,
              "board_bindings": bindings, "routes": routes,
              "optimization": optimization, "inputs": records,
              "source_dependencies": source_dependencies,
              "physical_pcb_revision": board["physical_pcb_revision"],
              "limitations": board["unknowns"] + ["Physical qualification not executed"],
              "physical_qualified": False,
              "enum_abi": provider.ENUM_ABI}
    result["factory"] = {"capabilities": factory_options.get("capabilities", False)}
    if clock_plan is not None:
        result["clock_plan"] = clock_plan
    result["resource_plan"] = resource_graph.resolve(claim_list).to_dict()
    result["trigger_plan"] = trigger_graph.resolve_fixed(selected, routes).to_dict()
    for item in selected:
        if item["kind"] == "adc_group" and item["trigger"] == "tim6-trgo":
            item["adc_group_resolution"]["trigger_identity"] = next(
                index + 1 for index, edge in enumerate(
                    result["trigger_plan"]["edges"])
                if edge["target"] == item["id"] + ":adc")
    result["dma_topology"] = dma_topology.resolve_selected(selected, provider).to_dict()
    if references is not None:
        result["fact_references"] = references
    result["configuration_sha256"] = hashlib.sha256(
        json.dumps(result, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
    return result


def c_id(value):
    return value.replace("-", "_")


def normalize_resource(value):
    timer = re.fullmatch(r"(?:timer:|controller:)?(TIM[0-9]+|TIMER[0-9]+)", value)
    if timer is not None:
        return "controller:" + timer[1]
    if value.startswith("dma:"):
        channel = re.fullmatch(r"dma:(GPDMA1):channel(0|[1-9][0-9]*)"
                               r"(?::request(0|[1-9][0-9]*))?", value)
        if channel is not None:
            return f"dma:{channel[1]}:channel{channel[2]}"
        match = re.fullmatch(r"dma:([A-Z][A-Z0-9_]*):stream([0-9]+)"
                             r"(?::channel([0-9]+))?", value)
        if match is None:
            fail(f"Invalid physical DMA resource: {value}")
        return f"dma:{match[1]}:stream{int(match[2])}"
    if value.startswith("irq:"):
        irq = value[4:]
        for suffix in ("_IRQHandler", "_IRQn", "_Handler"):
            if irq.endswith(suffix):
                return "irq:" + irq[:-len(suffix)]
    return value


def resolve_ir(assembly_path, root=ROOT, *, input_paths=None):
    """Return the immutable typed resolution consumed by every emitter."""
    try:
        from .ir import ConfigurationIR
    except ImportError:
        from ir import ConfigurationIR
    try:
        return ConfigurationIR.from_validated(
            resolve(assembly_path, root, input_paths=input_paths))
    except ConfigurationError as error:
        raise ConfigurationError(f"{assembly_path}: {error}") from error


def generate(result, output):
    """Emit narrow compile/link inputs; this never emits a code dependency graph."""
    write = lambda name, data: (output / name).write_text(data, encoding="utf-8")
    write("resolved.json", json.dumps(result.to_dict() if hasattr(result, "to_dict")
                                      else result, indent=2, sort_keys=True) + "\n")
    # Compatibility fixtures enter through one adapter; normal generation uses
    # the already validated immutable CPU/kernel/instance decisions directly.
    try:
        from .ir import ConfigurationIR
    except ImportError:
        from ir import ConfigurationIR
    if not isinstance(result, ConfigurationIR):
        result = ConfigurationIR.from_validated(result)
    profile, policy = result.cpu_profile, result.kernel
    header = ["/* Generated from the sole resolved configuration. */",
              "#ifndef NEXUS_CONFIG_H", "#define NEXUS_CONFIG_H",
              f'#define NEXUS_CONFIG_SHA256 "{result["configuration_sha256"]}"',
              f'#define NEXUS_EXACT_PART "{result["part"]}"',
              f'#define NEXUS_CORE_HZ {result["clock"]["core_hz"]}u',
              f'#define NEXUS_CLOCK_INPUT_HZ {result["clock"]["input_hz"]}u',
              f'#define NEXUS_CLOCK_SOURCE_{result["clock"]["source"].upper()} 1',
              f'#define NEXUS_MAIN_STACK_BYTES {result.memory_budget.main_stack_bytes}u',
              f'#define NEXUS_BACKEND_{result["backend"].upper()} 1',
              *cpu_profiles.header_lines(profile), *kernel.header_lines(policy),
              *memory_regions.header_lines(result),
              *metadata.header_lines(thaw(result["platform_metadata"]))]
    probes = any(getattr(getattr(item.options, "irq", None), "probe", False)
                 for item in result.controllers)
    if probes:
        header += ["#if defined(NX_IO_CYCLE_PROBES) && NX_IO_CYCLE_PROBES != 1",
                   '#error "Probe choice must match the selected Platform usage"',
                   "#endif", "#ifndef NX_IO_CYCLE_PROBES",
                   "#define NX_IO_CYCLE_PROBES 1", "#endif"]
    if result["clock"]["source"] == "hse":
        header.insert(7, f'#define NEXUS_HSE_HZ {result["clock"]["input_hz"]}u')
    if result["soc_family"] == "stm32h743":
        cache_mask = maintained("stm32h743").cache_mask(result["clock_profile"])
        header.append(f'#define NEXUS_SOC_CACHE_MASK {cache_mask}u')
    cmake = [f'set(NEXUS_SOC_FAMILY "{result["soc_family"]}")',
             f'set(NEXUS_EXACT_PART "{result["part"]}")',
             'set(NEXUS_PROVIDER_CONFIG_MODULES "' + ';'.join(
                 descriptor.maintained(result["soc_family"]).configuration_modules)
             + '")',
             *cpu_profiles.selection_lines(profile),
             *kernel.selection_lines(policy),
             *memory_regions.selection_lines(result),
             f'set(NEXUS_CONFIG_SHA256 "{result["configuration_sha256"]}")',
             f'set(NEXUS_OPTIMIZATION "{result["optimization"]}")',
             'set(NEXUS_SELECTED_KINDS "' + ';'.join(sorted({item['kind'] for item in result['controllers']})) + '")',
             'set(NEXUS_SELECTED_COMPONENTS "' + ';'.join(result['components']) + '")']
    cmake.append("set(NEXUS_IO_CYCLE_PROBES " + ("ON" if probes else "OFF") + ")")
    for selection in result.controllers:
        prefix = "NEXUS_" + c_id(selection.id).upper()
        header.append(f"#define {prefix}_SELECTED 1")
        if selection.kind == "gpio":
            mask = selection.options.mask
            header.append(f"#define {prefix}_MASK {mask}u")
        irq = getattr(selection.options, "irq", None)
        for field in ("rx_capacity", "baud", "irq_priority", "event_capacity"):
            value = (irq.priority if irq is not None else None) if (
                field == "irq_priority") else getattr(selection.options, field, None)
            if value is not None:
                header.append(f"#define {prefix}_{field.upper()} {value}u")
    header.extend(["#endif", ""])
    write("nexus_config.h", "\n".join(header))
    write("selection.cmake", "\n".join(cmake) + "\n")
    try:
        from .bindings import emit
    except ImportError:
        from bindings import emit
    try:
        from .factory import emit as emit_factory, emit_strong
    except ImportError:
        from factory import emit as emit_factory, emit_strong
    factory_header, factory_source = emit_factory(result)
    write("nexus_factory.h", factory_header)
    write("nexus_factory_strong.h", emit_strong(result))
    bindings_header, bindings_source = emit(result)
    bindings_source += "\n" + factory_source
    write("nexus_bindings.h", bindings_header)
    write("bindings.c", bindings_source)
    write("memory.ld", memory_regions.linker(result))
    write("resource-budget.json", json.dumps(memory_regions.resource_budget(result),
                                            indent=2, sort_keys=True) + "\n")
    write(".nexus-config-bundle", result["configuration_sha256"] + "\n")


def protected_inputs(assembly, root):
    """Read only path selectors before withdrawing an owned public bundle.

    Malformed authored bytes still invalidate previous output. This preflight
    makes no selection decisions and never replaces normal strict resolution.
    """
    assembly = Path(assembly).absolute()
    paths = [assembly, root.resolve(), ROOT, root / metadata.POLICY]
    try:
        content = assembly.read_text(encoding="utf-8")
        raw = (tomllib.loads(content) if assembly.suffix == ".toml" else
               json.loads(content))
    except (OSError, ValueError):
        return paths
    if not isinstance(raw, dict):
        return paths
    package = raw.get("board_package")
    if isinstance(raw.get("board"), str):
        package = str(root / "boards" / raw["board"])
    if isinstance(package, str):
        directory = (assembly.parent / package).resolve()
        paths.append(directory)
        try:
            board = json.loads((directory / "board.json").read_text(encoding="utf-8"))
            family = board.get("soc_family")
            if isinstance(family, str):
                paths.append(root / "soc" / family)
        except (OSError, ValueError, AttributeError):
            pass
    if isinstance(raw.get("layout"), str):
        paths.append(assembly.parent / raw["layout"])
    return paths


def configure(assembly, output, root=ROOT):
    inputs = protected_inputs(assembly, root)
    packages = [root / name for name in ("soc", "boards", "dependencies", "vendors", "ext")]
    packages.extend(ROOT / name for name in (
        "tools/configure", "tools/maintenance", "scripts/ci"))
    packages.extend(path for path in inputs if Path(path).is_dir() and
                    Path(path).resolve() not in {root.resolve(), ROOT.resolve()})
    with bundle.transaction(output, ".nexus-config-bundle",
                            inputs=inputs, packages=packages, fail=fail) as temporary:
        initial_code_paths, initial_code_identity = dependencies.snapshot(
            mode="platform", sdk=sdk.installed(root))
        input_paths = {}
        result = resolve_ir(assembly, root, input_paths=input_paths)
        dependencies.verify(initial_code_paths, initial_code_identity)
        code_paths, code_identity = dependencies.selected(
            initial_code_paths, initial_code_identity, family=result.soc_family)
        generate(result, temporary)
        (temporary / "input_paths.json").write_text(
            json.dumps(input_paths, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        dependencies.emit(temporary, code_paths, code_identity)
        # Publish only if every snapshotted authored or package input still
        # matches the resolved identity, including compatibility metadata.
        for record in result["inputs"]:
            if file_digest(Path(input_paths[record["path"]])) != record["sha256"]:
                fail(f"Input changed during generation: {record['path']}")
        dependencies.verify(initial_code_paths, initial_code_identity)
    return result.to_dict()


def main(argv=None):
    arguments = list(sys.argv[1:] if argv is None else argv)
    commands = {"generate", "check", "explain", "list-bindings", "init"}
    command = arguments.pop(0) if arguments and arguments[0] in commands else "generate"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=ROOT)
    parser.add_argument("--assembly", required=command != "init", type=Path)
    parser.add_argument("--output", required=command in {"generate", "init"}, type=Path)
    if command == "init":
        parser.add_argument("--board", required=True)
        parser.add_argument("--backend", required=True,
                            choices=("native", "baremetal", "freertos", "threadx"))
        parser.add_argument("--clock", required=True)
    args = parser.parse_args(arguments)
    try:
        if command == "init":
            if args.output.exists() or args.output.is_symlink():
                fail("init refuses to overwrite an existing authored configuration")
            if args.output.suffix != ".toml":
                fail("init output must be authored TOML (.toml)")
            contents = ("schema = 2\nboard = " + json.dumps(args.board) +
                        "\nbackend = " + json.dumps(args.backend) +
                        "\nclock = " + json.dumps(args.clock) +
                        "\n\n[memory]\nmain_stack_bytes = 2048\nlibc_heap_bytes = 0\n")
            args.output.parent.mkdir(parents=True, exist_ok=True)
            try:
                with args.output.open("x", encoding="utf-8") as stream:
                    stream.write(contents)
                result = resolve_ir(args.output, args.source_root)
            except BaseException:
                args.output.unlink(missing_ok=True)
                raise
        elif command == "generate":
            result = configure(args.assembly, args.output, args.source_root)
        else:
            result = resolve_ir(args.assembly, args.source_root)
        if command == "explain":
            print(json.dumps(result.to_dict(), indent=2, sort_keys=True))
        elif command == "list-bindings":
            print(f"Board: {result['board']}")
            for name, binding in result["board_bindings"].items():
                route = result["routes"][binding["route"]]
                print(f"{name}: {route['kind']} {route['controller']} "
                      f"modes={','.join(route['modes'])}")
        else:
            print(f"Resolved {result['part']}/{result['board']}/{result['backend']}: "
                  f"{result['configuration_sha256']}")
        return 0
    except (ConfigurationError, OSError, ValueError, TypeError, KeyError) as error:
        print(f"Nexus configuration rejected: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
