"""Generate pure, typed static lookup without registration or construction."""

from dataclasses import dataclass
import json

from tools.configure.ir import ConfigurationIR
from tools.configure.contracts import CONTROLLER_CONTRACTS, CONTROLLER_KINDS
from tools.configure import io_traits, selected_contracts
from tools.configure.providers import for_selection
from tools.configure import enhanced_endpoints
from tools.configure import stream_profiles, uart_formats, usb_profiles
from tools.configure.calls import Symbol
from tools.configure import usb_otg

FACTORY_ABI_EPOCH = 2

KINDS = CONTROLLER_KINDS


def symbol(name):
    return name.replace("-", "_")


def controller_ordinal_identifier(kind, identifier):
    """Name a diagnostic ordinal, never a typed factory or wire identity."""
    return ("NX_PLATFORM_CONTROLLER_INDEX_" + symbol(kind).upper() + "_" +
            symbol(identifier).upper())


def controller_ordinals(result):
    """Project the single resolved controller order without a second registry."""
    return tuple((index, item,
                  controller_ordinal_identifier(item.kind, item.id))
                 for index, item in enumerate(_typed(result).controllers))


def interface_type(class_name, shape):
    """Use actual public types where controller/direction names differ."""
    if class_name in {"timer_capture", "advanced_pwm", "pwm_group", "encoder", "pwm_adc"}:
        return f"nx_{class_name}_t"
    if class_name in {"eth", "mdio", "memory_window", "flash_async", "nand"}:
        return f"nx_{class_name}_port_t"
    if class_name in {"usb_device", "usb_host"}:
        return f"nx_{class_name}_port_t"
    if class_name == "usb_endpoint":
        return "nx_usb_endpoint_port_t"
    return f"nx_{class_name.removesuffix('_device')}_{shape}_t"


def public_header(kind):
    if kind in CONTROLLER_CONTRACTS:
        return CONTROLLER_CONTRACTS[kind].header
    return {"spi_device": "spi", "i2c_device": "i2c",
            "usb_endpoint": "usb_device",
            "spi_transaction_device": "spi_transaction",
            "i2c_long_device": "i2c_long",
            "spi_dma_transaction_device": "spi_dma_transaction"}.get(kind, kind)


def _typed(result):
    if not isinstance(result, ConfigurationIR):
        raise TypeError("Factory emission requires ConfigurationIR")
    return result


def classes(result):
    """Keep controller and endpoint namespaces independent in every view."""
    result = _typed(result)
    records = [(kind, "port", [item for item in result.controllers
                              if controller_class(item) == kind])
               for kind in KINDS]
    for kind in ("uart_word", "spi_dma_transaction", "spi_frames", "pin_state"):
        instances = [item for item in result.controllers
                     if controller_class(item) == kind]
        if instances:
            records.append((kind, "port", instances))
    records += [(kind + "_device", "endpoint", [item for item in result.devices
                  if enhanced_endpoints.class_name(item) == kind + "_device"])
                for kind in ("spi", "i2c")]
    for name in ("spi_transaction_device", "i2c_long_device",
                 "spi_dma_transaction_device"):
        instances = [item for item in result.devices
                     if enhanced_endpoints.class_name(item) == name]
        if instances:
            records.append((name, "endpoint", instances))
    records.append(("usb_endpoint", "endpoint", [item for item in result.devices
                    if item.kind == "usb_device"]))
    for name in ("memory_window", "parallel16", "flash_async", "nand"):
        instances = [item for item in result.devices
                     if selected_contracts.endpoint_class(item) == name]
        if instances:
            records.append((name, "endpoint", instances))
    records.append(("mdio", "port", [item for item in result.controllers
                                    if item.kind in {"eth", "eth_frames"}]))
    return records


def controller_class(item):
    """Physical UART resource and nominal word face remain distinct facts."""
    return selected_contracts.controller_class(item)


def _uart_instances(result):
    """Nominal byte UART IDs never substitute for word or endpoint IDs."""
    return tuple(item for item in result.controllers
                 if controller_class(item) == "uart")


