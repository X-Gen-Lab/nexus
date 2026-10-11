"""Small, strict TOML authoring surface; hardware facts remain JSON."""
import tomllib

INSTANCE_FIELDS = {
    "gpio": {"binding", "mode"},
    "uart": {"binding", "mode", "baud", "rx", "irq"},
    "spi": {"binding", "mode", "max_hz", "irq"},
    "i2c": {"binding", "mode", "max_hz"},
    "flash": {"binding", "mode"},
    "watchdog": {"binding", "mode"},
    "exti": {"binding", "mode", "edge", "event_capacity", "irq"},
    "pwm": {"binding", "mode", "period_ticks", "duty_ticks", "tick_hz"},
    "adc": {"binding", "mode", "channels", "sample_times", "reference_mv", "timeout_ms", "irq"},
}
KINDS = set(INSTANCE_FIELDS)
MEMORY_FIELDS = {"main_stack_bytes", "libc_heap_bytes", "static_ram_limit_bytes",
                 "flash_load_limit_bytes"}
ENDPOINT_FIELDS = {"spi": {"controller", "driver", "cs_binding", "mode", "max_hz", "model_bytes"},
                   "i2c": {"controller", "driver", "address", "model_bytes"}}
TOP_REQUIRED = {"schema", "backend", "clock", "memory"}
TOP_OPTIONAL = {"board", "board_package", "optimization", "components", "layout", "abi", "os"}



def normalize(data, path, root, fail, obj, integer, text):
    obj(data, TOP_REQUIRED, TOP_OPTIONAL | KINDS | {"spi_device", "i2c_device"},
        "assembly TOML")
    integer(data["schema"], 2, 2, "assembly schema")
    if ("board" in data) == ("board_package" in data):
        fail("Select exactly one Board id or explicit board_package")
    if "board" in data:
        name = text(data["board"], "Board id")
        if "/" in name or "\\" in name or name in {".", ".."}:
            fail("Board id cannot be a path; use explicit board_package")
        package = root / "boards" / name
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
                obj(selection, {"id", "authored_kind", "binding", "mode", "baud", "rx", "irq"},
                    (), f"uart.{name}")
                rx = obj(selection.pop("rx"), {"format"}, {"capacity"}, f"uart.{name}.rx")
                irq = obj(selection.pop("irq"), {"priority"}, {"calls_os"}, f"uart.{name}.irq")
                selection.update(rx_profile=rx["format"], irq_priority=irq["priority"])
                if "capacity" in rx:
                    selection["rx_capacity"] = rx["capacity"]
                if "calls_os" in irq:
                    selection["calls_os"] = irq["calls_os"]
                if selection["mode"] == "irq":
                    selection["mode"] = "irq-byte-event"
            elif kind in {"exti", "spi", "adc"} and "irq" in selection:
                irq = obj(selection.pop("irq"), {"priority"}, {"calls_os"}, f"{kind}.{name}.irq")
                selection["irq_priority"] = irq["priority"]
                if "calls_os" in irq:
                    selection["calls_os"] = irq["calls_os"]
            controllers.append(selection)
    devices = []
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
    result = {"schema_version": 1, "board_package": package,
              "backend": data["backend"], "clock_profile": data["clock"],
              "controllers": controllers, "devices": devices,
              "memory_budgets": budgets, "layout": data.get("layout")}
    for field in ("abi", "optimization", "components", "os"):
        if field in data:
            result[field] = data[field]
    return result


def read(path, snapshots, root, fail, obj, integer, text):
    if path.is_symlink() or not path.is_file():
        fail(f"Assembly input must be a regular file: {path}")
    contents = path.read_bytes()
    snapshots[path] = contents
    try:
        data = tomllib.loads(contents.decode("utf-8"))
    except (tomllib.TOMLDecodeError, UnicodeDecodeError) as error:
        fail(f"Invalid TOML: {error}")
    return normalize(data, path, root, fail, obj, integer, text)


def editor_schema():
    """Expose the same authoring fields for editor completion, not resolution."""
    try:
        from . import kernel
    except ImportError:
        import kernel

    def record(fields, required=()):
        properties = {name: {"type": "string"} for name in sorted(fields)}
        numeric = {"baud", "capacity", "priority", "main_stack_bytes", "libc_heap_bytes",
                   "static_ram_limit_bytes", "flash_load_limit_bytes", "max_hz",
                   "event_capacity", "period_ticks", "duty_ticks", "tick_hz",
                   "reference_mv", "timeout_ms", "address", "model_bytes"}
        for name in fields & numeric:
            properties[name] = {"type": "integer", "minimum": 0}
        for name in fields & {"channels", "sample_times"}:
            properties[name] = {"type": "array", "items": {"type": "integer", "minimum": 0}}
        if "mode" in fields and "controller" in fields:
            properties["mode"] = {"type": "integer", "minimum": 0, "maximum": 3}
        if "irq" in fields:
            properties["irq"] = record({"priority", "calls_os"}, {"priority"})
            properties["irq"]["properties"]["calls_os"] = {"type": "boolean"}
        if "rx" in fields:
            properties["rx"] = record({"format", "capacity"}, {"format"})
            properties["rx"]["properties"]["format"]["enum"] = ["bytes", "events", "blocks"]
        return {"type": "object", "properties": properties,
                "required": sorted(required), "additionalProperties": False}
    schema = record(TOP_REQUIRED | TOP_OPTIONAL | KINDS | {"spi_device", "i2c_device"},
                    TOP_REQUIRED)
    schema["$schema"] = "https://json-schema.org/draft/2020-12/schema"
    schema["title"] = "Nexus authored assembly schema 2"
    properties = schema["properties"]
    properties["schema"] = {"const": 2}
    properties["backend"]["enum"] = ["native", "baremetal", "freertos"]
    properties["optimization"]["enum"] = ["O2", "Os", "O3", "O2-lto", "Os-lto", "O3-lto"]
    properties["components"] = {"type": "array", "items": {"type": "string"}, "uniqueItems": True}
    properties["abi"] = record({"arch", "fpu", "float_abi"}, {"arch", "fpu", "float_abi"})
    properties["memory"] = record(MEMORY_FIELDS, {"main_stack_bytes"})
    properties["os"] = kernel.editor_schema()
    for kind, fields in INSTANCE_FIELDS.items():
        properties[kind] = {"type": "object", "additionalProperties": record(fields, {"binding", "mode"})}
    for kind, fields in ENDPOINT_FIELDS.items():
        properties[kind + "_device"] = {"type": "object", "additionalProperties": record(fields, {"controller"})}
    schema["oneOf"] = [{"required": ["board"], "not": {"required": ["board_package"]}},
                       {"required": ["board_package"], "not": {"required": ["board"]}}]
    return schema
