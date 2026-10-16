"""Generate pure, typed static lookup without registration or construction."""

try:
    from .providers.common import mode_traits, fail
    from .providers import maintained
    from .ir import ConfigurationIR
except ImportError:
    from providers.common import mode_traits, fail
    from providers import maintained
    from ir import ConfigurationIR

FACTORY_ABI_EPOCH = 1

KINDS = ("gpio", "uart", "spi", "i2c", "flash", "watchdog", "exti", "pwm", "adc", "can", "usb_device", "eth", "timer_capture", "advanced_pwm")


def symbol(name):
    return name.replace("-", "_")


def interface_type(class_name, shape):
    """Use actual public types where controller/direction names differ."""
    if class_name in {"timer_capture", "advanced_pwm"}:
        return f"nx_{class_name}_t"
    if class_name in {"eth", "mdio"}:
        return f"nx_{class_name}_port_t"
    if class_name == "usb_device":
        return "nx_usb_device_port_t"
    if class_name == "usb_endpoint":
        return "nx_usb_endpoint_port_t"
    return f"nx_{class_name.removesuffix('_device')}_{shape}_t"


def public_header(kind):
    return {"pwm": "timer", "eth": "ethernet"}.get(kind, kind)


def _typed(result):
    return (result if isinstance(result, ConfigurationIR) else
            ConfigurationIR.from_validated(result))


def classes(result):
    """Keep controller and endpoint namespaces independent in every view."""
    result = _typed(result)
    records = [(kind, "port", [item for item in result.controllers
                              if item.kind == kind]) for kind in KINDS]
    records += [(kind + "_device", "endpoint", [item for item in result.devices
                  if item.kind == kind]) for kind in ("spi", "i2c")]
    records.append(("usb_endpoint", "endpoint", [item for item in result.devices
                    if item.kind == "usb_device"]))
    records.append(("mdio", "port", [item for item in result.controllers
                                    if item.kind == "eth"]))
    return records


def emit(result):
    result = _typed(result)
    header = ['/**', ' * \\file            nexus_factory.h',
              ' * \\brief           Typed lookup of statically selected instances.',
              ' * \\author          Nexus Team', ' */',
              '#ifndef NEXUS_FACTORY_H', '#define NEXUS_FACTORY_H']
    for kind in KINDS:
        header.append(f'#include "nexus/io/{public_header(kind)}.h"')
    header.append('#include "nexus/io/mdio.h"')
    header += ['#ifdef __cplusplus', 'extern "C" {', '#endif']
    source = []
    for class_name, shape, instances in classes(result):
        prefix = "NX_" + class_name.upper() + "_ID_"
        return_type = "const " + interface_type(class_name, shape) + "*"
        argument = f"nx_{class_name}_id_t id"
        header += [f"typedef uint16_t nx_{class_name}_id_t;", "enum {"]
        for index, instance in enumerate(instances):
            header.append(f'    {prefix}{symbol(instance.id).upper()} = {index},')
        header += [f"    {prefix}COUNT = {len(instances)}", "};",
                   '/**',
                   ' * \\brief           Return static identity without initialization.',
                   ' *',
                   ' * \\param[in]       id: Generated assembly-local identity.',
                   ' *',
                   ' * \\return          Fixed borrowed face, or NULL for an invalid ID.',
                   ' *',
                   ' * \\note            Task/ISR safe; no allocation, locks or hardware',
                   ' *                  access. Identity exists before startup. Operations',
                   ' *                  require successful startup and quiescent shutdown.',
                   ' */', f"{return_type} nx_factory_{class_name}({argument});"]
        source += ['/** \\brief Pure typed lookup; lifecycle remains explicit. */',
                   f"{return_type} nx_factory_{class_name}({argument}) {{",
                   "    switch (id) {"]
        for instance in instances:
            name = symbol(instance.id)
            pointer = (f"nx_device_{name}" if shape == "endpoint"
                       else f"nx_binding_{name}_mdio" if class_name == "mdio"
                       else f"nx_binding_{name}")
            source += [f"        case {prefix}{name.upper()}:", f"            return {pointer};"]
        source += ["        default:", "            return NULL;", "    }", "}"]
    if result.factory.capabilities:
        cap_header, cap_source = emit_capabilities(result)
        header.extend(cap_header)
        source.extend(cap_source)
    header += ['#ifdef __cplusplus', '}', '#endif', '#endif', '']
    return "\n".join(header), "\n".join(source) + "\n"


