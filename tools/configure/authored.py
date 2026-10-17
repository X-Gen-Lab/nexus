"""Small, strict TOML authoring surface; hardware facts remain JSON."""
import tomllib

INSTANCE_FIELDS = {
    "adc_group": {"binding", "mode", "channels", "sample_times",
                  "reference_mv", "exclusive", "silicon_profile", "trigger",
                  "period_us", "watchdog", "irq"},
    "pwm_adc": {"binding", "mode", "tick_hz", "period_ticks", "trigger_ticks",
                "deadtime_ticks", "channels", "sample_times", "reference_mv",
                "exclusive", "irq"},
    "can_fd": {"binding", "mode", "bitrate", "data_bitrate", "event_capacity",
               "drain_timeout_us", "poll_limit", "filters", "exclusive", "irq"},
    "encoder": {"binding", "mode", "counting", "filter_a", "filter_b", "invert_a", "invert_b", "exclusive"},
    "pwm_group": {"binding", "mode", "tick_hz", "period_ticks", "deadtime_ticks", "exclusive", "irq"},
    "timer_capture": {"binding", "mode", "tick_hz", "edge", "filter", "exclusive", "irq"},
    "advanced_pwm": {"binding", "mode", "tick_hz", "period_ticks", "deadtime_ticks",
                     "break_active_high", "idle_high", "idle_complement_high", "exclusive", "irq"},
    "eth": {"binding", "mode", "tx_count", "rx_count", "rx_storage_bytes",
            "address", "init_timeout_us", "drain_timeout_us", "irq"},
    "usb_device": {"binding", "mode", "event_capacity", "rx_fifo_words",
                   "vbus_sensing", "poll_limit", "drain_timeout_us",
                   "clock_timeout_us", "irq"},
    "can": {"binding", "mode", "operation", "event_capacity", "bitrate",
            "timing", "irq", "drain_timeout_us", "poll_limit", "fifo_order"},
    "gpio": {"binding", "mode"},
    "uart": {"binding", "mode", "baud", "rx", "irq", "frame", "stream",
             "fifo"},
    "spi": {"binding", "mode", "max_hz", "irq"},
    "i2c": {"binding", "mode", "max_hz", "irq", "timing"},
    "flash": {"binding", "mode"},
    "watchdog": {"binding", "mode"},
    "exti": {"binding", "mode", "edge", "event_capacity", "irq"},
    "pwm": {"binding", "mode", "period_ticks", "duty_ticks", "tick_hz"},
    "adc": {"binding", "mode", "channels", "sample_times", "reference_mv", "timeout_ms", "irq", "stream"},
}
INSTANCE_FIELDS["eth_frames"] = set(INSTANCE_FIELDS["eth"])
KINDS = set(INSTANCE_FIELDS)
MEMORY_FIELDS = {"main_stack_bytes", "libc_heap_bytes", "static_ram_limit_bytes",
                 "flash_load_limit_bytes", "regions"}
ENDPOINT_FIELDS = {"spi": {"controller", "driver", "cs_binding", "mode", "max_hz", "model_bytes"},
                   "i2c": {"controller", "driver", "address", "model_bytes"}}
USB_ENDPOINT_FIELDS = {"controller", "number", "direction", "transfer",
                       "max_packet", "fifo_words"}
TOP_REQUIRED = {"schema", "backend", "clock", "memory"}
TOP_OPTIONAL = {"board", "board_package", "optimization", "components", "layout", "abi", "os", "factory", "can_domain"}