def _dac_tx_factory(result):
    """An optional face reuses the controller's ID, state and shared methods."""
    instances = [item for item in result.controllers if item.kind == "dac"]
    if not instances:
        return [], []
    header = ['#include "nexus/io/dac_tx.h"',
        r'/** \brief Pure same-controller lookup; NULL for CPU modes or bad IDs. */',
        'const nx_dac_tx_port_t* nx_factory_dac_tx(nx_dac_id_t id);']
    source = [r'/** \brief Return an existing transfer face without side effects. */',
        'const nx_dac_tx_port_t* nx_factory_dac_tx(nx_dac_id_t id) {',
        '    switch (id) {']
    for item in instances:
        if for_selection(item).mode_implementation(item.kind, item.mode) == "dac_dma":
            name = symbol(item.id)
            source += [f'        case NX_DAC_ID_{name.upper()}:',
                       f'            return &s_nx_dac_tx_{name};']
    source += ['        default:', '            return NULL;', '    }', '}']
    return header, source


def _flash_maintenance_factory(result):
    """An explicit optional face shares the ordinary physical controller ID."""
    instances = [item for item in result.controllers if item.kind == "flash"]
    if not instances:
        return [], []
    header = ['#include "nexus/io/flash_maintenance.h"',
        r'/** \brief Pure optional maintenance lookup; never authorizes pulses. */',
        'const nx_flash_maintenance_port_t*',
        'nx_factory_flash_maintenance(nx_flash_id_t id);']
    source = [r'/** \brief Return an existing selected face without effects. */',
        'const nx_flash_maintenance_port_t*',
        'nx_factory_flash_maintenance(nx_flash_id_t id) {',
        '    switch (id) {']
    for item in instances:
        if getattr(item.options, "maintenance", False):
            name = symbol(item.id)
            source += [f'        case NX_FLASH_ID_{name.upper()}:',
                       f'            return &s_nx_flash_maintenance_{name};']
    source += ['        default:', '            return NULL;', '    }', '}']
    return header, source


def _block_async_factory(result):
    """One optional finite face uses the same exact card session and ID."""
    instances = [item for item in result.controllers if item.kind == "block"]
    if not instances:
        return [], []
    header = ['#include "nexus/io/block_async.h"',
        r'/** \brief Pure finite transport lookup on the same block owner. */',
        'const nx_block_async_port_t* nx_factory_block_async(nx_block_id_t id);']
    source = [r'/** \brief Return the selected finite face without effects. */',
        'const nx_block_async_port_t* nx_factory_block_async(nx_block_id_t id) {',
        '    switch (id) {']
    for item in instances:
        if (item.mode in {"sdio-irq", "sdio-dma"} and
                item.options.protocol != "sdio"):
            name = symbol(item.id)
            source += [f'        case NX_BLOCK_ID_{name.upper()}:',
                       f'            return &s_nx_block_async_{name};']
    source += ['        default:', '            return NULL;', '    }', '}']
    return header, source


def _sdio_io_factory(result):
    """I/O functions share a physical host ID, never a runtime registry."""
    instances = [item for item in result.controllers if item.kind == "block"]
    if not instances:
        return [], []
    header = ['#include "nexus/io/sdio_io.h"',
        r'/** \brief Pure optional I/O-function lookup on the same host. */',
        'const nx_sdio_io_port_t* nx_factory_sdio_io(nx_block_id_t id);']
    source = [r'/** \brief Return an existing I/O face without discovering a card. */',
        'const nx_sdio_io_port_t* nx_factory_sdio_io(nx_block_id_t id) {',
        '    switch (id) {']
    for item in instances:
        if item.options.protocol == "sdio":
            name = symbol(item.id)
            source += [f'        case NX_BLOCK_ID_{name.upper()}:',
                       f'            return &s_nx_sdio_io_{name};']
    source += ['        default:', '            return NULL;', '    }', '}']
    return header, source


