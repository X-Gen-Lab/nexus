"""Small, strict TOML authoring surface; hardware facts remain JSON."""
import tomllib
from dataclasses import dataclass
from collections.abc import Mapping
from types import MappingProxyType

from tools.configure.ir import Record, freeze
from tools.configure.contracts import (
    CONTROLLER_CONTRACTS, CONTROLLER_KINDS, ENDPOINT_FIELDS, USB_ENDPOINT_FIELDS,
    EXTERNAL_ENDPOINT_FIELDS, EXTERNAL_ENDPOINT_KINDS, EXTERNAL_ENDPOINT_REQUIRED,
    SERIAL_FLASH_FIELDS)


@dataclass(frozen=True)
class AuthoredAssembly(Record):
    """Parsed user intent; unresolved choices are never hardware decisions."""
    board_package: str
    backend: str
    clock_profile: str
    controllers: tuple[Mapping, ...]
    devices: tuple[Mapping, ...]
    memory_budgets: Mapping
    layout: str | None
    source_fields: tuple[str, ...]
    abi: str | None = None
    optimization: str = "Os"
    components: tuple[str, ...] = ()
    os: Mapping | None = None
    factory: Mapping | None = None
    can_domain: Mapping | None = None
    power: Mapping | None = None
    soc_package: str | None = None
    hardware: Mapping | None = None
    clock_parameters: Mapping | None = None

    @classmethod
    def create(cls, values):
        return cls(**{key: freeze(value) for key, value in values.items()},
                   source_fields=tuple(values))

    def projection(self):
        return {name: getattr(self, name) for name in self.source_fields}

# Exact field objects are shared with selection and public interface metadata.
INSTANCE_FIELDS = MappingProxyType(
    {kind: contract.authored_fields
     for kind, contract in CONTROLLER_CONTRACTS.items()})
KINDS = frozenset(CONTROLLER_KINDS)
MEMORY_FIELDS = {"main_stack_bytes", "libc_heap_bytes", "static_ram_limit_bytes",
                 "flash_load_limit_bytes", "image_prefix_bytes",
                 "image_suffix_bytes", "regions"}
TOP_REQUIRED = {"schema", "backend", "clock", "memory"}
TOP_OPTIONAL = {"board", "board_package", "soc_package", "optimization",
                "components", "layout", "abi", "os", "factory", "can_domain",
                "power", "hardware", "clock_parameters"}



def builtin_board_directory(root, name, fail, *, test_support=False):
    """Resolve production Boards or the explicitly enabled host-test fixture."""
    if name == "native_reference":
        if not test_support:
            fail("Native host model requires explicit test support")
        return root / "tests/support/native/board"
    return root / "boards" / name


