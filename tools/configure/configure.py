#!/usr/bin/env python3
"""Resolve reviewed hardware facts and emit one atomic configuration bundle.

CMake remains the authority for source targets. This tool never discovers source
code, creates workers, selects application policy, or invents a hardware route.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

try:
    from .providers import maintained
    from .providers.common import (ConfigurationError, fail, integer, sequence,
                                   validate_selection)
except ImportError:
    from providers import maintained
    from providers.common import (ConfigurationError, fail, integer, sequence,
                                  validate_selection)

ROOT = Path(__file__).resolve().parents[2]
UINT32_MAX = (1 << 32) - 1
IDENTIFIER = re.compile(r"[A-Za-z][A-Za-z0-9_-]{0,63}")
PIN = re.compile(r"P([A-K])([0-9]|1[0-5])")
KINDS = {"gpio", "uart", "spi", "i2c", "flash", "watchdog", "exti",
         "timer", "pwm", "adc"}
C_KEYWORDS = set("auto break case char const continue default do double else enum extern float for goto if inline int long register restrict return short signed sizeof static struct switch typedef union unsigned void volatile while _Alignas _Alignof _Atomic _Bool _Complex _Generic _Imaginary _Noreturn _Static_assert _Thread_local main".split())


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
        records = load(manifest)["dependencies"]
        record = next((item for item in records if item["path"] == relative), None)
        if record is None:
            fail(f"Installed SDK has no source identity: {relative}")
        commit = record["commit"]
    else:
        checkout = root / relative
        if not (checkout / ".git").exists():
            fail(f"Initialize the locked SDK dependency: {relative}")
        def git(directory, *arguments):
            command = subprocess.run(["git", "-C", str(directory), *arguments],
                                     capture_output=True, text=True)
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


def uses_dma(mode):
    return mode.startswith("dma") or mode == "trigger-dma"


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
    return json.loads(contents.decode("utf-8"), object_pairs_hook=pairs,
                      parse_constant=lambda value: fail(f"Invalid number {value}"))


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
    obj(soc, {"schema_version", "id", "variants", "cpu", "clock_profiles",
              "controllers"}, {"provenance", "source_lock", "reserved_resources"},
        "SoC")
    integer(soc["schema_version"], 1, 1, "SoC schema")
    identity(soc["id"], "SoC id")
    if not isinstance(soc["variants"], dict) or part not in soc["variants"]:
        fail(f"Unmaintained exact part: {part}")
    variant = obj(soc["variants"][part], {"package", "flash", "memory"},
                  {"pins"}, "variant")
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
        obj(region, {"id", "origin", "size", "dma", "linker"}, (), "memory")
        identity(region["id"], "memory id")
        if type(region["dma"]) is not bool or type(region["linker"]) is not bool:
            fail("Memory DMA/linker facts must be boolean")
        start, end = address_range(region["origin"], region["size"], region["id"])
        if any(start < old_end and old_start < end for old_start, old_end, _ in ranges):
            fail(f"Overlapping physical memory domain: {region['id']}")
        ranges.append((start, end, region["id"]))
    if sum(region["linker"] for region in memory) != 1:
        fail("Exactly one maintained default RAM linker domain is required")
    cpu = obj(soc["cpu"], {"arch", "fpu", "float_abi"}, (), "CPU ABI")
    expected = maintained(soc["id"]).CPU_ABI
    if cpu != expected:
        fail(f"Unsupported CPU/FPU ABI: {cpu}")
    if not isinstance(soc["controllers"], dict):
        fail("SoC controllers must be an object")
    for name, controller in soc["controllers"].items():
        identity(name, "controller")
        obj(controller, {"kind", "modes", "irq", "pins"},
            {"resources", "max_hz", "irq_numbers", "channel_count"}, name)
        if controller["kind"] not in KINDS:
            fail(f"Unmaintained controller kind: {controller['kind']}")
        for field in ("modes", "irq", "pins"):
            values = sequence(controller[field], f"{name}.{field}")
            unique(values, f"{name}.{field}")
            for value in values:
                text(value, f"{name}.{field}")
        if not controller["modes"]:
            fail(f"Controller {name} has no maintained mode")
    if not isinstance(soc["clock_profiles"], dict) or not soc["clock_profiles"]:
        fail("Clock profiles are required")
    for name, profile in soc["clock_profiles"].items():
        identity(name, "clock profile")
        obj(profile, {"hse_hz", "core_hz"},
            {"apb1_hz", "apb2_hz", "timebase", "flash_wait_states"}, name)
        integer(profile["hse_hz"], 1, context="HSE Hz")
        integer(profile["core_hz"], 1, context="core Hz")
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


def resolve(assembly_path, root=ROOT, *, input_paths=None):
    """Validate all input facts before producing the sole resolved decision."""
    root = root.resolve()
    if assembly_path.is_symlink():
        fail("Assembly must not be a symlink")
    snapshots = {}
    read = lambda path: load(path, snapshots)
    assembly_path = assembly_path.resolve()
    assembly_input = load_assembly(assembly_path, root, snapshots)
    assembly = obj(assembly_input,
                   {"schema_version", "board_package", "backend", "clock_profile",
                    "controllers", "devices", "memory_budgets", "layout"},
                   {"abi", "optimization", "components"}, "assembly")
    integer(assembly["schema_version"], 1, 1, "assembly schema")
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
    if soc["id"] != family:
        fail("Board SoC family differs from the package identity")
    obj(routes_doc, {"schema_version", "routes"}, (), "routes")
    integer(routes_doc["schema_version"], 1, 1, "routes schema")
    routes = {}
    for route in sequence(routes_doc["routes"], "routes"):
        obj(route, {"id", "variants", "controller", "kind", "pins", "modes",
                    "source"}, {"resources", "dma", "mode_resources"}, "route")
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
        if not controller or controller["kind"] != route["kind"]:
            fail(f"Route {route['id']} does not match a maintained controller")
        if not set(route["modes"]).issubset(controller["modes"]):
            fail(f"Route {route['id']} advertises unsupported modes")
        mode_resources = obj(route.get("mode_resources", {}), set(), set(route["modes"]),
                             "mode-specific resources")
        for mode, resources in mode_resources.items():
            if mode not in route["modes"]:
                fail("Mode-specific resource has no maintained route mode")
            for resource in sequence(resources, "mode-specific resources"):
                normalize_resource(text(resource, "mode-specific resource"))
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
    text(board["source_revision"], "Board source revision")
    if board["physical_pcb_revision"] is not None:
        text(board["physical_pcb_revision"], "physical PCB revision")
    obj(board["clocks"], {"hse_hz"}, {"lse_hz"}, "Board clocks")
    integer(board["clocks"]["hse_hz"], 1, context="Board HSE")
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
    if backend not in {"baremetal", "freertos", "native"}:
        fail(f"Unsupported OS backend: {backend}")
    if (provider.MODEL) != (backend == "native"):
        fail("Native is a host model; its backend cannot qualify MCU execution")
    clock = soc["clock_profiles"].get(assembly["clock_profile"])
    if clock is None:
        fail(f"Unknown clock profile: {assembly['clock_profile']}")
    if clock["hse_hz"] != board["clocks"]["hse_hz"]:
        fail("Clock profile does not match the Board oscillator")
    if "abi" in assembly and assembly["abi"] != soc["cpu"]:
        fail("Assembly ABI contradicts the selected SoC")
    optimization = assembly.get("optimization", "Os")
    if optimization not in {"O2", "Os", "O3", "O2-lto", "Os-lto", "O3-lto"}:
        fail(f"Unsupported optimization profile: {optimization}")
    budgets = obj(assembly["memory_budgets"], {"main_stack_bytes"},
                  {"flash_load_bytes", "static_ram_bytes", "libc_heap_bytes"},
                  "memory budgets")
    for name, value in budgets.items():
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
    bindings = {}
    for binding in sequence(board["bindings"], "Board bindings"):
        obj(binding, {"id", "controller", "route"}, {"initial", "mask"}, "binding")
        identity(binding["id"], "binding id")
        if binding["id"] in bindings:
            fail(f"Duplicate Board binding: {binding['id']}")
        route = routes.get(binding["route"])
        if not route or part not in route["variants"] or route["controller"] != binding["controller"]:
            fail(f"Board binding {binding['id']} has no reviewed exact-part route")
        if "initial" in binding:
            integer(binding["initial"], 0, UINT32_MAX, "GPIO initial")
        if "mask" in binding:
            integer(binding["mask"], 1, 65535, "GPIO mask")
            actual_mask = sum(1 << int(PIN.fullmatch(pin["pin"])[2]) for pin in route["pins"])
            if binding["mask"] != actual_mask:
                fail(f"GPIO binding {binding['id']} mask differs from its reviewed pins")
            if binding.get("initial", 0) & ~binding["mask"]:
                fail(f"GPIO initial sets unauthorized bits: {binding['id']}")
        bindings[binding["id"]] = binding
    claims = {}
    claim_list = []

    def claim(key, owner, mode, source):
        text(key, "resource key")
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
        previous = claims.get(normalize_resource(text(key, "resource key")))
        if previous is not None and previous["owner"] == "soc-timebase":
            continue
        claim(key, owner, "reserved", board["id"])
    if backend == "freertos":
        for irq in ("SysTick", "PendSV", "SVC"):
            claim("irq:" + irq, "freertos", "kernel", "FreeRTOS Cortex-M4F")
    selected = []
    instance_ids = []
    irq_owners = {}
    for selection in sequence(assembly["controllers"], "selected controllers"):
        obj(selection, {"id", "binding", "mode"},
            {"irq_priority", "rx_capacity", "baud", "max_hz", "event_capacity",
             "period_ticks", "duty_ticks", "channels", "sample_cycles",
             "timeout_ms", "calls_os", "rx_profile", "sample_times",
             "reference_mv", "tick_hz", "edge", "authored_kind"}, "controller selection")
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
            fail(f"Unsupported mode {mode} for {selection['binding']}")
        validate_selection(selection, route, provider, ram["size"])
        for numeric in {"rx_capacity", "baud", "max_hz", "event_capacity",
                        "period_ticks", "sample_cycles", "timeout_ms"} & selection.keys():
            integer(selection[numeric], 1, context=numeric)
        if "duty_ticks" in selection:
            integer(selection["duty_ticks"], 0, context="duty ticks")
            if selection["duty_ticks"] > selection.get("period_ticks", 0):
                fail("PWM duty exceeds period")
        if "irq_priority" in selection:
            integer(selection["irq_priority"], 0, 15, "IRQ priority")
        if route["kind"] == "spi" and mode == "short-poll" and ({"irq_priority", "calls_os"} & selection.keys()):
            fail("Polling SPI has no IRQ options")
        if route["kind"] == "spi" and mode.startswith("dma") and "irq_priority" not in selection:
            fail("DMA SPI requires an explicit IRQ priority")
        if "calls_os" in selection and type(selection["calls_os"]) is not bool:
            fail("calls_os must be boolean")
        if selection.get("calls_os", False) and (backend != "freertos" or
                                                 selection.get("irq_priority", 0) < 5):
            fail("OS-calling ISR violates the FreeRTOS syscall ceiling")
        if selection.get("max_hz", 0) > hardware.get("max_hz", UINT32_MAX):
            fail("Controller speed exceeds maintained limit")
        if uses_dma(mode) and not route.get("dma"):
            fail("DMA mode has no reviewed DMA route")
        if uses_dma(mode):
            provider.validate_dma(route, mode, normalize_resource)
        if route["kind"] != "gpio":
            claim("controller:" + route["controller"], name, mode, route["id"])
        for pin in route["pins"]:
            claim("pin:" + pin["pin"], name, mode, route["id"])
        if mode in {"irq-byte-event", "irq", "irq-blocks", "edge-event", "edge"} or uses_dma(mode):
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
            claim(normalize_resource(resource), name, mode, route["id"])
        if uses_dma(mode):
            for dma in route["dma"]:
                claim("dma:" + dma, name, mode, route["id"])
                claim(provider.dma_irq(dma), name, mode, route["id"])
        selected.append({**selection, "controller": route["controller"],
                         "kind": route["kind"], "route": route["id"],
                         "pins": route["pins"], "initial": binding.get("initial", 0),
                         "mask": binding.get("mask", 0),
                         **({"dma": route["dma"]} if uses_dma(mode) else {})})
    unique(instance_ids, "controller instance IDs")
    unique([c_id(name).upper() for name in instance_ids], "generated C identifiers")
    device_ids = []
    addresses = set()
    chip_selects = set()
    for device in sequence(assembly["devices"], "devices"):
        obj(device, {"id", "controller", "driver"}, {"cs_binding", "address",
            "mode", "max_hz", "authored_kind", "model_bytes"}, "device")
        device_ids.append(identity(device["id"], "device ID"))
        c_identity(device["id"])
        controller = next((item for item in selected if item["id"] == device["controller"]), None)
        if controller is None or controller["kind"] not in {"spi", "i2c"}:
            fail("Device references no selected SPI/I2C controller")
        authored_kind = device.pop("authored_kind", controller["kind"])
        if authored_kind != controller["kind"]:
            fail("Authored device kind differs from controller binding kind")
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
        if device["driver"] not in {"spi-endpoint", "i2c-endpoint", "bmp280"}:
            fail(f"Unknown maintained device driver: {device['driver']}")
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
    for controller in selected:
        children = [item for item in assembly["devices"] if item["controller"] == controller["id"]]
        if controller["kind"] == "i2c" and not children:
            fail("I2C requires at least one explicit address endpoint")
        if controller["kind"] == "spi" and provider.SPI_REQUIRES_ENDPOINT and not children:
            fail("SPI requires at least one reviewed CS endpoint")
    layout = variant["flash"].copy()
    inputs = [assembly_path, board_path, soc_path, routes_path]
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
            inputs.append(contained_file(root, lock))
        else:
            obj(lock, {"cmsis_core", "cmsis_device_f4"}, (), "SoC source lock")
            if any(not isinstance(value, str) or not re.fullmatch(r"[0-9a-f]{40}", value)
                   for value in lock.values()):
                fail("SDK Git dependencies require exact commit identities")
            for key, path in {"cmsis_core": "vendors/arm/CMSIS_5",
                              "cmsis_device_f4": "vendors/st/cmsis_device_f4"}.items():
                source_dependencies.append(dependency_identity(root, path, lock[key]))
    if backend == "freertos":
        source_dependencies.append(dependency_identity(root, "ext/freertos"))
    manifest = root / ".nexus-source-sdk.json"
    if manifest.is_file():
        inputs.append(manifest)
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
    components = sequence(assembly.get("components", []), "components")
    for component in components:
        text(component, "component")
    unique(components, "components")
    maintained_components = {"bus-owner", "spi-owner", "spi-async-owner", "uart-owner", "bmp280", "bmp280-spi",
                             "log", "storage", "flash-storage", "modbus-rtu"}
    for component in components:
        if component not in maintained_components:
            fail(f"Unknown maintained component: {component}")
    if any(device["driver"] == "bmp280" for device in assembly["devices"]):
        if "bmp280-spi" not in components:
            fail("BMP280 SPI device requires the maintained bmp280-spi component")
    result = {"schema_version": 1, "board": board["id"], "soc_family": family,
              "part": part, "package": variant["package"], "backend": backend,
              "cpu": soc["cpu"], "clock_profile": assembly["clock_profile"],
              "clock": clock, "memory": variant["memory"], "flash": variant["flash"],
              "layout": layout, "controllers": selected, "devices": assembly["devices"],
              "components": components, "claims": claim_list, "memory_budgets": budgets,
              "board_bindings": bindings, "routes": routes,
              "optimization": optimization, "inputs": records,
              "source_dependencies": source_dependencies,
              "physical_pcb_revision": board["physical_pcb_revision"],
              "limitations": board["unknowns"] + ["Physical qualification not executed"],
              "physical_qualified": False,
              "enum_abi": provider.ENUM_ABI}
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
        match = re.fullmatch(r"dma:DMA([12]):stream([0-7])(?::channel([0-7]))?", value)
        if match is None:
            fail(f"Invalid physical DMA resource: {value}")
        return f"dma:DMA{match[1]}:stream{match[2]}"
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
    return ConfigurationIR.from_validated(
        resolve(assembly_path, root, input_paths=input_paths))


def generate(result, output):
    """Emit narrow compile/link inputs; this never emits a code dependency graph."""
    write = lambda name, data: (output / name).write_text(data, encoding="utf-8")
    write("resolved.json", json.dumps(result.to_dict() if hasattr(result, "to_dict")
                                      else result, indent=2, sort_keys=True) + "\n")
    header = ["/* Generated from the sole resolved configuration. */",
              "#ifndef NEXUS_CONFIG_H", "#define NEXUS_CONFIG_H",
              f'#define NEXUS_CONFIG_SHA256 "{result["configuration_sha256"]}"',
              f'#define NEXUS_EXACT_PART "{result["part"]}"',
              f'#define NEXUS_CORE_HZ {result["clock"]["core_hz"]}u',
              f'#define NEXUS_HSE_HZ {result["clock"]["hse_hz"]}u',
              f'#define NEXUS_MAIN_STACK_BYTES {result["memory_budgets"]["main_stack_bytes"]}u',
              f'#define NEXUS_BACKEND_{result["backend"].upper()} 1',
              '#define NEXUS_IRQ_SYSCALL_PRIORITY 5u',
              '#define NEXUS_IRQ_PRIORITY_BITS 4u']
    cmake = [f'set(NEXUS_SOC_FAMILY "{result["soc_family"]}")',
             f'set(NEXUS_EXACT_PART "{result["part"]}")',
             f'set(NEXUS_CPU_ARCH "{result["cpu"]["arch"]}")',
             f'set(NEXUS_CPU_FPU "{result["cpu"]["fpu"]}")',
             f'set(NEXUS_FLOAT_ABI "{result["cpu"]["float_abi"]}")',
             f'set(NEXUS_ENUM_ABI "{result["enum_abi"]}")',
             f'set(NEXUS_BACKEND "{result["backend"]}")',
             f'set(NEXUS_CONFIG_SHA256 "{result["configuration_sha256"]}")',
             f'set(NEXUS_OPTIMIZATION "{result["optimization"]}")',
             'set(NEXUS_SELECTED_KINDS "' + ';'.join(sorted({item['kind'] for item in result['controllers']})) + '")',
             'set(NEXUS_SELECTED_COMPONENTS "' + ';'.join(result['components']) + '")']
    for selection in result["controllers"]:
        prefix = "NEXUS_" + c_id(selection["id"]).upper()
        header.append(f"#define {prefix}_SELECTED 1")
        if selection["kind"] == "gpio":
            mask = selection["mask"] or sum(1 << int(pin["pin"][2:]) for pin in selection["pins"])
            header.append(f"#define {prefix}_MASK {mask}u")
        for field in ("rx_capacity", "baud", "irq_priority", "event_capacity"):
            if field in selection:
                header.append(f"#define {prefix}_{field.upper()} {selection[field]}u")
    header.extend(["#endif", ""])
    write("nexus_config.h", "\n".join(header))
    write("selection.cmake", "\n".join(cmake) + "\n")
    try:
        from .bindings import emit
    except ImportError:
        from bindings import emit
    try:
        from .factory import emit as emit_factory
    except ImportError:
        from factory import emit as emit_factory
    factory_header, factory_source = emit_factory(result)
    write("nexus_factory.h", factory_header)
    bindings_header, bindings_source = emit(result)
    bindings_source += "\n" + factory_source
    write("nexus_bindings.h", bindings_header)
    write("bindings.c", bindings_source)
    ram = next(region for region in result["memory"] if region["linker"])
    flash = result["layout"]
    stack = result["memory_budgets"]["main_stack_bytes"]
    heap = result["memory_budgets"].get("libc_heap_bytes", 0)
    write("memory.ld", f'''/* Exact resolved physical layout; no product reservation. */
ENTRY(Reset_Handler)
PHDRS {{
    flash PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
    ram PT_LOAD FLAGS(6);
    stack PT_LOAD FLAGS(6);
}}
MEMORY {{
    FLASH (rx) : ORIGIN = 0x{flash["origin"]:08x}, LENGTH = {flash["size"]}
    RAM (rwx) : ORIGIN = 0x{ram["origin"]:08x}, LENGTH = {ram["size"]}
}}
_estack = ORIGIN(RAM) + LENGTH(RAM);
_sp = _estack;
_Min_Stack_Size = {stack};
_Min_Heap_Size = {heap};
__nx_msp_start = _estack - _Min_Stack_Size;
__nx_msp_end = _estack;
__nx_resolved_sha256_0 = 0x{result["configuration_sha256"][0:8]};
__nx_resolved_sha256_1 = 0x{result["configuration_sha256"][8:16]};
__nx_resolved_sha256_2 = 0x{result["configuration_sha256"][16:24]};
__nx_resolved_sha256_3 = 0x{result["configuration_sha256"][24:32]};
__nx_resolved_sha256_4 = 0x{result["configuration_sha256"][32:40]};
__nx_resolved_sha256_5 = 0x{result["configuration_sha256"][40:48]};
__nx_resolved_sha256_6 = 0x{result["configuration_sha256"][48:56]};
__nx_resolved_sha256_7 = 0x{result["configuration_sha256"][56:64]};
SECTIONS {{
    .isr_vector : {{ KEEP(*(.isr_vector)) KEEP(*(.vectors)) }} > FLASH :flash
    .text : {{ *(.text*) *(.rodata*) KEEP(*(.init)) KEEP(*(.fini)) }} > FLASH :flash
    .ARM.extab : {{ *(.ARM.extab*) }} > FLASH :flash
    .ARM.exidx : {{ __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; }} > FLASH :flash
    .preinit_array : {{ PROVIDE_HIDDEN(__preinit_array_start = .); KEEP(*(.preinit_array*)) PROVIDE_HIDDEN(__preinit_array_end = .); }} > FLASH :flash
    .init_array : {{ PROVIDE_HIDDEN(__init_array_start = .); KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array*)) PROVIDE_HIDDEN(__init_array_end = .); }} > FLASH :flash
    .fini_array : {{ PROVIDE_HIDDEN(__fini_array_start = .); KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array*)) PROVIDE_HIDDEN(__fini_array_end = .); }} > FLASH :flash
    _sidata = LOADADDR(.data);
    .data : {{ . = ALIGN(4); _sdata = .; *(.data*) . = ALIGN(4); _edata = .; }} > RAM AT> FLASH :data
    .bss (NOLOAD) : AT(ADDR(.bss)) {{ . = ALIGN(4); _sbss = .; *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; }} > RAM :ram
    .reserved (NOLOAD) : AT(ADDR(.reserved)) {{ . = ALIGN(8); __heap_start = .; __nx_heap_start = .; . += _Min_Heap_Size; __heap_end = .; __nx_heap_end = .; }} > RAM :ram
    .msp __nx_msp_start (NOLOAD) : AT(__nx_msp_start) {{ . += _Min_Stack_Size; }} > RAM :stack
    PROVIDE(end = _ebss);
    PROVIDE(_end = _ebss);
    ASSERT(_ebss + _Min_Heap_Size <= _estack - _Min_Stack_Size, "RAM stack/heap overlap")
}}
''')
    write("resource-budget.json", json.dumps({"schema_version": 1,
          "flash_loaded_max": result["memory_budgets"].get("flash_load_bytes", flash["size"]),
          "flash_footprint_max": result["memory_budgets"].get("flash_load_bytes", flash["size"]),
          "ram_reserved_max": {region["id"]: result["memory_budgets"].get("static_ram_bytes", region["size"])
                               if region["linker"] else 0 for region in result["memory"]},
          "msp_max": stack, "heap_max": heap}, indent=2, sort_keys=True) + "\n")
    write(".nexus-config-bundle", result["configuration_sha256"] + "\n")


def configure(assembly, output, root=ROOT):
    output = output.absolute()
    if output.is_symlink() or output == root.resolve() or output == output.parent:
        fail("Unsafe configuration output directory")
    if output.exists():
        if not output.is_dir() or not (output / ".nexus-config-bundle").is_file():
            fail("Existing output is not an owned configuration bundle")
        # Invalidate before parsing; a rejected reconfiguration cannot reuse old
        # successful headers or selection files, including after process failure.
        shutil.rmtree(output)
    input_paths = {}
    result = resolve_ir(assembly, root, input_paths=input_paths)
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix=".nexus-config-", dir=output.parent))
    try:
        generate(result, temporary)
        (temporary / "input_paths.json").write_text(
            json.dumps(input_paths, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        os.replace(temporary, output)
    except BaseException:
        shutil.rmtree(temporary, ignore_errors=True)
        raise
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
                            choices=("native", "baremetal", "freertos"))
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