def _uart_validator_factory(result):
    """Expose an optional exact transport callback without a runtime registry."""
    instances = _uart_instances(result)
    if not instances:
        return [], []
    header = [
        '/**',
        ' * \\brief           Pure lookup of reviewed TX admission.',
        ' * \\param[in]       id: Same exact ID used to obtain the UART face.',
        ' * \\return          Shared callback, or NULL for unsupported mode/ID.',
        ' * \\note            Task/ISR lookup has no hardware, allocation or',
        ' *                  lifecycle effect. Execute the callback only with',
        ' *                  that transport context under its exclusive owner',
        ' *                  and provider context contract. It neither admits',
        ' *                  nor retains request/payload storage. Symbols can',
        ' *                  be shared; same-ID/context pairing is a caller',
        ' *                  precondition, not runtime authentication.',
        ' */',
        'nx_uart_tx_validate_fn nx_factory_uart_tx_validator(nx_uart_id_t id);']
    selected = []
    for item in instances:
        provider = for_selection(item)
        implementation = provider.mode_implementation(item.kind, item.mode)
        callback = getattr(provider, "UART_TX_VALIDATORS", {}).get(
            (item.provider, item.mode, implementation))
        if callback is not None:
            if not isinstance(callback, Symbol):
                raise ValueError("Reviewed UART validator requires a typed C symbol")
            selected.append((item, callback))
    source = []
    for name in sorted({callback.name for _, callback in selected}):
        source += [f'extern nx_result_t {name}(void* context,',
                   '    const nx_uart_tx_request_t* request);']
    source += [
        '/** \\brief Exact admission symbol; no per-instance forwarding layer. */',
        'nx_uart_tx_validate_fn nx_factory_uart_tx_validator(nx_uart_id_t id) {',
        '    switch (id) {']
    for item, callback in selected:
        source += ['        case NX_UART_ID_' + symbol(item.id).upper() + ':',
                   '            return ' + callback.render() + ';']
    source += ['        default:', '            return NULL;', '    }', '}']
    return header, source


@dataclass(frozen=True)
class FactoryAccessor:
    """Selected source-level access, derived from the static factory classes."""

    id: str
    class_name: str
    shape: str
    include: str
    reference: str
    getter: str
    return_type: str


def accessors(result):
    """Project selected nominal getters without another identity authority."""
    return tuple(
        FactoryAccessor(
            id=item.id, class_name=name, shape=shape,
            include="nexus_factory_strong.h",
            reference="NX_" + name.upper() + "_REF_" + symbol(item.id).upper(),
            getter="nx_factory_" + name + "_ref",
            return_type="const " + interface_type(name, shape) + "*",
        )
        for name, shape, instances in classes(result) for item in instances
    )


def emit(result):
    result = _typed(result)
    header = ['/**', ' * \\file            nexus_factory.h',
              ' * \\brief           Typed lookup of statically selected instances.',
              ' * \\author          Nexus Team', ' */',
              '#ifndef NEXUS_FACTORY_H', '#define NEXUS_FACTORY_H',
              '#include <stddef.h>', '#include <stdint.h>',
              '#include "nexus/io/fwd.h"']
    if uart_formats.probed(result):
        header.append('#include "nexus_config.h"')
    selected_headers = sorted({public_header(name)
                               for name, _, instances in classes(result)
                               if instances})
    for name in selected_headers:
        header.append(f'#include "nexus/io/{name}.h"')
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
    validator_header, validator_source = _uart_validator_factory(result)
    header.extend(validator_header)
    source.extend(validator_source)
    if result.factory.capabilities:
        cap_header, cap_source = emit_capabilities(result)
        header.extend(cap_header)
        source.extend(cap_source)
        traits_header, traits_source = io_traits.emit_controllers(result.controllers)
        header.extend(traits_header)
        source.extend(traits_source)
        endpoint_header, endpoint_source = enhanced_endpoints.emit_traits(result)
        header.extend(endpoint_header)
        source.extend(endpoint_source)
        usb_header, usb_source = usb_profiles.emit(result)
        header.extend(usb_header)
        source.extend(usb_source)
    otg_header, otg_source = usb_otg.emit_factory(result)
    header.extend(otg_header)
    source.extend(otg_source)
    activation_header, activation_source = usb_profiles.emit_activation(result)
    header.extend(activation_header)
    source.extend(activation_source)
    nor=[item for item in result.devices if item.driver=="flash_async-endpoint"]
    if nor:
        header += [r'/** \brief Same NOR endpoint ID; readonly synchronous Flash face. */',
                   'const nx_flash_port_t* nx_factory_flash_async_flash(nx_flash_async_id_t id);']
        source += [r'/** \brief Return the same NOR owner without effects. */',
                   'const nx_flash_port_t* nx_factory_flash_async_flash(nx_flash_async_id_t id) {',
                   '    switch (id) {']
        for item in nor:
            name=symbol(item.id)
            source += [f'        case NX_FLASH_ASYNC_ID_{name.upper()}:',
                       f'            return &s_nx_flash_readonly_{name};']
        source += ['        default:', '            return NULL;', '    }', '}']
    owner_header, owner_source = enhanced_endpoints.emit_owner_factory(result)
    header.extend(owner_header)
    source.extend(owner_source)
    probe_header, probe_source = uart_formats.emit_probe_factory(result)
    header.extend(probe_header)
    source.extend(probe_source)
    profile_header, profile_source = stream_profiles.emit_factory(result)
    header.extend(profile_header)
    source.extend(profile_source)
    dac_header, dac_source = _dac_tx_factory(result)
    header.extend(dac_header)
    source.extend(dac_source)
    flash_header, flash_source = _flash_maintenance_factory(result)
    header.extend(flash_header)
    source.extend(flash_source)
    block_header, block_source = _block_async_factory(result)
    header.extend(block_header)
    source.extend(block_source)
    io_header, io_source = _sdio_io_factory(result)
    header.extend(io_header)
    source.extend(io_source)
    header += ['#ifdef __cplusplus', '}', '#endif', '#endif', '']
    return "\n".join(header), "\n".join(source) + "\n"