def emit_strong(result):
    """Opt-in nominal IDs; wrappers own no storage or hardware effects."""
    header = ["/**", " * \\file            nexus_factory_strong.h",
              " * \\brief           Optional nominal factory identities.",
              " * \\author          Nexus Team", " */",
              "#ifndef NEXUS_FACTORY_STRONG_H", "#define NEXUS_FACTORY_STRONG_H",
              '#include "nexus_factory.h"', "#ifdef __cplusplus",
              "#define NX_FACTORY_REF_VALUE(type, value)".ljust(79) + "\\",
              "    type {".ljust(79) + "\\",
              "        value".ljust(79) + "\\",
              "    }",
              'extern "C" {', "#else",
              "#define NX_FACTORY_REF_VALUE(type, value) ((type){value})",
              "#endif"]
    for class_name, shape, instances in classes(result):
        upper = class_name.upper()
        ref = f"nx_{class_name}_ref_t"
        header += ["/** \\brief Explicit assembly-local ID; not a wire ABI. */",
                   "typedef struct {", "    uint16_t value;", f"}} {ref};"]
        macros = []
        for name in [*(symbol(instance.id).upper() for instance in instances),
                     "COUNT"]:
            key = f"#define NX_{upper}_REF_{name}"
            value = f"NX_FACTORY_REF_VALUE({ref}, NX_{upper}_ID_{name})"
            macros.append((key, value))
        width = max(len(key) for key, _ in macros)
        for key, value in macros:
            if width + 1 + len(value) <= 80:
                header.append(key.ljust(width) + " " + value)
            else:
                header += [key.ljust(79) + "\\", "    " + value]
        return_type = "static inline const " + interface_type(class_name, shape) + "*"
        declaration = f"nx_factory_{class_name}_ref({ref} id) {{"
        signature = ([return_type, declaration]
                     if len(return_type + " " + declaration) > 80 else
                     [return_type + " " + declaration])
        header += ["/** \\brief Pure getter; lifecycle remains explicit. */",
                   *signature,
                   f"    return nx_factory_{class_name}(id.value);", "}"]
    header += ["#ifdef __cplusplus", "}", "#endif", "#endif", ""]
    return "\n".join(header)


def _capability(item, model, endpoint=None):
    """Describe maintained execution traits, never infer silicon capacity."""
    kind, mode = item.kind, item.mode
    traits = mode_traits(kind, mode)
    flags = ["NX_FACTORY_CAP_MODEL"] if model else []
    if traits.polling:
        flags.append("NX_FACTORY_CAP_POLLING")
    if not model:
        if traits.irq:
            flags.append("NX_FACTORY_CAP_IRQ")
        if traits.tx_dma:
            flags.append("NX_FACTORY_CAP_TX_DMA")
        if traits.rx_dma:
            flags.append("NX_FACTORY_CAP_RX_DMA")
        if traits.bus_master:
            flags.append("NX_FACTORY_CAP_BUS_MASTER_DMA")
    if traits.block_stream:
        flags.append("NX_FACTORY_CAP_BLOCK_STREAM")
        if traits.dma and not traits.continuous:
            flags.append("NX_FACTORY_CAP_STREAM_GAPS")
    units, unit_bytes, alignment, messages = 0, 0, 1, 0
    if kind == "uart":
        unit_bytes = 1
        if traits.dma:
            units = 65535
    elif kind == "spi":
        unit_bytes = 1
        if traits.dma:
            units = 65535
        elif not model:
            units = 256
    elif kind == "i2c":
        unit_bytes = 1
        if not model:
            units = "NX_I2C_MAX_MESSAGE_BYTES"
            messages = "NX_I2C_MAX_MESSAGES"
    elif kind == "adc":
        unit_bytes, alignment = 2, 2
        count = len(item.options.channels)
        units = 65535 - 65535 % count if traits.dma else count
    elif kind == "can":
        # Each submitted request borrows one frame, not all TX mailboxes.
        units, unit_bytes, messages = 8, 1, 1
    elif kind == "eth":
        units, unit_bytes, alignment, messages = "NX_ETH_FRAME_MAX", 1, 4, 1
    elif kind == "usb_device":
        units = endpoint.options.max_packet if endpoint is not None else 64
        unit_bytes, messages = 1, 1
    elif kind == "flash":
        if model:
            unit_bytes = alignment = 1
        else:
            provider = maintained(item.provider)
            unit_bytes = getattr(provider, "FLASH_PROGRAM_UNIT", None)
            alignment = getattr(provider, "FLASH_INPUT_ALIGNMENT", None)
            if any(type(value) is not int or not 1 <= value <= 65535 or
                   value & (value - 1) for value in (unit_bytes, alignment)):
                fail("Flash provider requires exact power-of-two units/alignment")
    return " | ".join(flags) or "0U", units, unit_bytes, alignment, messages


