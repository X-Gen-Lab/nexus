#!/usr/bin/env python3
"""Resolve reviewed hardware facts and emit one atomic configuration bundle.

CMake remains the authority for source targets. This tool never discovers source
code, creates workers, selects application policy, or invents a hardware route.
"""
from collections.abc import Mapping
from contextlib import contextmanager
from dataclasses import dataclass, replace
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

# Direct CLI execution binds only this checkout or relocated SDK.
if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.configure import cpu as cpu_profiles
from tools.configure import bundle, dependencies, sdk, i2s2
from tools.configure.calls import C_KEYWORDS as C11_KEYWORDS
from tools.configure.ir import (
    CanDomainIR, CanFilterIR, ClockPlanIR, ConfigurationIR, ControllerIR,
    EndpointIR, FactoryOptions, MemoryBudgetIR, PhysicalPartIR, PwmSharedOwnerIR,
    RuntimePowerIR, freeze, thaw)
from tools.configure import resource_graph, trigger_graph, dma_topology
from tools.configure.contracts import COMPONENTS, CONTROLLER_KINDS
from tools.configure import kernel, memory as memory_regions, metadata, factory, can, threadx, usb_device, ethernet, timer_advanced, secondary_controllers, fact_references, clocks, physical_facts
from tools.configure.providers import descriptor, maintained
from tools.configure.selection import (validate_clock_profiles, resolve_interrupts,
                        validate_interrupt_profile,
                        validate_selection)
from tools.configure.providers.common import (ConfigurationError, cpu_abi, fail, integer,
                               mode_traits, route_dma,
                               sequence)

ROOT = Path(__file__).resolve().parents[2]
UINT32_MAX = (1 << 32) - 1
IDENTIFIER = re.compile(r"[A-Za-z][A-Za-z0-9_-]{0,63}")
PIN = physical_facts.GPIO_PIN
KINDS = frozenset(CONTROLLER_KINDS)
C_KEYWORDS = C11_KEYWORDS | {"main"}


@contextmanager
def error_context(context):
    """Attach the authored instance while retaining the precise field failure."""
    try:
        yield
    except ConfigurationError as error:
        details = {**error.details,
                   "contexts": [*error.details.get("contexts", ()), context]}
        raise ConfigurationError(f"{context}: {error}", **details) from error


def c_identity(value):
    identifier = c_id(value)
    if identifier.upper() == "COUNT":
        fail(f"Reserved factory identifier: {value}")
    if identifier in C_KEYWORDS:
        fail(f"Reserved C identifier: {value}")
    return identifier


def dependency_identity(root, relative, expected=None):
    """Validate staged development pins, or verified installed SDK identities."""
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
            return command.stdout
        # One porcelain snapshot supplies HEAD and dirty state. Every call still
        # observes the checkout and staged Gitlink afresh; no result is cached.
        status = git(checkout, "status", "--porcelain=v2", "--branch",
                     "--no-ahead-behind", "--untracked-files=normal").splitlines()
        commits = [line.removeprefix("# branch.oid ") for line in status
                   if line.startswith("# branch.oid ")]
        if (len(commits) != 1 or re.fullmatch(
                r"(?:[0-9a-f]{40}|[0-9a-f]{64})", commits[0]) is None):
            fail(f"Cannot verify SDK dependency: {relative}")
        commit = commits[0]
        # A dependency upgrade must configure before its parent commit exists.
        # Require the real index's explicit stage-0 pin, never an unstaged
        # checkout or inherited hook index. SDK publication separately requires
        # clean committed source and does not use this development selection.
        entries = git(root, "--literal-pathspecs", "ls-files", "--stage", "-z",
                      "--", relative).split("\0")
        metadata, separator, path = entries[0].partition("\t")
        if (len(entries) != 2 or entries[-1] != "" or not separator or
                path != relative or metadata.split() != ["160000", commit, "0"]):
            fail(f"SDK dependency differs from the locked Gitlink: {relative}")
        if any(line and not line.startswith("# ") for line in status):
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
        details = (error.details if isinstance(error, ConfigurationError) else {})
        raise ConfigurationError(f"{path}: {error}",
                                 **{**details, "source": str(path)}) from error