def emit_strong(result):
    """Default nominal accessors own no storage or hardware effects."""
    header = ["/**", " * \\file            nexus_factory_strong.h",
              " * \\brief           Nominal factory identities and pure accessors.",
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
        if result.factory.capabilities:
            declaration = f"nx_factory_{class_name}_capability_ref("
            argument = f"{ref} id) {{"
            signature = ([declaration + argument]
                         if len(declaration + argument) <= 80 else
                         [declaration, "    " + argument])
            header += [
                "/** \\brief Pure selected-mode metadata; no hardware access. */",
                "static inline const nx_factory_io_capability_t*",
                *signature,
                f"    return nx_factory_{class_name}_capability(id.value);", "}"]
    if any(item.driver == "flash_async-endpoint" for item in result.devices):
        header += [
            r'/** \brief Pure readonly lookup on the same asynchronous Flash owner. */',
            'static inline const nx_flash_port_t*',
            'nx_factory_flash_async_flash_ref(nx_flash_async_ref_t id) {',
            '    return nx_factory_flash_async_flash(id.value);', '}']
    if any(item.kind == "usb_device" and usb_otg.is_managed(item.mode)
           for item in result.controllers):
        header += [r'/** \brief Pure managed owner lookup using the same USB Device ref. */',
                   'static inline const nx_usb_otg_port_t*',
                   'nx_factory_usb_otg_ref(nx_usb_device_ref_t id) {',
                   '    return nx_factory_usb_otg(id.value);', '}']
    if any(item.kind == "dac" for item in result.controllers):
        header += [
            r'/** \brief Pure optional transfer lookup using the same DAC ref. */',
            'static inline const nx_dac_tx_port_t*',
            'nx_factory_dac_tx_ref(nx_dac_ref_t id) {',
            '    return nx_factory_dac_tx(id.value);', '}']
    if any(item.kind == "flash" for item in result.controllers):
        header += [
            r'/** \brief Pure optional physical maintenance lookup. */',
            'static inline const nx_flash_maintenance_port_t*',
            'nx_factory_flash_maintenance_ref(nx_flash_ref_t id) {',
            '    return nx_factory_flash_maintenance(id.value);', '}']
    if any(item.kind == "block" for item in result.controllers):
        header += [
            r'/** \brief Pure optional finite transport lookup. */',
            'static inline const nx_block_async_port_t*',
            'nx_factory_block_async_ref(nx_block_ref_t id) {',
            '    return nx_factory_block_async(id.value);', '}']
        header += [
            r'/** \brief Pure optional I/O-function lookup on the same host. */',
            'static inline const nx_sdio_io_port_t*',
            'nx_factory_sdio_io_ref(nx_block_ref_t id) {',
            '    return nx_factory_sdio_io(id.value);', '}']
    if _uart_instances(result):
        header += [
            '/** \\brief Pure callback lookup; pair with the same UART ref. */',
            'static inline nx_uart_tx_validate_fn',
            'nx_factory_uart_tx_validator_ref(nx_uart_ref_t id) {',
            '    return nx_factory_uart_tx_validator(id.value);', '}']
    header += ["#ifdef __cplusplus", "}", "#endif", "#endif", ""]
    return "\n".join(header)


def _capability(item, model, endpoint=None, *, public_class=None):
    """Keep the existing optional ABI as a selected-contract projection."""
    return selected_contracts.capability(item, model, endpoint,
                                         public_class=public_class)


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
        ' * Zero unit_bytes means no fixed byte width; the typed face defines units.',
        ' * For I2C, units bounds each message; unit_bytes is the payload unit.',
        ' * For enhanced SPI, max_messages bounds segments, never I2C messages.',
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
                item, for_selection(result).MODEL,
                instance if shape == "endpoint" else None,
                public_class=class_name)
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
    diagnostic_header, diagnostic_source = emit_controller_metadata(result)
    header.extend(diagnostic_header)
    source.extend(diagnostic_source)
    return header, source