def emit_capabilities(result):
    """Optional selected-only readonly metadata stays outside the I/O face."""
    result = _typed(result)
    header = [
        '/** \\brief Maintained execution traits; no physical qualification. */',
        'enum {', '    NX_FACTORY_CAP_MODEL = 1U << 0,',
        '    NX_FACTORY_CAP_POLLING = 1U << 1,',
        '    NX_FACTORY_CAP_IRQ = 1U << 2,',
        '    NX_FACTORY_CAP_TX_DMA = 1U << 3,',
        '    NX_FACTORY_CAP_RX_DMA = 1U << 4,',
        '    NX_FACTORY_CAP_BLOCK_STREAM = 1U << 5,',
        '    NX_FACTORY_CAP_STREAM_GAPS = 1U << 6,',
        '    NX_FACTORY_CAP_BUS_MASTER_DMA = 1U << 7', '};',
        '/**',
        ' * \\brief Readonly bounds of the selected provider/mode.',
        ' * \\note Zero units/messages means no bound described, not unlimited.',
        ' * For I2C, units bounds each message; unit_bytes is the payload unit.',
        ' * For Flash, unit_bytes constrains destination offset and length;',
        ' * buffer_alignment describes caller input, independently of that unit.',
        ' * Board wiring, memory domains and deadlines remain separate contracts.',
        ' */', 'typedef struct {', '    uint32_t features;',
        '    uint32_t max_transfer_units;', '    uint16_t unit_bytes;',
        '    uint16_t buffer_alignment;', '    uint16_t max_messages;',
        '} nx_factory_io_capability_t;']
    source = []
    for class_name, shape, instances in classes(result):
        for instance in instances:
            item = (next(port for port in result.controllers
                         if port.id == instance.controller)
                    if shape == "endpoint" else instance)
            flags, units, size, alignment, messages = _capability(
                item, result.soc_family == "native",
                instance if class_name == "usb_endpoint" else None)
            if class_name == "mdio":
                flags, units, size, alignment, messages = "NX_FACTORY_CAP_POLLING", 1, 2, 1, 1
            # Public C constants are the bounds authority; never recopy values.
            units = f"{units}U" if type(units) is int else units
            messages = f"{messages}U" if type(messages) is int else messages
            name = symbol(instance.id)
            source += [f"static const nx_factory_io_capability_t "
                       f"s_nx_capability_{class_name}_{name} = {{",
                       f"    {flags}, {units}, {size}U, {alignment}U, {messages}",
                       "};"]
        function = f"nx_factory_{class_name}_capability"
        header += ['/** \\brief Pure lookup; NULL for an invalid assembly ID. */',
                   f"const nx_factory_io_capability_t* {function}(",
                   f"    nx_{class_name}_id_t id);"]
        source += ['/** \\brief Pure selected-only capability lookup. */',
                   f"const nx_factory_io_capability_t* {function}(",
                   f"    nx_{class_name}_id_t id) {{", "    switch (id) {"]
        for instance in instances:
            name = symbol(instance.id)
            source += [f"        case NX_{class_name.upper()}_ID_{name.upper()}:",
                       f"            return &s_nx_capability_{class_name}_{name};"]
        source += ["        default:", "            return NULL;", "    }", "}"]
    return header, source