def obj(value, required, optional=(), context="object"):
    if not isinstance(value, dict):
        fail(f"{context}: expected object")
    missing = set(required) - value.keys()
    unknown = value.keys() - set(required) - set(optional)
    if missing or unknown:
        fail(f"{context}: missing {sorted(missing)}, unknown {sorted(unknown)}",
             code="unknown_or_missing_key", context=context,
             missing=sorted(missing), unknown=sorted(unknown))
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


def _validate_physical_variant(value, provider, policy):
    """Validate each maintained part before any route can refer to its facts."""
    variant = obj(value, {"package", "flash", "memory"},
                  {"pins", "pad_conditions"}, "variant")
    if not provider.MODEL or "pins" in variant:
        physical_facts.package_pins(
            variant.get("pins"), variant.get("pad_conditions", {}),
            getattr(provider, "CONDITIONAL_PACKAGE_PADS", ()))
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
    for region in memory:
        obj(region, {"id", "origin", "size", "linker"},
            {"dma", "dma_engines"}, "memory")
    unique([region["id"] for region in memory], "memory IDs")
    ranges = [(flash["origin"], flash["origin"] + flash["size"], "Flash")]
    for region in memory:
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
    policy.validate(variant)
    return variant


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
    from tools.configure.providers.common import exact_part
    exact_part(provider.PARTS, part)
    reference = getattr(provider, "FACT_REFERENCES", None)
    if reference is not None and soc.get("references") != reference:
        fail("SoC references differ from the maintained source index")
    # Routes can reference unselected packages too. Every physical contract must
    # be valid independently of which part this assembly happens to select.
    for exact, package in soc["variants"].items():
        identity(exact, "variant")
        _validate_physical_variant(package, provider,
                                   exact_part(provider.PARTS, exact))
    variant = soc["variants"][part]
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
    from tools.configure import coupled_io
    coupled_io.validate_soc(soc, provider)
    if not isinstance(soc["clock_profiles"], dict) or not soc["clock_profiles"]:
        fail("Clock profiles are required")
    for name, profile in soc["clock_profiles"].items():
        identity(name, "clock profile")
        provider.normalize_clock(profile)
    validate_clock_profiles(soc, provider)
    return variant


def load_assembly(path, root=ROOT, snapshots=None):
    """Normalize one explicit authored input; no validation or format fallback."""
    path = path.resolve()
    snapshots = {} if snapshots is None else snapshots
    if path.suffix == ".toml":
        from tools.configure import authored
        return authored.read(path, snapshots, root.resolve(), fail, obj, integer, text)
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


def validate_board_context(board):
    """Validate shared provenance and clock facts without choosing a product."""
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


def resolve_bindings(board, routes, part):
    """Validate Board electrical bindings and materialize safe masks."""
    bindings = {}
    for binding in sequence(board["bindings"], "Board bindings"):
        obj(binding, {"id", "controller", "route"},
            {"initial", "mask", "supply_mv", "pin_states"}, "binding")
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
        from tools.configure import pin_states
        resolved_states = pin_states.validate_binding(binding, route["kind"], actual_mask)
        if resolved_states is not None:
            binding = {**binding, "pin_state_resolution": resolved_states}
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
    components = sequence(list(assembly.components) if
                          isinstance(assembly.components, tuple) else
                          assembly.components, "components")
    for component in components:
        text(component, "component")
    unique(components, "components")
    for component in components:
        if component not in COMPONENTS:
            fail(f"Unknown maintained component: {component}")
    if any(device["driver"] == "bmp280" for device in assembly.devices):
        if "bmp280-spi" not in components:
            fail("BMP280 SPI device requires the maintained bmp280-spi component")
    return components


def resource_claims(provider):
    """One claim ledger serves resolution and read-only reservation queries."""
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
            fail(f"Resource conflict {key}: {previous['owner']} "
                 f"(source {previous['source']}) versus {owner} "
                 f"(source {source})", code="resource_conflict",
                 resource=key, claims=[previous, record])
        claims[key] = record
        claim_list.append(record)

    return claims, claim_list, claim