def normalize(data, path, root, fail, obj, integer, text):
    obj(data, TOP_REQUIRED, TOP_OPTIONAL | KINDS | {"spi_device", "i2c_device", "usb_endpoint"},
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
                    {"frame", "stream", "fifo"}, f"uart.{name}")
                rx = obj(selection.pop("rx"), {"format"}, {"capacity"}, f"uart.{name}.rx")
                irq = obj(selection.pop("irq"), {"priority"}, {"calls_os", "probe"}, f"uart.{name}.irq")
                selection.update(rx_profile=rx["format"], irq_priority=irq["priority"])
                if "capacity" in rx:
                    selection["rx_capacity"] = rx["capacity"]
                if "calls_os" in irq:
                    selection["calls_os"] = irq["calls_os"]
                if "probe" in irq:
                    selection["irq_probe"] = irq["probe"]
                if selection["mode"] == "irq":
                    selection["mode"] = "irq-byte-event"
            elif kind in {"exti", "spi", "adc", "adc_group", "i2c", "can", "can_fd", "pwm_adc", "usb_device", "eth", "eth_frames", "timer_capture", "advanced_pwm", "pwm_group"} and "irq" in selection:
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
    endpoints = data.get("usb_endpoint", {})
    if not isinstance(endpoints, dict):
        fail("USB endpoints: expected endpoint tables")
    for name, definition in endpoints.items():
        obj(definition, USB_ENDPOINT_FIELDS - {"fifo_words"}, {"fifo_words"},
            "USB endpoint " + name)
        devices.append({**definition, "id": name, "driver": "usb-fs-endpoint",
                        "authored_kind": "usb_device"})
    result = {"schema_version": 1, "board_package": package,
              "backend": data["backend"], "clock_profile": data["clock"],
              "controllers": controllers, "devices": devices,
              "memory_budgets": budgets, "layout": data.get("layout")}
    for field in ("abi", "optimization", "components", "os", "factory", "can_domain"):
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
        from . import kernel, threadx, uart_formats
    except ImportError:
        import kernel
        import threadx
        import uart_formats

    def record(fields, required=()):
        properties = {name: {"type": "string"} for name in sorted(fields)}
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
                   "upper"}
        for name in fields & numeric:
            properties[name] = {"type": "integer", "minimum": 0}
        for name in fields & {"channels", "sample_times"}:
            properties[name] = {"type": "array", "items": {"type": "integer", "minimum": 0}}
        for name in fields & {"fifo_order", "remote", "match_remote", "vbus_sensing",
                             "break_active_high", "idle_high", "idle_complement_high", "exclusive", "invert_a", "invert_b"}:
            properties[name] = {"type": "boolean"}
        if "timing" in fields:
            properties["timing"] = record(
                {"prescaler", "segment1", "segment2", "sjw"},
                {"prescaler", "segment1", "segment2", "sjw"})
        if "mode" in fields and "controller" in fields:
            properties["mode"] = {"type": "integer", "minimum": 0, "maximum": 3}
        if "irq" in fields:
            properties["irq"] = record({"priority", "calls_os"}, {"priority"})
            properties["irq"]["properties"]["calls_os"] = {"type": "boolean"}
        if "rx" in fields:
            properties["rx"] = record({"format", "capacity"}, {"format"})
            properties["rx"]["properties"]["format"]["enum"] = ["bytes", "events", "blocks", "words"]
        if "frame" in fields:
            properties["frame"] = record(
                {"data_bits", "parity", "stop_bits", "flow"},
                {"data_bits", "parity", "stop_bits", "flow"})
            frame = properties["frame"]["properties"]
            frame["data_bits"] = {"enum": [7, 8, 9]}
            frame["stop_bits"] = {"enum": [1, 2]}
            frame["parity"] = {"enum": ["none", "even", "odd"]}
            frame["flow"] = {"const": "none"}
        if "stream" in fields:
            stream_fields = {"max_units_per_second", "units_per_block",
                             "blocks", "release_delay_us", "target_delay_us",
                             "on_exhaustion", "source"}
            stream = record(stream_fields, stream_fields)
            for name in stream_fields - {"on_exhaustion", "source"}:
                stream["properties"][name] = {"type": "integer", "minimum": 0}
            stream["properties"]["on_exhaustion"] = {
                "const": "stop-and-report-gap"}
            properties["stream"] = stream
        if "watchdog" in fields:
            properties["watchdog"] = record({"lower", "upper"},
                                             {"lower", "upper"})
        if "fifo" in fields:
            names = {"rx_threshold", "tx_threshold", "rx_budget_bytes",
                     "tx_budget_bytes"}
            properties["fifo"] = record(names, names)
            for name in ("rx_budget_bytes", "tx_budget_bytes"):
                properties["fifo"]["properties"][name] = {
                    "type": "integer", "minimum": 1, "maximum": 16}
        return {"type": "object", "properties": properties,
                "required": sorted(required), "additionalProperties": False}
    schema = record(TOP_REQUIRED | TOP_OPTIONAL | KINDS | {"spi_device", "i2c_device", "usb_endpoint"},
                    TOP_REQUIRED)
    schema["$schema"] = "https://json-schema.org/draft/2020-12/schema"
    schema["title"] = "Nexus authored assembly schema 2"
    properties = schema["properties"]
    properties["schema"] = {"const": 2}
    properties["backend"]["enum"] = ["native", "baremetal", "freertos", "threadx"]
    properties["optimization"]["enum"] = ["O0", "Og", "O2", "Os", "O3", "O2-lto", "Os-lto", "O3-lto"]
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
    filter_fields = {"controller", "bank", "id", "mask", "format", "fifo",
                     "remote", "match_remote"}
    properties["can_domain"]["properties"]["filters"] = {
        "type": "array", "items": record(filter_fields, filter_fields)}
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
    for kind, fields in INSTANCE_FIELDS.items():
        properties[kind] = {"type": "object", "additionalProperties": record(fields, {"binding", "mode"})}
        if kind == "can_fd":
            filter_fields = {"id", "mask", "format", "fifo"}
            filters = record(filter_fields, filter_fields)
            filters["properties"]["format"] = {"enum": ["standard", "extended"]}
            filters["properties"]["fifo"] = {"enum": [0, 1]}
            properties[kind]["additionalProperties"]["properties"]["filters"] = {
                "type": "array", "items": filters, "maxItems": 36}
        if kind == "uart":
            uart = properties[kind]["additionalProperties"]
            irq = uart["properties"]["irq"]
            irq["properties"]["probe"] = {"type": "boolean"}
            uart["properties"]["frame"]["properties"]["flow"] = {"type": "string"}
            uart["allOf"] = [{
                "if": {"properties": {"mode": {"const": uart_formats.RTSCTS_MODE}},
                       "required": ["mode"]},
                "then": {"properties": {
                    "frame": {"const": dict(uart_formats.RTSCTS_FRAME)}}},
                "else": {"properties": {
                    "frame": {"properties": {"flow": {"const": "none"}}}}}}]
        if kind == "i2c":
            timing_fields = {"kernel_hz_min", "kernel_hz_max", "pclk_hz_min",
                             "bus_hz_min", "rise_ns_max", "fall_ns_max", "source"}
            timing = record(timing_fields, timing_fields)
            for name in timing_fields - {"source"}:
                timing["properties"][name] = {"type": "integer", "minimum": 0}
            properties[kind]["additionalProperties"]["properties"]["timing"] = timing
        if kind in {"eth", "eth_frames"}:
            properties[kind]["additionalProperties"]["properties"]["address"] = {
                "type": "array", "minItems": 6, "maxItems": 6,
                "items": {"type": "integer", "minimum": 0, "maximum": 255}}
    for kind, fields in ENDPOINT_FIELDS.items():
        properties[kind + "_device"] = {"type": "object", "additionalProperties": record(fields, {"controller"})}
    endpoint_schema = record(USB_ENDPOINT_FIELDS, USB_ENDPOINT_FIELDS - {"fifo_words"})
    endpoint_schema["properties"]["direction"] = {"enum": ["in", "out"]}
    endpoint_schema["properties"]["transfer"] = {"enum": ["control", "bulk"]}
    properties["usb_endpoint"] = {"type": "object", "additionalProperties": endpoint_schema}
    schema["oneOf"] = [{"required": ["board"], "not": {"required": ["board_package"]}},
                       {"required": ["board_package"], "not": {"required": ["board"]}}]
    return schema
