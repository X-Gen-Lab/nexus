"""Nominal endpoint classes share controllers without interface casts."""
from dataclasses import replace

from tools.configure import selected_contracts

DRIVERS = selected_contracts.ENDPOINT_DRIVERS


def class_name(device):
    """Keep enhanced factory IDs separate from legacy endpoint namespaces."""
    return selected_contracts.endpoint_class(device)


def face_type(device):
    """Return the actual typed public face, never an old endpoint alias."""
    selected = class_name(device)
    return "nx_" + selected.removesuffix("_device") + "_endpoint_t"


def face(device, state, legacy_operations, prefix=None):
    """Emit one immutable enhanced or legacy face around existing state."""
    name = device.id.replace("-", "_")
    typename = face_type(device)
    operations = legacy_operations
    if device.driver == "spi-dma-transaction":
        controller = device.controller.replace("-", "_")
        context = f"s_nx_sequence_endpoint_{name}"
        return [f"static nx_spi_dma_sequence_endpoint_t {context} = {{",
                f"    &s_nx_sequence_{controller}, &{state}", "};",
                f"static const {typename} s_nx_device_face_{name} = {{",
                f"    &nx_spi_dma_sequence_endpoint_ops, &{context}", "};",
                f"const {typename}* const nx_device_{name} = "
                f"&s_nx_device_face_{name};"]
    if device.driver == "spi-transaction":
        if prefix not in {"native", "stm32", "gd32"}:
            raise ValueError("SPI transaction provider lacks an explicit table")
        operations = f"nx_{prefix}_spi_transaction_ops"
    elif device.driver == "i2c-long-endpoint":
        operations = "nx_i2c_v2_long_endpoint_ops"
    return [f"static const {typename} s_nx_device_face_{name} = {{",
            f"    &{operations}, &{state}", "};",
            f"const {typename}* const nx_device_{name} = "
            f"&s_nx_device_face_{name};"]


def dma_construction(item, construction, prefix):
    """Select sequence state only for the nominal enhanced DMA interface.

    The existing hardware constructor and IRQs keep the exact original engine
    state. Only the public sequence face owns outer loans. The private engine
    face is never exported by a factory or endpoint namespace.
    """
    if item.interface_kind != "spi_dma_transaction":
        return construction
    name = item.id.replace("-", "_")
    sequence = f"s_nx_sequence_{name}"
    backend = f"nx_{prefix}_spi_dma_sequence_backend"
    definitions = [line.replace(f"prepare_{name}(", f"prepare_engine_{name}(")
                   .replace(f"nx_spi_port_stop(nx_binding_{name})",
                            f"nx_spi_dma_transaction_stop(nx_binding_{name})")
                   .replace(f"nx_spi_port_stop(&s_nx_face_{name})",
                            f"nx_spi_dma_transaction_stop(&s_nx_face_{name})")
                   for line in construction.definitions]
    definitions += [f"static nx_result_t prepare_{name}(void) {{",
                    f"    if ({sequence}.active != NULL || {sequence}.cs_held ||",
                    f"        {sequence}.pending_release) {{ return NX_ERROR_BUSY; }}",
                    f"    nx_result_t status = prepare_engine_{name}();",
                    "    if (status != NX_SUCCESS) { return status; }",
                    f"    return nx_spi_dma_sequence_initialize(&{sequence},",
                    f"        &{backend}, &s_nx_port_{name});", "}"]
    return replace(construction, definitions=tuple(definitions),
                   face_context="&" + sequence,
                   face_definitions=(
                       f"static nx_spi_dma_sequence_state_t {sequence} = {{",
                       f"    .backend = &{backend}, .context = &s_nx_port_{name}",
                       "};"), retain_on_failed_init=True)