def emit_controller_metadata(result):
    """Opt-in cold labels and aliases of the existing capability objects."""
    ordinals = controller_ordinals(result)
    header = [
        '/** \\brief Immutable selected facts, never operational readiness. */',
        'typedef enum {',
        '    NX_FACTORY_CONTROLLER_ID, NX_FACTORY_CONTROLLER_KIND,',
        '    NX_FACTORY_CONTROLLER_HARDWARE, NX_FACTORY_CONTROLLER_MODE,',
        '    NX_FACTORY_CONTROLLER_FIELD_COUNT',
        '} nx_factory_controller_field_t;',
        '/**',
        ' * \\brief Reuse the selected controller\'s existing capability object.',
        ' * \\param[in] controller_index Resolved controller ordinal from a',
        ' * diagnostic or NX_PLATFORM_CONTROLLER_INDEX_*; not a typed factory ID.',
        ' * \\return Existing immutable capability, or NULL for an invalid ordinal.',
        ' * \\note Task/ISR safe before startup and after stop. No allocation,',
        ' * locks, hardware, clock or mutable-state access. No readiness implied.',
        ' */',
        'const nx_factory_io_capability_t* nx_factory_controller_capability(',
        '    size_t controller_index);',
        '/**',
        ' * \\brief Locate immutable assembly, kind, hardware or mode text.',
        ' * \\param[in] controller_index Same diagnostic ordinal as above.',
        ' * \\param[in] field Exact requested fact, not an execution-state query.',
        ' * \\return Static borrowed text, or NULL for an invalid ordinal/field.',
        ' * \\note Task/ISR safe; no hardware or lifecycle effects. Text and',
        ' * ordinals belong to this assembly, not a persistent or wire ABI.',
        ' */',
        'const char* nx_factory_controller_label(',
        '    size_t controller_index, nx_factory_controller_field_t field);']
    source = [
        '/** \\brief Alias existing capabilities without another metadata table. */',
        'const nx_factory_io_capability_t* nx_factory_controller_capability(',
        '    size_t controller_index) {',
        '    switch (controller_index) {']
    for index, item, _ in ordinals:
        source += [f'        case {index}U:',
                   '            return &s_nx_capability_' +
                   controller_class(item) + '_' + symbol(item.id) + ';']
    source += ['        default:', '            return NULL;', '    }', '}',
               '/** \\brief Pure cold lookup; diagnostic storage is never retained. */',
               'const char* nx_factory_controller_label(',
               '    size_t controller_index, nx_factory_controller_field_t field) {']
    if not ordinals:
        source.append('    (void)field;')
    source.append('    switch (controller_index) {')
    for index, item, _ in ordinals:
        source += [f'        case {index}U:', '            switch (field) {']
        for name, value in (("ID", item.id), ("KIND", item.kind),
                            ("HARDWARE", item.controller), ("MODE", item.mode)):
            source += [f'                case NX_FACTORY_CONTROLLER_{name}:',
                       '                    return ' + json.dumps(value) + ';']
        source += ['                default:', '                    return NULL;',
                   '            }']
    source += ['        default:', '            return NULL;', '    }', '}']
    return header, source