def reserved_claims(soc, board, provider):
    """Project the same immutable SoC and Board reservations used by resolve."""
    claims, records, claim = resource_claims(provider)
    family = soc["id"]
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
    return records


def board_directory(path):
    """Accept explicit private paths, rejecting symlinks before normalization."""
    path = Path(path).absolute()
    if any(item.is_symlink() for item in (path, *path.parents)):
        fail("Board package must not follow a symlink")
    if not path.is_dir():
        fail(f"Board package must be a directory: {path}")
    return path.resolve()


@dataclass(frozen=True)
class _MemoryResolution:
    """Typed budgets and immutable physical domains from one memory decision."""
    budget: MemoryBudgetIR
    default_ram: Mapping
    regions: tuple[Mapping, ...]
    gpio_user: bool


@dataclass(frozen=True)
class _FlashLayoutResolution:
    """One selected image and its optional authored input identity."""
    layout: Mapping
    input_path: Path | None


@dataclass(frozen=True)
class _SourceResolution:
    """Locked sources and additional files consumed by dependency admission."""
    dependencies: tuple[Mapping, ...]
    inputs: tuple[Path, ...]


def _resolve_memory(assembly, variant, provider, part, kernel_profile):
    """Admit budgets and optional domains against exact physical capacity."""
    from tools.configure.ir import (MemoryRegionBudgetIR,
                                    MpuMemoryReservationsIR, freeze, thaw)
    budgets = obj(thaw(assembly.memory_budgets), {"main_stack_bytes"},
                  {"flash_load_bytes", "static_ram_bytes", "libc_heap_bytes", "regions", "mpu"},
                  "memory budgets")
    for name, value in budgets.items():
        if name in {"regions", "mpu"}:
            continue
        integer(value, 0 if name == "libc_heap_bytes" else 1, context=name)
    if budgets["main_stack_bytes"] % 8:
        fail("Main stack must have 8-byte alignment")
    ram = next(region for region in variant["memory"] if region["linker"])
    gpio_user = (assembly.backend == "freertos" and
                 kernel_profile.user_io == "gpio-write")
    if gpio_user:
        provider.validate_user_io_context(assembly, part)
    if ("mpu" in budgets) != gpio_user:
        fail("Explicit protected MPU reservations require GPIO user I/O")
    if gpio_user:
        fields = {"privileged_flash_bytes", "user_flash_bytes",
                  "syscall_flash_bytes", "privileged_ram_bytes",
                  "user_ram_bytes"}
        reservations = obj(budgets["mpu"], fields, (), "MPU reservations")
        for name, value in reservations.items():
            integer(value, 32, UINT32_MAX, "MPU reservation " + name)
            if value % 32:
                fail("MPU reservations require exact 32-byte granules")
        if (sum(reservations[name] for name in (
                "privileged_flash_bytes", "user_flash_bytes",
                "syscall_flash_bytes")) > variant["flash"]["size"] or
                reservations["privileged_ram_bytes"] +
                reservations["user_ram_bytes"] > ram["size"]):
            fail("MPU reservations exceed physical Flash/SRAM1")
        if (budgets.get("regions") or budgets["main_stack_bytes"] +
                budgets.get("libc_heap_bytes", 0) >=
                reservations["privileged_ram_bytes"] or
                budgets.get("static_ram_bytes", ram["size"]) >
                reservations["privileged_ram_bytes"]):
            fail("MPU privileged reservations cannot cover optional RAM or exhaust MSP")
    if budgets["main_stack_bytes"] + budgets.get("libc_heap_bytes", 0) >= ram["size"]:
        fail("Reserved stack/heap exhaust default RAM")
    if budgets.get("static_ram_bytes", ram["size"]) > ram["size"]:
        fail("Static RAM budget exceeds physical linker domain")
    if budgets.get("flash_load_bytes", variant["flash"]["size"]) > variant["flash"]["size"]:
        fail("Flash budget exceeds exact physical density")
    selected_memory = memory_regions.select(
        variant["memory"], budgets, provider.PARTS[part].optional_memory,
        obj=obj, integer=integer, fail=fail)
    memory_budget = MemoryBudgetIR(
        budgets["main_stack_bytes"], budgets.get("libc_heap_bytes", 0),
        budgets.get("static_ram_bytes"), budgets.get("flash_load_bytes"),
        tuple(MemoryRegionBudgetIR(name, item["limit_bytes"])
              for name, item in sorted(budgets.get("regions", {}).items())),
        MpuMemoryReservationsIR(**budgets["mpu"]) if "mpu" in budgets else None)
    return _MemoryResolution(memory_budget, freeze(ram),
                             freeze(selected_memory), gpio_user)