def emit_traits(result):
    """Optional selected-only endpoint facts use their actual data units."""
    header, source = [], []
    controllers = {item.id: item for item in result.controllers}
    for driver, (_, namespace) in sorted(DRIVERS.items()):
        instances = [item for item in result.devices if item.driver == driver]
        if not instances:
            continue
        typename = f"nx_factory_{namespace}_traits_t"
        word_based = driver != "i2c-long-endpoint"
        fields = (("supported_word_bits", "max_total_words",
                   "max_segment_words", "max_segments") if word_based else
                  ("address_bits", "max_message_bytes", "max_messages"))
        header += [
            '/**',
            ' * \\brief Exact enhanced-interface bounds, without physical proof.',
            ' * \\note Zero bounds are unspecified, never unlimited. SPI counts',
            ' * are words, independent of 8/16-bit spans. supported_word_bits',
            ' * uses bit N for N-bit words; a request chooses one width.',
            ' * I2C bounds each message. These do not replace provider checks.',
            ' */', 'typedef struct {',
            *[f"    uint32_t {name};" for name in fields],
            f'}} {typename};',
            '/** \\brief Pure selected-only lookup, NULL for an invalid ID. */',
            f'const {typename}* nx_factory_{namespace}_traits(',
            f'    nx_{namespace}_id_t id);']
        for item in instances:
            name = item.id.replace("-", "_")
            contract = selected_contracts.describe(
                controllers[item.controller], item)
            if driver == "i2c-long-endpoint":
                values = (contract.fields["address_bits"],
                          contract.max_transfer_units,
                          contract.max_messages)
            else:
                widths = [f"UINT32_C(1) << {bits}U"
                          for bits in contract.word_bits]
                width = (widths[0] if len(widths) == 1 else
                         " | ".join(f"({value})" for value in widths))
                values = (width, contract.max_total_units,
                          contract.max_segment_units, contract.max_segments)
            values = tuple(f"{value}U" if type(value) is int else
                           "0U" if value is None else value for value in values)
            source += [f"static const {typename}",
                       f"    s_nx_traits_{namespace}_{name} = {{",
                       *[f"    {value}," for value in values], "};"]
        source += ['/** \\brief Pure enhanced-interface traits lookup. */',
                   f'const {typename}* nx_factory_{namespace}_traits(',
                   f'    nx_{namespace}_id_t id) {{', '    switch (id) {']
        for item in instances:
            name = item.id.replace("-", "_")
            source += [f'        case NX_{namespace.upper()}_ID_{name.upper()}:',
                       f'            return &s_nx_traits_{namespace}_{name};']
        source += ['        default:', '            return NULL;', '    }', '}']
    return header, source


def emit_owner_factory(result):
    """Bind each enhanced owner to its exact provider membership proof."""
    selected = [item for item in result.controllers
                if item.interface_kind == "spi_dma_transaction"]
    if not selected:
        return [], []
    prefixes = {"stm32f407": "stm32", "gd32f470": "gd32", "native": "native"}
    header = ['#include "nexus/io/spi_dma_owner_binding.h"',
              '/** \\brief Pure lookup of the selected physical/model owner. */',
              'const nx_spi_dma_owner_binding_t* nx_factory_spi_dma_owner_binding(',
              '    nx_spi_dma_transaction_id_t id);']
    source = []
    for item in selected:
        name = item.id.replace("-", "_")
        prefix = prefixes[item.provider]
        source += [f'static const nx_spi_dma_owner_binding_t s_nx_spi_owner_{name} = {{',
                   f'    .port = &s_nx_face_{name},',
                   f'    .contains = nx_{prefix}_spi_dma_owner_contains,',
                   f'    .attach_wake = nx_{prefix}_spi_dma_owner_attach_wake',
                   '};']
    source += ['const nx_spi_dma_owner_binding_t* nx_factory_spi_dma_owner_binding(',
               '    nx_spi_dma_transaction_id_t id) {', '    switch (id) {']
    for item in selected:
        name = item.id.replace("-", "_")
        source += [f'        case NX_SPI_DMA_TRANSACTION_ID_{name.upper()}:',
                   f'            return &s_nx_spi_owner_{name};']
    return header, source + ['        default:', '            return NULL;',
                             '    }', '}']