def normalize(data, path, root, fail, obj, integer, text, *,
              test_support=False):
    obj(data, TOP_REQUIRED, TOP_OPTIONAL | KINDS | set(EXTERNAL_ENDPOINT_KINDS) | {"spi_device", "i2c_device", "usb_endpoint"},
        "assembly TOML")
    integer(data["schema"], 2, 2, "assembly schema")
    if ("board" in data) == ("board_package" in data):
        fail("Select exactly one Board id or explicit board_package")
    if "board" in data:
        name = text(data["board"], "Board id")
        if "/" in name or "\\" in name or name in {".", ".."}:
            fail("Board id cannot be a path; use explicit board_package")
        package = builtin_board_directory(root, name, fail,
                                          test_support=test_support)
        if not (package / "board.json").is_file():
            fail(f"Unknown Board package id: {name}")
        package = str(package)
    else:
        package = text(data["board_package"], "Board package")
    memory = obj(data["memory"], {"main_stack_bytes"},
                 MEMORY_FIELDS - {"main_stack_bytes"},
                 "memory TOML")
    renamed = {"static_ram_limit_bytes": "static_ram_bytes",
               "flash_load_limit_bytes": "flash_load_bytes"}
    budgets = {renamed.get(key, key): value for key, value in memory.items()}
    controllers = []
    for kind in sorted(KINDS):
        instances = data.get(kind, {})
        if not isinstance(instances, dict):
            fail(f"{kind}: expected instance tables")
        for name, definition in instances.items():
            if not isinstance(definition, dict):
                fail(f"{kind}.{name}: expected instance table")
            obj(definition, {"binding", "mode"}, INSTANCE_FIELDS[kind] - {"binding", "mode"},
                f"{kind}.{name}")
            selection = dict(definition)
            selection["id"] = name
            selection["authored_kind"] = kind
            if kind == "uart":
                from tools.configure import uart_formats
                uart_formats.normalize_authored(selection, name, obj)
            elif "irq" in INSTANCE_FIELDS[kind] and "irq" in selection:
                irq = obj(selection.pop("irq"), {"priority"}, {"calls_os"}, f"{kind}.{name}.irq")
                selection["irq_priority"] = irq["priority"]
                if "calls_os" in irq:
                    selection["calls_os"] = irq["calls_os"]
            controllers.append(selection)
    devices = []
    for kind in EXTERNAL_ENDPOINT_KINDS:
        instances = data.get(kind, {})
        if not isinstance(instances, dict):
            fail(f"{kind}: expected endpoint tables")
        for name, definition in instances.items():
            required = EXTERNAL_ENDPOINT_REQUIRED[kind]
            optional = {"parameters"} if kind == "memory_window" else set()
            if (kind == "flash_async" and isinstance(definition, dict) and
                    definition.get("mode") in {
                        "nor-indirect", "nor-indirect-quad-read"}):
                required = SERIAL_FLASH_FIELDS
            if (kind == "memory_window" and isinstance(definition, dict) and
                    not ({"read_timing", "write_timing"} & set(definition))):
                required = required - {"read_timing", "write_timing"} | {"parameters"}
                optional = set()
            obj(definition, required, optional, f"{kind}.{name}")
            devices.append({**definition, "id": name,
                            "driver": kind + "-endpoint",
                            "authored_kind": kind})
    for kind in ("spi", "i2c"):
        instances = data.get(kind + "_device", {})
        if not isinstance(instances, dict):
            fail(f"{kind}_device: expected endpoint tables")
        for name, definition in instances.items():
            if not isinstance(definition, dict):
                fail(f"{kind}_device.{name}: expected endpoint table")
            obj(definition, {"controller"}, ENDPOINT_FIELDS[kind] - {"controller"},
                f"{kind}_device.{name}")
            device = dict(definition)
            device["id"] = name
            device.setdefault("driver", kind + "-endpoint")
            device["authored_kind"] = kind
            devices.append(device)
    endpoints = data.get("usb_endpoint", {})
    if not isinstance(endpoints, dict):
        fail("USB endpoints: expected endpoint tables")
    for name, definition in endpoints.items():
        obj(definition, USB_ENDPOINT_FIELDS - {"fifo_words"}, {"fifo_words"},
            "USB endpoint " + name)
        devices.append({**definition, "id": name, "driver": "usb-fs-endpoint",
                        "authored_kind": "usb_device"})
    result = {"board_package": package,
              "backend": data["backend"], "clock_profile": data["clock"],
              "controllers": controllers, "devices": devices,
              "memory_budgets": budgets, "layout": data.get("layout")}
    for field in ("soc_package", "abi", "optimization", "components", "os", "factory", "can_domain", "power", "hardware", "clock_parameters"):
        if field in data:
            result[field] = data[field]
    return AuthoredAssembly.create(result)


def read(path, snapshots, root, fail, obj, integer, text, *,
         test_support=False):
    if path.is_symlink() or not path.is_file():
        fail(f"Assembly input must be a regular file: {path}")
    contents = path.read_bytes()
    snapshots[path] = contents
    try:
        data = tomllib.loads(contents.decode("utf-8"))
    except (tomllib.TOMLDecodeError, UnicodeDecodeError) as error:
        fail(f"Invalid TOML: {error}")
    return normalize(data, path, root, fail, obj, integer, text,
                     test_support=test_support)


