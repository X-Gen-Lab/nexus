"""Write mutable test intentions through the sole production TOML boundary.

This helper is not part of the configuration input closure or installed SDK.
Kinds are explicit test facts, never inferred from a chip or binding name.
Unknown and invalid values are retained for production rejection tests.
"""
import json


def _value(value):
    if isinstance(value, str):
        return json.dumps(value, ensure_ascii=False)
    if type(value) is bool:
        return "true" if value else "false"
    if type(value) in (int, float):
        return str(value)
    if isinstance(value, (list, tuple)):
        return "[" + ", ".join(_value(item) for item in value) + "]"
    if isinstance(value, dict):
        return "{ " + ", ".join(
            json.dumps(key) + " = " + _value(item)
            for key, item in value.items()) + " }"
    raise TypeError(f"TOML fixture cannot represent {type(value).__name__}")


def render_toml(document, *, kinds=None):
    """Project detached test intentions to schema-2 source without validation."""
    kinds = {} if kinds is None else kinds
    renamed = {"schema_version": "schema", "clock_profile": "clock",
               "memory_budgets": "memory"}
    memory_names = {"static_ram_bytes": "static_ram_limit_bytes",
                    "flash_load_bytes": "flash_load_limit_bytes"}
    result = {"schema": 2}
    for name, value in document.items():
        if name in {"controllers", "devices"} or (
                name == "layout" and value is None):
            continue
        if name == "schema_version" and type(value) is int and value == 1:
            value = 2
        if name == "memory_budgets" and isinstance(value, dict):
            value = {memory_names.get(key, key): item
                     for key, item in value.items()}
        result[renamed.get(name, name)] = value
    lines = [json.dumps(name) + " = " + _value(value)
             for name, value in result.items()]
    controller_kinds = {}
    for original in document.get("controllers", ()):
        item = dict(original)
        identity = item.pop("id")
        kind = item.pop("authored_kind", None)
        if kind is None:
            kind = kinds[item["binding"]]
        controller_kinds[identity] = kind
        if kind == "uart":
            rx = {}
            for source, target in (("rx_profile", "format"),
                                   ("rx_capacity", "capacity")):
                if source in item:
                    rx[target] = item.pop(source)
            item["rx"] = rx
        irq = {}
        for source, target in (("irq_priority", "priority"),
                               ("calls_os", "calls_os"),
                               ("irq_probe", "probe")):
            if source in item:
                irq[target] = item.pop(source)
        if irq or kind == "uart":
            item["irq"] = irq
        lines.extend(("", "[" + json.dumps(kind) + "." +
                      json.dumps(identity) + "]"))
        lines.extend(json.dumps(name) + " = " + _value(value)
                     for name, value in item.items())
    for original in document.get("devices", ()):
        item = dict(original)
        identity = item.pop("id")
        kind = item.pop("authored_kind", None)
        if kind is None:
            kind = controller_kinds[item["controller"]]
        if kind == "usb_device":
            section = "usb_endpoint"
            item.pop("driver", None)
        else:
            section = kind + "_device"
        lines.extend(("", "[" + json.dumps(section) + "." +
                      json.dumps(identity) + "]"))
        lines.extend(json.dumps(name) + " = " + _value(value)
                     for name, value in item.items())
    return "\n".join(lines) + "\n"