def _resolve_flash_layout(layout_name, assembly_directory, physical_flash, read):
    """Select one reset-address image on reviewed physical erase boundaries."""
    from tools.configure.ir import freeze
    layout = physical_flash.copy()
    layout_path = None
    if layout_name is not None:
        layout_path = (assembly_directory / text(layout_name, "layout path")).resolve()
        layout_doc = obj(read(layout_path), {"schema_version", "regions"}, (), "layout")
        integer(layout_doc["schema_version"], 1, 1, "layout schema")
        boundaries = {physical_flash["origin"]}
        cursor = physical_flash["origin"]
        for size in physical_flash["erase_blocks"]:
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
        if len(images) != 1 or images[0]["origin"] != physical_flash["origin"]:
            fail("Only one reset-address image is maintained; offsets/bootloaders are unsupported")
        unique(region_ids, "Flash region IDs")
        layout = {**physical_flash, **images[0]}
    return _FlashLayoutResolution(freeze(layout), layout_path)


def _resolve_source_dependencies(soc, provider, family, root, backend, read,
                                 source_dependency):
    """Resolve exact selected Git/archive pins without selecting source targets."""
    from tools.configure.ir import freeze
    inputs = []
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
    return _SourceResolution(freeze(source_dependencies), tuple(inputs))


@dataclass(frozen=True)
class _DeviceGraphResolution:
    """Complete controller/endpoint decisions; local builders never escape."""
    controllers: tuple[ControllerIR, ...]
    devices: tuple[EndpointIR, ...]
    resources: resource_graph.ResourcePlanIR
    triggers: trigger_graph.TriggerPlanIR
    dma: dma_topology.DmaTopologyIR
    can_domain: CanDomainIR | None
    pwm_shared_owners: tuple[PwmSharedOwnerIR, ...]
    inputs: tuple[Path, ...]