def editor_schema():
    """Expose the same authoring fields for editor completion, not resolution."""
    from tools.configure import (
        kernel, threadx, uart_formats, stream_profiles, timer_advanced, can,
        i2c_timing_selection, ethernet)

    def record(fields, required=()):
        properties = {name: {"type": "string"} for name in sorted(fields)}
        if "pull" in fields:
            properties["pull"]["enum"] = ["none", "up", "down"]
        if "output_type" in fields:
            properties["output_type"]["enum"] = ["push-pull", "open-drain"]
        if "speed" in fields:
            properties["speed"]["enum"] = ["low", "medium", "high", "very-high"]
        block_bounds = {"bus_width": {"type": "integer", "enum": [1, 4, 8]},
                        "max_blocks": {"type": "integer", "minimum": 1,
                                       "maximum": 32},
                        "poll_budget": {"type": "integer", "minimum": 1,
                                        "maximum": 1000000},
                        "stop_timeout_us": {"type": "integer", "minimum": 1,
                                            "maximum": 2**32 - 1}}
        numeric = {"baud", "capacity", "priority", "main_stack_bytes", "libc_heap_bytes",
                   "static_ram_limit_bytes", "flash_load_limit_bytes", "max_hz",
                   "event_capacity", "period_ticks", "duty_ticks", "tick_hz",
                   "reference_mv", "timeout_ms", "address", "model_bytes",
                   "bitrate", "drain_timeout_us", "poll_limit", "prescaler",
                   "segment1", "segment2", "sjw", "split", "bank", "id", "mask", "fifo",
                   "rx_fifo_words", "clock_timeout_us", "number", "max_packet", "fifo_words",
                   "tx_count", "rx_count", "rx_storage_bytes", "init_timeout_us",
                   "filter", "filter_a", "filter_b", "deadtime_ticks",
                   "trigger_ticks", "data_bitrate", "period_us", "lower",
                   "upper", "image_prefix_bytes", "image_suffix_bytes", "drain_us"}
        for name in fields & numeric:
            properties[name] = {"type": "integer", "minimum": 0}
        for name in fields & block_bounds.keys():
            properties[name] = block_bounds[name]
        for name in fields & {"image_prefix_bytes", "image_suffix_bytes"}:
            properties[name]["maximum"] = 2**32 - 1
        for name in fields & {"channels", "sample_times"}:
            properties[name] = {"type": "array", "items": {"type": "integer", "minimum": 0}}
        if "resolution_bits" in fields:
            from tools.configure.adc import RESOLUTION_BITS
            properties["resolution_bits"] = {
                "type": "integer", "enum": list(RESOLUTION_BITS), "default": 12}
        if "delay_cycles" in fields:
            properties["delay_cycles"] = {
                "type": "integer", "minimum": 5, "maximum": 20, "default": 5}
        if "address_bits" in fields:
            properties["address_bits"] = {
                "type": "integer", "enum": [7, 10], "default": 7}
        if "active_slots" in fields:
            properties["active_slots"] = {
                "type": "integer", "minimum": 1, "maximum": 65535,
                "description": "SAI enabled slot mask; omitted selects every "
                               "declared slot. Inactive slots have no FIFO "
                               "storage; this does not reorder samples."}
        if "window_ticks" in fields:
            properties["window_ticks"] = {
                "type": "integer", "minimum": 1, "maximum": 62, "default": 1}
        if "threshold_code" in fields:
            properties["threshold_code"] = {
                "type": "integer", "minimum": 0, "maximum": 7,
                "description": "PVD PLS encoding; consult the exact datasheet "
                               "threshold bounds and hysteresis. No voltage "
                               "policy is selected by default."}
        for name in fields & {"active_high", "fifo_order", "remote", "match_remote", "vbus_sensing", "remote_wakeup",
                             "break_active_high", "idle_high", "idle_complement_high", "exclusive", "invert_a", "invert_b", "lsb_first", "maintenance", "checksum_offload"}:
            properties[name] = {"type": "boolean"}
        if "lsb_first" in fields:
            properties["lsb_first"].update(
                default=False,
                description="Shift each SPI frame LSB first; false keeps MSB "
                            "first. Requires a reviewed endpoint provider.")
        if "timing" in fields:
            properties["timing"] = record(
                {"prescaler", "segment1", "segment2", "sjw"},
                {"prescaler", "segment1", "segment2", "sjw"})
        if "layers" in fields:
            dimensions = {"width", "height", "hsync", "hback_porch", "hfront_porch",
                          "vsync", "vback_porch", "vfront_porch"}
            polarities = {"hsync_polarity", "vsync_polarity", "de_polarity",
                          "clock_polarity"}
            timing = record(dimensions | polarities, dimensions | polarities)
            for name in dimensions:
                timing["properties"][name] = {
                    "type": "integer",
                    "minimum": 0 if name in {"hback_porch", "vback_porch"} else 1}
            for name in polarities - {"clock_polarity"}:
                timing["properties"][name] = {"enum": ["active-low", "active-high"]}
            timing["properties"]["clock_polarity"] = {"enum": ["normal", "inverted"]}
            properties["timing"] = timing
            names = {"layer", "x", "y", "width", "height", "formats", "alpha", "pixel_alpha"}
            layer = record(names, names)
            for name in names - {"formats", "pixel_alpha"}:
                layer["properties"][name] = {"type": "integer", "minimum": 0}
            layer["properties"]["formats"] = {
                "type": "array", "minItems": 1, "uniqueItems": True,
                "items": {"enum": ["rgb565", "argb8888"]}}
            layer["properties"]["pixel_alpha"] = {"type": "boolean"}
            properties["layers"] = {"type": "array", "minItems": 1,
                                     "maxItems": 2, "items": layer}
        if "mode" in fields and "controller" in fields:
            properties["mode"] = {"type": "integer", "minimum": 0, "maximum": 3}
        if "irq" in fields:
            properties["irq"] = record({"priority", "calls_os"}, {"priority"})
            properties["irq"]["properties"]["calls_os"] = {"type": "boolean"}
        from tools.configure import usb_otg, usb_host
        usb_otg.nested_editor_schema(properties, fields, record)
        usb_host.pipe_editor_schema(properties, fields)
        uart_formats.receive_editor_schema(properties, fields, record)
        from tools.configure import uart_wires
        uart_wires.editor_schema(properties, fields)
        from tools.configure import spi_frames
        spi_frames.editor_schema(properties, fields)
        stream_profiles.nested_editor_schema(properties, fields, record)
        if "watchdog" in fields:
            properties["watchdog"] = record({"lower", "upper"},
                                             {"lower", "upper"})
        if "max_service_gap_us" in fields:
            properties["max_service_gap_us"] = {
                "type": "integer", "minimum": 1, "maximum": 2**32 - 1}
        if "injected" in fields:
            injected_fields = {"channels", "sample_times", "min_interval_us"}
            properties["injected"] = record(
                injected_fields | {"trigger", "delay_us"}, {"channels", "sample_times"})
            properties["injected"]["properties"]["trigger"] = {
                "enum": ["tim1-ch4", "tim1-trgo", "tim3-ch2", "tim3-ch4",
                         "tim4-ch1", "tim4-ch2", "tim4-ch3", "tim4-trgo",
                         "tim5-ch4", "tim5-trgo", "tim8-ch2", "tim8-ch3", "tim8-ch4"]}
            properties["injected"]["properties"]["delay_us"] = {
                "type": "integer", "minimum": 1, "maximum": 0xffffffff}
            properties["injected"]["dependentRequired"] = {
                "trigger": ["delay_us"], "delay_us": ["trigger"]}
            properties["injected"]["properties"]["min_interval_us"] = {
                "type": "integer", "minimum": 1, "maximum": 2**32 - 1}
        uart_formats.fifo_editor_schema(properties, fields, record)
        result = {"type": "object", "properties": properties,
                  "required": sorted(required), "additionalProperties": False}
        if "injected" in fields:
            result["allOf"] = [{
                "if": {"properties": {"mode": {"const": "regular-auto-injected"}},
                       "required": ["mode"]},
                "then": {"properties": {"injected": {"properties": {
                    "min_interval_us": False, "trigger": False, "delay_us": False}}}},
                "else": {"properties": {"injected": {"required": ["min_interval_us"]}}}}]
        return result
    schema = record(TOP_REQUIRED | TOP_OPTIONAL | KINDS | set(EXTERNAL_ENDPOINT_KINDS) | {"spi_device", "i2c_device", "usb_endpoint"},
                    TOP_REQUIRED)
    schema["$schema"] = "https://json-schema.org/draft/2020-12/schema"
    schema["title"] = "Nexus authored assembly schema 2"
    properties = schema["properties"]
    properties["schema"] = {"const": 2}
    properties["hardware"] = {"type": "object",
                              "additionalProperties": {"type": "string"}}
    properties["backend"]["enum"] = ["native", "baremetal", "freertos", "threadx"]
    properties["optimization"]["enum"] = ["O0", "Og", "O2", "Os", "O3", "O2-lto", "Os-lto", "O3-lto"]
    properties["power"] = record({"mode", "controllers"}, {"mode", "controllers"})
    properties["power"]["properties"]["mode"] = {"const": "fixed-run"}
    properties["power"]["properties"]["controllers"] = {"type": "array", "minItems": 1, "maxItems": 1, "items": {"type": "string"}, "uniqueItems": True}
    properties["components"] = {"type": "array", "items": {"type": "string"}, "uniqueItems": True}
    properties["abi"] = record({"arch", "fpu", "float_abi"}, {"arch", "fpu", "float_abi"})
    properties["memory"] = record(MEMORY_FIELDS, {"main_stack_bytes"})
    properties["memory"]["properties"]["regions"] = {
        "type": "object", "additionalProperties": record(
            {"limit_bytes"}, {"limit_bytes"})}
    properties["memory"]["properties"]["regions"]["additionalProperties"][
        "properties"]["limit_bytes"] = {"type": "integer", "minimum": 4,
                                        "multipleOf": 4}
    properties["can_domain"] = record({"split", "filters"}, {"split", "filters"})
    filter_fields = can.FILTER_FIELDS
    properties["can_domain"]["properties"]["filters"] = {
        "type": "array", "items": record(filter_fields, filter_fields - {"tag"})}
    properties["can_domain"]["properties"]["filters"]["items"]["properties"][
        "tag"] = {"type": "integer", "minimum": 1, "maximum": 65535}
    properties["os"] = {"anyOf": [kernel.editor_schema(), threadx.editor_schema()]}
    schema["allOf"] = [
        {"if": {"properties": {"backend": {"const": backend}},
                "required": ["backend"]},
         "then": {"properties": {"os": policy}}}
        for backend, policy in (("freertos", kernel.editor_schema()),
                                ("threadx", threadx.editor_schema()))]
    properties["factory"] = {
        "type": "object", "properties": {"capabilities": {"type": "boolean"}},
        "additionalProperties": False}
    from tools.configure.stm32f4.clocks import DEFAULT, AHB, APB
    clock_fields = {name: {"type": "integer", "minimum": 1}
                    for name in DEFAULT}
    clock_fields.update(source={"enum": ["hse", "hsi"]},
                        css={"type": "boolean"})
    clock_fields.update(sysclk_source={"enum": ["pll", "input"],
                                      "default": "pll"},
                        pll48={"type": "boolean", "default": False})
    for name, values in (("pll_p", (2, 4, 6, 8)), ("ahb_div", AHB),
                         ("apb1_div", APB), ("apb2_div", APB),
                         ("adc_div", (2, 4, 6, 8))):
        clock_fields[name] = {"type": "integer", "enum": list(values)}
    auxiliary = record({"m", "n", "sai1", "ltdc"}, {"m", "n"})
    auxiliary["properties"].update(
        m={"type": "integer", "minimum": 2, "maximum": 63},
        n={"type": "integer", "minimum": 50, "maximum": 432})
    for output, divider, posts in (("sai1", "q", range(1, 33)),
                                    ("ltdc", "r", (2, 4, 8, 16))):
        shape = record({divider, "post_divider"}, {divider, "post_divider"})
        shape["properties"][divider] = {
            "type": "integer", "minimum": 2, "maximum": 15 if divider == "q" else 7}
        shape["properties"]["post_divider"] = {"type": "integer", "enum": list(posts)}
        auxiliary["properties"][output] = shape
    auxiliary["anyOf"] = [{"required": ["sai1"]}, {"required": ["ltdc"]}]
    clock_fields["pll_sai"] = auxiliary
    properties["clock_parameters"] = {
        "type": "object", "additionalProperties": False,
        "required": list(DEFAULT), "properties": clock_fields}
    for kind, fields in INSTANCE_FIELDS.items():
        properties[kind] = {"type": "object", "additionalProperties": record(fields, {"binding", "mode"})}
        if kind == "block":
            properties[kind]["additionalProperties"]["properties"]["speed"] = {
                "enum": ["default", "high-speed"]}
        if kind in {"pwm_group", "timer_pulse", "timer_sequence"}:
            shape = properties[kind]["additionalProperties"]["properties"]
            shape["master_trigger"] = {"enum": ["none", "enable", "update", "oc4ref"]}
            shape["synchronization"] = record({"source", "mode"}, {"source", "mode"})
            shape["synchronization"]["properties"]["mode"] = {"enum": ["reset", "gated", "trigger"]}
            for field in {"poll_limit", "burst_budget_hclk_cycles"} & shape.keys():
                shape[field] = {"type": "integer", "minimum": 1}
        if kind in {"capture", "audio"}:
            from tools.configure.stm32f4.capture_audio import editor_schema
            editor_schema(kind, properties[kind]["additionalProperties"])
        if kind == "i2c_target":
            from tools.configure import i2c_target
            i2c_target.editor_schema(properties[kind]["additionalProperties"])
        if kind == "rtc":
            from tools.configure import rtc
            rtc.editor_schema(properties[kind]["additionalProperties"])
        if kind == "lptim":
            timer_advanced.lptim_editor_schema(
                properties[kind]["additionalProperties"])
        if kind == "can_fd":
            can.fd_editor_schema(
                properties[kind]["additionalProperties"], record)
        if kind == "uart":
            uart_formats.controller_editor_schema(
                properties[kind]["additionalProperties"])
        if kind == "i2c":
            i2c_timing_selection.controller_editor_schema(
                properties[kind]["additionalProperties"], record)
        if kind == "dac":
            from tools.configure import dac
            dac.editor_schema(properties[kind]["additionalProperties"][
                "properties"])
        if kind in {"eth", "eth_frames"}:
            ethernet.controller_editor_schema(
                properties[kind]["additionalProperties"])
    for kind, fields in ENDPOINT_FIELDS.items():
        properties[kind + "_device"] = {"type": "object", "additionalProperties": record(fields, {"controller"})}
    for kind in EXTERNAL_ENDPOINT_KINDS:
        required = EXTERNAL_ENDPOINT_REQUIRED[kind]
        fields = required | ({"parameters"} if kind == "memory_window" else set())
        if kind == "flash_async":
            fields |= SERIAL_FLASH_FIELDS
        endpoint = record(fields, required)
        if kind == "flash_async":
            endpoint["required"] = sorted(required & SERIAL_FLASH_FIELDS)
            endpoint["oneOf"] = [
                {"required": sorted(required), "not": {"required": ["mode"]}},
                {"required": sorted(SERIAL_FLASH_FIELDS),
                 "properties": {"mode": {"enum": [
                     "nor-indirect", "nor-indirect-quad-read"]}},
                 "not": {"anyOf": [{"required": [field]} for field in
                                   ("read_timing", "write_timing", "nor")]}}]
        if kind == "mapped_storage":
            endpoint["properties"]["mode"] = {"const": "nor-mapped-cpu"}
        if "ready_delay_us" in fields:
            endpoint["properties"]["ready_delay_us"] = {
                "type": "integer", "minimum": 1, "maximum": 1000000}
        for field in ("parameters", "nor", "geometry"):
            if field in fields:
                endpoint["properties"][field] = {"type": "object"}
        if kind == "memory_window":
            endpoint["required"] = sorted(required - {"read_timing", "write_timing"})
            endpoint["oneOf"] = [
                {"required": ["read_timing", "write_timing"]},
                {"required": ["parameters"],
                 "not": {"anyOf": [{"required": ["read_timing"]},
                                    {"required": ["write_timing"]}]}}]
            endpoint["properties"]["mode"] = {
                "enum": ["cpu-window", "startup-memory", "psram-window",
                         "graphics-owned"]}
        for field in ("read_timing", "write_timing"):
            if field not in fields:
                continue
            endpoint["properties"][field] = {
                "type": "object", "additionalProperties": False,
                "required": ["address_setup", "address_hold", "data_setup", "bus_turn"],
                "properties": {name: {"type": "integer", "minimum": low,
                                      "maximum": high}
                               for name, low, high in (
                                   ("address_setup", 0, 15),
                                   ("address_hold", 1, 15),
                                   ("data_setup", 1, 255),
                                   ("bus_turn", 0, 15))}}
        properties[kind] = {"type": "object", "additionalProperties": endpoint}
    endpoint_schema = record(USB_ENDPOINT_FIELDS, USB_ENDPOINT_FIELDS - {"fifo_words"})
    endpoint_schema["properties"]["direction"] = {"enum": ["in", "out"]}
    endpoint_schema["properties"]["transfer"] = {
        "enum": ["control", "bulk", "interrupt", "isochronous"]}
    properties["usb_endpoint"] = {"type": "object", "additionalProperties": endpoint_schema}
    schema["oneOf"] = [{"required": ["board"], "not": {"required": ["board_package"]}},
                       {"required": ["board_package"], "not": {"required": ["board"]}}]
    explain_schema(schema)
    return schema