def _resolve_device_graph(assembly, *, root, soc, board, routes, bindings,
                          provider, clock, clock_plan, kernel_profile,
                          irq_profile, memory, i2s2_role, read,
                          snapshots) -> _DeviceGraphResolution:
    """Validate coupled device choices, then publish one immutable graph.

    USB FIFO placement, SPI sequence faces and ADC trigger identities depend on
    the complete selected set. Keep their mutable normalizers local until all
    cross-controller and endpoint checks have passed; expose only typed values.
    """
    family, part = board["soc_family"], board["soc"]
    backend = assembly.backend
    host = soc["cpu"]["arch"] == "native"
    ram, gpio_user = memory.default_ram, memory.gpio_user
    controller_selections = thaw(assembly.controllers)
    endpoint_selections = thaw(assembly.devices)
    claims, claim_list, claim = resource_claims(provider)
    for record in reserved_claims(soc, board, provider):
        claim(**record)
    if i2s2_role is not None:
        for resource in i2s2_role.claims:
            claim(resource, "i2s2", i2s2_role.profile, "RM0090/DS8626 chip reference")
    if backend == "freertos":
        for irq in ("SysTick", "PendSV", "SVC"):
            claim("irq:" + irq, "freertos", "kernel",
                  "FreeRTOS " + irq_profile.kernel_port)
    elif backend == "threadx" and not host:
        for irq in ("PendSV", *(["SysTick"] if
                                kernel_profile.tick_source == "systick" else [])):
            claim("irq:" + irq, "threadx", "kernel",
                  "ThreadX " + irq_profile.kernel_port)
    selected = []
    adc_group_contract = None
    instance_ids = []
    irq_owners = {}
    for selection in sequence(controller_selections, "selected controllers"):
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
            resolved_pin_states = None
            if mode == "named-states":
                from tools.configure import pin_states
                resolved_pin_states = pin_states.select(
                    binding, family=family, part=part,
                    controller=route["controller"], backend=backend,
                    clock=assembly.clock_profile)
            provider.validate_mode_context(
                selection, route, backend=backend,
                clock_profile=assembly.clock_profile)
            for numeric in {"rx_capacity", "baud", "max_hz", "event_capacity",
                            "period_ticks", "sample_cycles", "timeout_ms"} & selection.keys():
                integer(selection[numeric], 1, context=numeric)
            if "duty_ticks" in selection:
                integer(selection["duty_ticks"], 0, context="duty ticks")
                if selection["duty_ticks"] > selection.get("period_ticks", 0):
                    fail("PWM duty exceeds period")
            if "irq_priority" in selection:
                integer(selection["irq_priority"], 0,
                        irq_profile.maximum_priority, "IRQ priority")
            if route["kind"] == "spi" and mode == "short-poll" and ({"irq_priority", "calls_os"} & selection.keys()):
                fail("Polling SPI has no IRQ options")
            if route["kind"] == "spi" and mode_traits("spi", mode).dma and "irq_priority" not in selection:
                fail("DMA SPI requires an explicit IRQ priority")
            if "calls_os" in selection and type(selection["calls_os"]) is not bool:
                fail("calls_os must be boolean")
            if selection.get("calls_os", False) and (
                    kernel_profile is None or host or
                    kernel_profile.kernel_policy == "none" or
                    selection.get("irq_priority", 0) < irq_profile.syscall_priority):
                fail("OS-calling ISR violates the selected kernel syscall ceiling")
            if selection.get("max_hz", 0) > hardware.get("max_hz", UINT32_MAX):
                fail("Controller speed exceeds maintained limit")
            traits = mode_traits(route["kind"], mode)
            if traits.dma and not route_dma(route, mode):
                fail("DMA mode has no reviewed DMA route")
            if traits.dma:
                provider.validate_dma(route, mode, normalize_resource)
            shared_pwm = (provider.PWM_SHARED is not None and
                          provider.PWM_SHARED.selected(
                              {"kind": route["kind"], "mode": mode}))
            if route["kind"] != "gpio" and not shared_pwm:
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
            if route["kind"] == "adc_group":
                if provider.ADC_GROUP is None:
                    fail("ADC group has no maintained provider")
                for resource in provider.ADC_GROUP.trigger_resources(selection):
                    claim(resource, name, mode, route["id"])
            if traits.dma:
                for dma in route_dma(route, mode):
                    claim("dma:" + dma, name, mode, route["id"])
                    claim(provider.dma_irq(dma), name, mode, route["id"])
            resolved_timing = None
            if route["kind"] == "i2c":
                from tools.configure import i2c_timing_selection
                resolved_timing = i2c_timing_selection.resolve(
                    selection, clock, provider, clock_plan=clock_plan)
            from tools.configure import stream_profiles
            resolved_stream = stream_profiles.resolve(
                selection, route["kind"], provider)
            selected.append({**selection, "controller": route["controller"],
                             "kind": route["kind"], "route": route["id"],
                             "pins": route["pins"], "initial": binding.get("initial", 0),
                             "mask": binding.get("mask", 0),
                             **({"pin_state_resolution": resolved_pin_states}
                                if resolved_pin_states is not None else {}),
                             **({"dma": route_dma(route, mode)} if traits.dma else {}),
                             **({"timing_resolution": resolved_timing}
                                if resolved_timing is not None else {}),
                             **({"stream_resolution": resolved_stream}
                                if resolved_stream is not None else {}),
                             **({"can_irqs": hardware["irq"]} if route["kind"] == "can" else {})})
            selected[-1].update(provider.mode_fields(
                selection, route, clock_profile=assembly.clock_profile))
            if route["kind"] == "can_fd":
                selected[-1]["message_ram_words"] = hardware["message_ram_words"]
            if route["kind"] == "adc_group":
                if provider.ADC_GROUP is None:
                    fail("ADC group has no maintained provider")
                if adc_group_contract is None:
                    adc_group_path = contained_file(
                        root, soc.get("adc_group_review", ""))
                    review = read(adc_group_path)
                    adc_group_contract = (
                        review, hashlib.sha256(
                            snapshots[adc_group_path]).hexdigest(),
                        adc_group_path)
                adc_group_review, adc_group_digest, _ = adc_group_contract
                selected[-1]["adc_group_resolution"] = (
                    provider.ADC_GROUP.resolve(
                        selection, adc_group_review, adc_group_digest,
                        part=part, clock=clock))
    provider.validate_controller_set(
        selected, clock_profile=assembly.clock_profile)
    ethernet.resolve_counter_domains(selected)
    pwm_shared_owners = []
    pwm_sharing = {}
    if any(item["kind"] == "pwm" and item["mode"] == "shared-fixed-pwm"
           for item in selected):
        if provider.PWM_SHARED is None:
            fail("Shared PWM has no maintained physical owner")
        pwm_shared_owners = provider.PWM_SHARED.resolve(
            selected, bindings, soc, clock, assembly.clock_profile)
        physical_claims, pwm_sharing = provider.PWM_SHARED.resources(
            pwm_shared_owners)
        claim_list.extend(physical_claims)
    unique(instance_ids, "controller instance IDs")
    unique([c_id(name).upper() for name in instance_ids], "generated C identifiers")
    can_domain = can.resolve_domain(thaw(assembly.can_domain), selected,
                                    soc, provider, claim, claims)
    device_ids = []
    addresses = set()
    chip_selects = set()
    for device in sequence(endpoint_selections, "devices"):
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
            provider.ENDPOINT_POLICY.validate(device, controller)
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
                provider.validate_endpoint_clock(speed, clock_plan=clock_plan)
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
    usb_device.complete_plan(selected, endpoint_selections, claim)
    for controller in selected:
        children = [item for item in endpoint_selections if item["controller"] == controller["id"]]
        if (provider.ENDPOINT_POLICY.model_async_echo and
                controller["kind"] == "spi" and
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
    if gpio_user and (len(selected) != 1 or
            selected[0]["kind"] != "gpio" or
            selected[0]["controller"] != "GPIOC" or
            selected[0]["mode"] != "output" or
            selected[0]["mask"] != 0x80):
        fail("GPIO user I/O requires the single exact PC7 output binding")
    resources = resource_graph.resolve(claim_list, shared=pwm_sharing)
    triggers = trigger_graph.resolve_fixed(selected, routes)
    for item in selected:
        if item["kind"] == "adc_group" and item["trigger"] == "tim6-trgo":
            item["adc_group_resolution"]["trigger_identity"] = next(
                index + 1 for index, edge in enumerate(triggers.edges)
                if edge.target == item["id"] + ":adc")
    dma_plan = dma_topology.resolve_selected(selected, provider)
    controllers = tuple(ControllerIR.create(item, family) for item in selected)
    devices = tuple(EndpointIR.create(item, controllers)
                    for item in endpoint_selections)
    return _DeviceGraphResolution(
        controllers=controllers, devices=devices,
        resources=resources, triggers=triggers, dma=dma_plan,
        can_domain=(CanDomainIR(can_domain["split"], tuple(
            CanFilterIR(**item) for item in can_domain["filters"]))
            if can_domain is not None else None),
        pwm_shared_owners=tuple(PwmSharedOwnerIR.from_validated(item)
                               for item in pwm_shared_owners),
        inputs=((adc_group_contract[2],)
                if adc_group_contract is not None else ()))


def _resolve(assembly_path, root=ROOT, *, input_paths=None):
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
    assembly = load_assembly(assembly_path, root, snapshots)
    factory_options = obj(thaw(assembly.factory) if assembly.factory is not None else {}, (),
                          {"capabilities"}, "factory policy")
    if ("capabilities" in factory_options and
            type(factory_options["capabilities"]) is not bool):
        fail("Factory capabilities must be boolean")
    board_name = text(assembly.board_package, "Board package")
    board_dir = board_directory(assembly_path.parent / board_name)
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
    soc_dir = root / descriptor.maintained(family).source_directory
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
    validate_board_context(board)
    backend = assembly.backend
    if backend not in {"baremetal", "freertos", "native", "threadx"}:
        fail(f"Unsupported OS backend: {backend}")
    host = soc["cpu"]["arch"] == "native"
    if (host and backend not in {"native", "threadx"}) or (
            not host and backend == "native"):
        fail("Native is a host model; its backend cannot qualify MCU execution")
    if backend == "threadx":
        threadx.validate_platform(board["id"], family)
    cpu_profile = cpu_profiles.resolve(soc["cpu"], soc["irq"], backend,
                                       enum_abi=provider.ENUM_ABI)
    validate_interrupt_profile(cpu_profile, provider)
    clock = soc["clock_profiles"].get(assembly.clock_profile)
    if clock is None:
        fail(f"Unknown clock profile: {assembly.clock_profile}")
    clock_plan = provider.clock_plan(assembly.clock_profile, clock)
    clock = provider.normalize_clock(clock)
    clocks.match_board(clock, board["clocks"])
    i2s2_role = i2s2.resolve(thaw(assembly.i2s2), family=family,
                            part=part, backend=backend,
                            clock_profile=assembly.clock_profile)
    if assembly.os is not None and not isinstance(assembly.os, Mapping):
        fail("OS policy: expected object")
    kernel_profile = kernel.resolve(thaw(assembly.os), cpu_profile,
                                    clock["core_hz"])
    if (backend == "freertos" and kernel_profile.lowpower_port == "timebase" and
            not provider.TIMEBASE_ALARM):
        fail("Timebase lowpower requires the maintained STM32F407/GD32F470 SoC alarm")
    cpu_profile = kernel.bind(cpu_profile, kernel_profile)
    irq_profile = cpu_profile.irq
    if assembly.abi is not None and assembly.abi != cpu_abi(soc["cpu"]):
        fail("Assembly ABI contradicts the selected SoC")
    optimization = assembly.optimization
    if optimization not in {"O0", "Og", "O2", "Os", "O3", "O2-lto", "Os-lto", "O3-lto"}:
        fail(f"Unsupported optimization profile: {optimization}")
    selected_memory = _resolve_memory(assembly, variant, provider, part,
                                      kernel_profile)
    bindings = resolve_bindings(board, routes, part)
    device_graph = _resolve_device_graph(
        assembly, root=root, soc=soc, board=board, routes=routes,
        bindings=bindings, provider=provider, clock=clock, clock_plan=clock_plan,
        kernel_profile=kernel_profile, irq_profile=irq_profile,
        memory=selected_memory, i2s2_role=i2s2_role, read=read, snapshots=snapshots)
    inputs = [assembly_path, board_path, soc_path, routes_path, policy_path]
    inputs.extend(device_graph.inputs)
    if reference_path is not None:
        inputs.append(reference_path)
    for name in sequence(board.get("inputs", []), "declared Board inputs"):
        inputs.append(contained_file(board_dir, name))
    selected_layout = _resolve_flash_layout(
        assembly.layout, assembly_path.parent, variant["flash"], read)
    if selected_layout.input_path is not None:
        inputs.append(selected_layout.input_path)
    selected_sources = _resolve_source_dependencies(
        soc, provider, family, root, backend, read, source_dependency)
    inputs.extend(selected_sources.inputs)
    records = bind_input_identities(inputs, root, snapshots, input_paths)
    components = resolve_components(assembly)
    resolved = ConfigurationIR(
        board=board["id"], soc_family=family,
        cpu_profile=cpu_profile, kernel=kernel_profile,
        controllers=device_graph.controllers, devices=device_graph.devices,
        memory_budget=selected_memory.budget,
        factory=FactoryOptions(factory_options.get("capabilities", False)),
        can_domain=device_graph.can_domain,
        resource_plan=device_graph.resources, trigger_plan=device_graph.triggers,
        dma_topology=device_graph.dma,
        clock_plan=(ClockPlanIR.from_validated(clock_plan)
                    if clock_plan is not None else None),
        physical_part=PhysicalPartIR(part, variant["package"],
            freeze(variant["flash"]), selected_memory.regions),
        clock=freeze(clock), clock_profile=assembly.clock_profile,
        layout=selected_layout.layout, board_bindings=freeze(bindings),
        routes=freeze(routes), platform_metadata=freeze(platform_metadata),
        inputs=freeze(records), source_dependencies=selected_sources.dependencies,
        components=tuple(components),
        can_controllers=(tuple(provider.CAN_CONTROLLERS)
                         if device_graph.can_domain is not None else ()),
        optimization=optimization,
        physical_pcb_revision=board["physical_pcb_revision"],
        limitations=tuple(board["unknowns"]) + ("Physical qualification not executed",),
        physical_qualified=False,
        fact_references=freeze(references),
        pwm_shared_owners=device_graph.pwm_shared_owners,
        i2s2=i2s2_role)
    resolved = resolved.with_integrations()
    if assembly.power is not None:
        from tools.configure import runtime_power
        runtime = runtime_power.resolve(thaw(assembly.power), resolved)
        resolved = replace(resolved, runtime_power=RuntimePowerIR(
            runtime.mode, tuple(runtime.controllers)))
    projection = resolved.to_dict()
    del projection["configuration_sha256"]
    digest = hashlib.sha256(json.dumps(
        projection, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
    return replace(resolved, configuration_sha256=digest)


def c_id(value):
    return value.replace("-", "_")


normalize_resource = resource_graph.normalize_resource


def resolve_ir(assembly_path, root=ROOT, *, input_paths=None):
    """Return the immutable typed resolution consumed by every emitter."""
    try:
        return _resolve(assembly_path, root, input_paths=input_paths)
    except ConfigurationError as error:
        raise ConfigurationError(f"{assembly_path}: {error}",
                                 **error.details) from error


def generate(result, output):
    """Keep the generation entry point independent of bundle publication."""
    from tools.configure.emission import emit_bundle
    return emit_bundle(result, output)


def protected_inputs(assembly, root):
    """Read only path selectors before withdrawing an owned public bundle.

    Malformed authored bytes still invalidate previous output. This preflight
    makes no selection decisions and never replaces normal strict resolution.
    """
    assembly = Path(assembly).absolute()
    paths = [assembly, root.resolve(), ROOT, root / metadata.POLICY]
    try:
        content = assembly.read_text(encoding="utf-8")
        raw = tomllib.loads(content) if assembly.suffix == ".toml" else {}
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
                paths.append(root / descriptor.maintained(family).source_directory)
        except (OSError, ValueError, AttributeError):
            # This probe only protects output paths; resolve validates the
            # complete Board and reports malformed or unsupported facts.
            pass
    if isinstance(raw.get("layout"), str):
        paths.append(assembly.parent / raw["layout"])
    return paths


def configure(assembly, output, root=ROOT):
    inputs = protected_inputs(assembly, root)
    packages = [root / name for name in (
        "soc", "drivers", "boards", "dependencies", "vendors", "ext")]
    packages.extend(ROOT / name for name in (
        "soc", "drivers", "tools/configure", "tools/maintenance", "scripts/ci"))
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
        for record in result.inputs:
            if file_digest(Path(input_paths[record["path"]])) != record["sha256"]:
                fail(f"Input changed during generation: {record['path']}")
        dependencies.verify(initial_code_paths, initial_code_identity)
    return result.to_dict()


def main(argv=None):
    """Run the developer CLI; resolution and emission remain independent."""
    from tools.configure.experience import run
    return run(sys.modules[__name__], argv)


if __name__ == "__main__":
    raise SystemExit(main())