def explain_schema(schema):
    """Attach editor help to the same fields without duplicating constraints."""
    descriptions = {
        "window_ticks": ("Minimum WWDG counter decrements before feeding; "
                         "the hardware divider is selected at enable."),
        "binding": "Exact reviewed Board binding; no route fallback.",
        "controller": "Authored controller instance id, not an endpoint id.",
        "mode": ("Explicit maintained mode; the product owns service "
                 "and shutdown."),
        "board": ("Maintained Board directory id; use board_package for "
                  "private PCB data."),
        "board_package": "Explicit Board package path relative to this assembly.",
        "clock": "Reviewed clock profile matching the Board oscillator facts.",
        "baud": ("UART baud in bits per second; the selected provider "
                 "checks its divider."),
        "capacity": ("Exact caller-selected ring capacity; not a hidden "
                     "maximum pool."),
        "priority": "Logical NVIC priority; lower numeric values are more urgent.",
        "calls_os": ("Whether this IRQ may call the selected kernel; "
                     "its ceiling is checked."),
        "reference_mv": ("Declared analog reference in millivolts; "
                         "not a measurement."),
        "resolution_bits": ("ADC raw resolution: 6, 8, 10 or 12 bits; "
                            "omitted selects 12, subject to provider support."),
        "main_stack_bytes": ("Reserved main stack in bytes, not measured "
                             "high-water."),
        "libc_heap_bytes": ("Explicit libc heap reservation in bytes; "
                            "zero disables it."),
        "static_ram_limit_bytes": ("Declared static RAM limit in bytes; "
                                   "checked against linked output."),
        "flash_load_limit_bytes": ("Declared Flash LOAD limit in bytes; "
                                   "not the ELF file size."),
        "image_prefix_bytes": ("Reserved bytes before the executable vectors "
                               "inside the selected physical Flash owner."),
        "image_suffix_bytes": ("Reserved bytes after the executable load window; "
                               "no header or trailer bytes are generated."),
        "max_units_per_second": "Producer-rate bound in payload units per second.",
        "units_per_block": "Payload units in each equally sized stream block.",
        "blocks": "Explicit caller-owned stream block count.",
        "source": "Reviewed source supporting this declared constraint.",
    }

    def visit(value):
        if isinstance(value, list):
            for child in value:
                visit(child)
        elif isinstance(value, dict):
            for name, field in value.get("properties", {}).items():
                if isinstance(field, bool):
                    continue
                if name in descriptions:
                    field["description"] = descriptions[name]
                elif name.endswith("_hz"):
                    field["description"] = (
                        "Frequency in hertz; exact provider limits still apply.")
                elif name.endswith("_us"):
                    field["description"] = (
                        "Time interval in microseconds; "
                        "not a measured hardware guarantee.")
                elif name.endswith("_ns"):
                    field["description"] = "Time interval in nanoseconds."
                elif name.endswith("_ticks"):
                    field["description"] = (
                        "Count in the explicitly selected timer clock ticks.")
                elif name.endswith("_bytes"):
                    field["description"] = "Explicit storage size in bytes."
            for key, child in value.items():
                # Conditions express admission predicates, not editor fields.
                if key != "if":
                    visit(child)

    visit(schema)
